#ifndef CR_CONTACT_SHADOW_H
#define CR_CONTACT_SHADOW_H
#include <math.h>
#include <stdint.h>
#include <string.h>

/* Contact strips clipped to actual lower top faces. A crease can be a solid
 * intersection, so compare top heights and use thickness only for falloff. No screen-space sampling,
 * depth texture or added route tessellation. Scratch storage is shared by the
 * single render thread; output is cached with each extruded route mesh. */
#define CR_CONTACT_MAX_SOURCE_VERTS 7200
#define CR_CONTACT_MAX_VERTS 1536
#define CR_CONTACT_HASH_SIZE 16384
#define CR_CONTACT_RADIUS 0.025f
#define CR_CONTACT_MIN_GAP 0.00001f
#define CR_CONTACT_MAX_GAP 0.140f

typedef struct { int a,b,uses; } cr_contact_edge_t;
typedef struct { float v[7]; } cr_contact_point_t; /* xyz, across/radius, along, gap, edge clearance */

static inline uint32_t cr_contact_hash_point(const float *p) {
    uint32_t h=2166136261u,bits;
    int i;
    for(i=0;i<3;i++) {
        float value=p[i]==0 ? 0 : p[i]; /* normalize signed zero */
        memcpy(&bits,&value,sizeof(bits));h=(h^bits)*16777619u;
    }
    return h;
}
static inline int cr_contact_same(const float *a,const float *b) {
    return a[0]==b[0] && a[1]==b[1] && a[2]==b[2];
}
static inline int cr_contact_clip(cr_contact_point_t *dst,const cr_contact_point_t *src,
                                  int n,int axis,float bound,float sign) {
    int i,j,count=0;
    for(i=0;i<n;i++) {
        const cr_contact_point_t *a=&src[i],*b=&src[(i+1)%n];
        float da=sign*(a->v[axis]-bound),db=sign*(b->v[axis]-bound);
        if((da>=0)!=(db>=0)) {
            float t=da/(da-db);
            if(count>=12)return 0;
            for(j=0;j<7;j++)dst[count].v[j]=a->v[j]+t*(b->v[j]-a->v[j]);
            count++;
        }
        if(db>=0) { if(count>=12)return 0;dst[count++]=*b; }
    }
    return count;
}

/* Returns vertices written, or -1 if the bounded output cannot hold the result.
 * Inputs use the existing position+normal (6 floats) vertex layout. */
