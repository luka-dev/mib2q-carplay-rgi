#include "../../maneuver_render/scene/lane_model.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    lr_state s; int count, mask, i, cap, combinations, checked=0;
    assert(!lr_valid_angle(1000) && !lr_valid_angle(-1000));
    assert(!lr_valid_angle(181) && !lr_valid_angle(-181));
    {
        float m[16]={0},x,z;m[0]=m[5]=m[9]=m[15]=1;
        assert(lr_project_ground(m,.2f,.095f,.3f,&x,&z));
        assert(fabsf(x-.2f)<1e-6f && fabsf(z-.395f)<1e-6f);
        assert(lr_project_ground(m,.2f,0,.3f,&x,&z) && fabsf(z-.3f)<1e-6f);
        m[15]=0;assert(!lr_project_ground(m,0,0,0,&x,&z));
    }
    for(count=1;count<=8;++count) {
        combinations=1;for(i=0;i<count;++i)combinations*=3;
        for(mask=0;mask<combinations;++mask) {
            int m=mask, top=0;
            memset(&s,0,sizeof(s));s.count=count;
            for(i=0;i<count;++i) {s.lane[i].position=i;s.lane[i].status=m%3;m/=3;if(s.lane[i].status>top)top=s.lane[i].status;}
            for(cap=1;cap<=8;++cap) {
                lr_window w=lr_choose(&s,cap,-1);
                assert(w.first>=0 && w.first+w.count<=count);
                assert(w.count>0 && w.count<=count);
                assert(!(w.hidden_left && w.hidden_right));
                assert(w.hidden_left+w.count+w.hidden_right==count);
                if(top) {assert(w.anchor>=w.first && w.anchor<w.first+w.count);assert(s.lane[w.anchor].status==top);}
                else assert(w.anchor==-1);
                if(w.anchor>=0 && w.anchor<=count-1-w.anchor)assert(w.first==0);
                if(w.anchor>=0 && w.anchor>count-1-w.anchor)assert(w.first+w.count==count);
                for(i=0;i<count;++i) if(s.lane[i].status>0) {
                    if(i<w.first)assert(w.recommended_left);
                    if(i>=w.first+w.count)assert(w.recommended_right);
                }
                ++checked;
            }
        }
    }
    memset(&s,0,sizeof(s));s.count=3;
    for(i=0;i<3;++i) {s.lane[i].position=2-i;s.lane[i].status=2;s.lane[i].primary=1000;}
    s.lane[1].primary=-1000;s.lane[2].primary=45;
    s.lane[0].count=5; s.lane[0].angles[0]=-1000;s.lane[0].angles[1]=1000;s.lane[0].angles[2]=0;s.lane[0].angles[3]=1;s.lane[0].angles[4]=45;
    lr_normalize(&s);assert(s.lane[0].position==0 && s.lane[2].count==2);
    assert(lr_choose(&s,2,-1).anchor==0);
    s.lane[0].primary=1000;assert(lr_choose(&s,2,-1).anchor==-1);
    s.lane[0].primary=s.lane[1].primary=0;
    assert(lr_choose(&s,3,1).anchor==1);
    {
        int first,last;
        memset(&s,0,sizeof(s));s.count=5;
        for(i=0;i<5;++i){s.lane[i].position=i;s.lane[i].primary=i<3?-90:0;}
        assert(lr_exit_group(&s,1,&first,&last)==3 && first==0 && last==2);
        s.lane[1].primary=0;s.lane[1].count=1;s.lane[1].angles[0]=-90;
        assert(lr_exit_group(&s,0,&first,&last)==3); /* optional turn still a real direction */
        s.lane[1].angles[0]=1000;
        assert(lr_exit_group(&s,0,&first,&last)==1); /* never bridge unknown/straight slot */
        assert(lr_exit_group(&s,2,&first,&last)==1);
        assert(lr_exit_group(&s,3,&first,&last)==0);
        s.lane[1].primary=-90;s.lane[1].position=7;
        assert(lr_exit_group(&s,0,&first,&last)==1); /* missing positions are not adjacency */
        s.lane[0].primary=-1000;
        assert(lr_exit_group(&s,0,&first,&last)==0);
    }
    memset(&s,0,sizeof(s));s.count=9;s.complete=1;
    lr_normalize(&s);assert(s.count==8 && !s.complete);
    memset(&s,0,sizeof(s));s.count=1;s.complete=1;s.lane[0].count=17;
    lr_normalize(&s);assert(!s.complete);
    puts("lane layout: sentinels, unknown slots, ordering, continuity, real edge, no two hidden sides and truncation invalidation PASS");
    printf("%d window/status combinations\n",checked);
    return 0;
}
