/* GPL-3.0-or-later. Edit only children.gal.exec/path, preserving other bytes.
 * Accepts the stock configuration's # comments. Refuses duplicate target keys,
 * escaped launch paths, unexpected types, and unbounded input. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
static char buf[131073];
static size_t len;
typedef struct { size_t a,b; } span;
static void bad(const char *s) { fprintf(stderr,"config_tool: %s\n",s); exit(2); }
static size_t space(size_t p)
{
    for(;;) {
        while(p<len && isspace((unsigned char)buf[p])) ++p;
        if(p<len && buf[p]=='#') { while(p<len && buf[p]!='\n') ++p; }
        else return p;
    }
}
static size_t string_end(size_t p)
{
    if(p>=len || buf[p]!='"') bad("expected string");
    for(++p;p<len;++p) {
        if(buf[p]=='\\') { if(++p>=len) break; }
        else if(buf[p]=='"') return p+1;
        else if((unsigned char)buf[p]<32) bad("control character in string");
    }
    bad("unterminated string"); return 0;
}
static size_t value_end(size_t p,unsigned depth)
{
    char close;
    if(depth>32 || p>=len) bad("invalid nesting or missing value");
    if(buf[p]=='"') return string_end(p);
    if(buf[p]!='{' && buf[p]!='[') {
        size_t start=p;
        while(p<len && !isspace((unsigned char)buf[p]) && buf[p]!=',' && buf[p]!='}' && buf[p]!=']' && buf[p]!='#') ++p;
        if(p==start) bad("empty value");
        return p;
    }
    close=buf[p]=='{'?'}':']'; ++p;
    for(;;) {
        p=space(p);
        if(p>=len) bad("unclosed object");
        if(buf[p]==close) return p+1;
        if(buf[p]==',' || buf[p]==':') { ++p; continue; }
        p=value_end(p,depth+1);
    }
}
static span member(span object,const char *name)
{
    span found={0,0};
    size_t p=space(object.a), k, e, v;
    if(buf[p]!='{') bad("expected object");
    ++p;
    for(;;) {
        p=space(p); if(p>=object.b) bad("object bounds");
        if(buf[p]=='}') break;
        k=p; e=string_end(k); p=space(e);
        if(buf[p++]!=':') bad("expected colon");
        v=space(p); p=value_end(v,0);
        if(e-k==strlen(name)+2 && !memcmp(buf+k+1,name,strlen(name))) {
            if(found.b) bad("duplicate target key");
            found.a=v; found.b=p;
        }
        p=space(p);
        if(buf[p]==',') ++p;
        else if(buf[p]!='}') bad("expected comma");
    }
    if(!found.b) bad("missing target key");
    return found;
}
static void valid_path(span s,int directory)
{
    size_t i;
    if(buf[s.a]!='"' || s.b-s.a<3 || s.b-s.a>256) bad("invalid launch string");
    for(i=s.a+1;i<s.b-1;++i)
        if(!isalnum((unsigned char)buf[i]) && !strchr("_./-",buf[i])) bad("unsafe launch characters");
    if(directory && buf[s.a+1]!='/') bad("launch directory must be absolute");
    if(!directory && memchr(buf+s.a+1,'/',s.b-s.a-2)) bad("exec must be a filename");
}
int main(int argc,char **argv)
{
    FILE *f;
    span root,children,gal,ex,pa;
    const char *newexec="\"probe_startup.sh\"";
    const char *newpath="\"/mnt/app/root/sq5_cluster_probe\"";
    if(argc<3 || argc>4) bad("usage: config_tool patch|get CONFIG [exec|path]");
    f=fopen(argv[2],"rb"); if(!f) bad("cannot open config");
    len=fread(buf,1,sizeof(buf)-1,f);
    if(ferror(f) || !feof(f)) bad("config too large or read error");
    fclose(f); buf[len]=0;
    root.a=space(0); root.b=value_end(root.a,0);
    if(space(root.b)!=len) bad("trailing data");
    children=member(root,"children"); gal=member(children,"gal");
    ex=member(gal,"exec"); pa=member(gal,"path"); valid_path(ex,0); valid_path(pa,1);
    if(!strcmp(argv[1],"get") && argc==4) {
        span s;
        if(!strcmp(argv[3],"exec")) s=ex;
        else if(!strcmp(argv[3],"path")) s=pa;
        else bad("unknown get field");
        fwrite(buf+s.a+1,1,s.b-s.a-2,stdout); putchar('\n');
    } else if(!strcmp(argv[1],"patch") && argc==3) {
        span first=ex,second=pa;
        const char *a=newexec,*b=newpath;
        if(pa.a<ex.a) { first=pa; second=ex; a=newpath; b=newexec; }
        fwrite(buf,1,first.a,stdout); fputs(a,stdout);
        fwrite(buf+first.b,1,second.a-first.b,stdout); fputs(b,stdout);
        fwrite(buf+second.b,1,len-second.b,stdout);
    } else bad("unknown operation");
    if(fflush(stdout)!=0 || ferror(stdout)) bad("output failed");
    return 0;
}
