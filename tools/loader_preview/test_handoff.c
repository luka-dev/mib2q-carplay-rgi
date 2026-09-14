#include "../../common/prism.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    /* Same motion at 30, 60, irregular and single-step cadence. */
    handoff_t a={0,0,1},b=a,c=a;
    handoff_step(&a,0.8);
    for(int i=0;i<24;i++) handoff_step(&b,1.0/30.0);
    for(int i=0;i<48;i++) handoff_step(&c,1.0/60.0);
    assert(fabs(a.position-b.position)<1e-12);
    assert(fabs(a.velocity-c.velocity)<1e-12);
    /* Retarget mid-flight without discarding position or velocity. */
    a=(handoff_t){0,0,1}; handoff_step(&a,0.22);
    double p=a.position,v=a.velocity;
    a.target=0; handoff_step(&a,0);
    assert(a.position==p && a.velocity==v);
    handoff_step(&a,0.00001);
    assert(fabs(a.position-p)<0.0001 && fabs(a.velocity-v)<0.001);
    for(int i=0;i<150;i++) {
        if(i%7==0) a.target=!a.target;
        handoff_step(&a,1.0/30.0);
        assert(isfinite(a.position) && isfinite(a.velocity));
        assert(a.position>=0 && a.position<=1);
    }
    a.target=0; handoff_step(&a,10);
    assert(a.position==0 && a.velocity==0);
    a.target=1; handoff_step(&a,10);
    assert(a.position==1 && a.velocity==0);
    assert(handoff_demo(3.4).position==0);
    assert(handoff_demo(6.8).position==1);
    assert(handoff_demo(10.0).position==0);
    for(int view=0;view<2;view++) for(int style=0;style<2;style++) {
        prism_layout_t l=prism_layout(view,style,122,18);
        assert(l.signature[0]>=l.content[0] && l.signature[0]+122<=l.content[0]+l.content[2]);
        assert(l.signature[1]>=l.content[1] && l.signature[1]+18<=l.content[1]+l.content[3]);
    }
    puts("Handoff: cadence independent, reversible, bounded, settles at both endpoints.");
}