static inline int cr_contact_build(const float *verts,int count,float thickness,
                                   float *output,int capacity) {
    static cr_contact_edge_t edges[CR_CONTACT_MAX_SOURCE_VERTS];
    static int table[CR_CONTACT_HASH_SIZE],faces[CR_CONTACT_MAX_SOURCE_VERTS/3];
    int i,j,edge_count=0,face_count=0,written=0;
    float max_clearance=0;
    if(count<3 || count>CR_CONTACT_MAX_SOURCE_VERTS || !(thickness>0))return 0;
    float min_top=1e9f,max_top=-1e9f;
    for(i=0;i+2<count;i+=3) {
        if(verts[i*6+4]<0.99f || verts[(i+1)*6+4]<0.99f || verts[(i+2)*6+4]<0.99f)continue;
        faces[face_count++]=i;
        for(j=0;j<3;j++) {
            min_top=fminf(min_top,verts[(i+j)*6+1]);max_top=fmaxf(max_top,verts[(i+j)*6+1]);
        }
    }
    /* Most routes/tail slices are coplanar: no hashing or pairwise clipping. */
    if(max_top-min_top<=CR_CONTACT_MIN_GAP)return 0;
    memset(table,0,sizeof(table));
    for(i=0;i<face_count;i++) {
        for(j=0;j<3;j++) {
            int a=faces[i]+j,b=faces[i]+(j+1)%3;
            uint32_t ha=cr_contact_hash_point(verts+a*6),hb=cr_contact_hash_point(verts+b*6);
            unsigned slot=((ha+hb)^((ha^hb)*16777619u))&(CR_CONTACT_HASH_SIZE-1);
            while(table[slot]) {
                cr_contact_edge_t *e=&edges[table[slot]-1];
                if((cr_contact_same(verts+a*6,verts+e->a*6) && cr_contact_same(verts+b*6,verts+e->b*6)) ||
                   (cr_contact_same(verts+a*6,verts+e->b*6) && cr_contact_same(verts+b*6,verts+e->a*6)))break;
                slot=(slot+1)&(CR_CONTACT_HASH_SIZE-1);
            }
            if(table[slot])edges[table[slot]-1].uses++;
            else { edges[edge_count]=(cr_contact_edge_t){a,b,1};table[slot]=++edge_count; }
        }
    }
    for(i=0;i<edge_count;i++) {
        const cr_contact_edge_t *edge=&edges[i];
        if(edge->uses!=1)continue; /* discard internal top-face triangulation */
        const float *a=verts+edge->a*6,*b=verts+edge->b*6;
        float dx=b[0]-a[0],dz=b[2]-a[2],length=sqrtf(dx*dx+dz*dz);
        if(length<1e-6f)continue;
        dx/=length;dz/=length;
        float min_x=fminf(a[0],b[0])-CR_CONTACT_RADIUS,max_x=fmaxf(a[0],b[0])+CR_CONTACT_RADIUS;
        float min_z=fminf(a[2],b[2])-CR_CONTACT_RADIUS,max_z=fmaxf(a[2],b[2])+CR_CONTACT_RADIUS;
        for(j=0;j<face_count;j++) {
            const float *v=verts+faces[j]*6;
            if(fminf(v[0],fminf(v[6],v[12]))>max_x || fmaxf(v[0],fmaxf(v[6],v[12]))<min_x ||
               fminf(v[2],fminf(v[8],v[14]))>max_z || fmaxf(v[2],fmaxf(v[8],v[14]))<min_z)continue;
            /* Reject mere proximity to a neighbouring face. The caster edge
             * itself must cross the receiving triangle with positive clearance.
             * This also rejects continuous slopes without an arbitrary path gap. */
            float ux=v[6]-v[0],uz=v[8]-v[2],vx=v[12]-v[0],vz=v[14]-v[2];
            float det=ux*vz-uz*vx;
            if(fabsf(det)<1e-10f)continue;
            float hx=((v[7]-v[1])*vz-(v[13]-v[1])*uz)/det;
            float hz=(ux*(v[13]-v[1])-vx*(v[7]-v[1]))/det;
            float ga=a[1]-(v[1]+hx*(a[0]-v[0])+hz*(a[2]-v[2]));
            float gb=b[1]-(v[1]+hx*(b[0]-v[0])+hz*(b[2]-v[2]));
            float lo=0,hi=1;
            int side;
            for(side=0;side<3 && lo<hi;side++) {
                const float *q=v+6*side,*r=v+6*((side+1)%3);
                float sa=((r[0]-q[0])*(a[2]-q[2])-(r[2]-q[2])*(a[0]-q[0]))/det;
                float sb=((r[0]-q[0])*(b[2]-q[2])-(r[2]-q[2])*(b[0]-q[0]))/det;
                if(sa<0 && sb<0) { hi=lo;break; }
                if((sa<0)!=(sb<0)) {
                    float t=sa/(sa-sb);
                    if(sa<0)lo=fmaxf(lo,t);else hi=fminf(hi,t);
                }
            }
            if(hi-lo<1e-6f || fmaxf(ga+(gb-ga)*lo,ga+(gb-ga)*hi)<=CR_CONTACT_MIN_GAP)continue;
            cr_contact_point_t poly[2][12];
            int k,n=3,src=0;
            float max_gap=-1e9f,min_gap=1e9f;
            for(k=0;k<3;k++) {
                const float *p=v+k*6;float along=(p[0]-a[0])*dx+(p[2]-a[2])*dz;
                float gap=a[1]+(b[1]-a[1])*along/length-p[1];
                float clearance=ga+(gb-ga)*along/length;
                poly[0][k]=(cr_contact_point_t){{p[0],p[1],p[2],
                    (-(p[0]-a[0])*dz+(p[2]-a[2])*dx)/CR_CONTACT_RADIUS,along,gap,clearance}};
                max_gap=fmaxf(max_gap,gap);min_gap=fminf(min_gap,gap);
            }
            if(max_gap<=CR_CONTACT_MIN_GAP || min_gap>=thickness+CR_CONTACT_MAX_GAP)continue;
            const int axes[6]={3,3,4,4,5,5};
            const float bounds[6]={-1,1,0,length,CR_CONTACT_MIN_GAP,thickness+CR_CONTACT_MAX_GAP};
            for(k=0;k<6 && n;k++) {
                n=cr_contact_clip(poly[1-src],poly[src],n,axes[k],bounds[k],k%2?-1:1);src=1-src;
            }
            {
                int result=1-src;
                int filtered=cr_contact_clip(poly[result],poly[src],n,6,CR_CONTACT_MIN_GAP,1);
                for(k=1;k+1<filtered;k++) {
                    const cr_contact_point_t *tri[3]={&poly[result][0],&poly[result][k],&poly[result][k+1]};
                    float area=(tri[1]->v[0]-tri[0]->v[0])*(tri[2]->v[2]-tri[0]->v[2])-
                               (tri[1]->v[2]-tri[0]->v[2])*(tri[2]->v[0]-tri[0]->v[0]);
                    int t;
                    if(fabsf(area)<1e-10f)continue;
                    if(written+3>capacity)return -1;
                    for(t=0;t<3;t++) {
                        float *out=output+6*written++;
                        memcpy(out,tri[t]->v,3*sizeof(float));
                        out[3]=tri[t]->v[3];out[4]=tri[t]->v[5];
                        /* Clearance grows continuously from the true crease.
                         * Normalize across the mesh, never per triangle/edge,
                         * so the width taper cannot restart at tessellation seams. */
                        out[5]=tri[t]->v[6];
                        max_clearance=fmaxf(max_clearance,out[5]);
                    }
                }
            }
        }
    }
    if(written && max_clearance>0) {
        float scale=1.0f/max_clearance;
        for(i=0;i<written;i++)output[i*6+5]*=scale;
    }
    return written;
}
#endif
