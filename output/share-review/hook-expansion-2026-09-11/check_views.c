/* PRIVATE boundary checks; the public demo remains small and readable. */
#include "view_area.h"
#include "view_switch.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    view_area a, saved;
    char text[512], tiny[2];
    assert(view_area_init(&a,(view_rect){100,200,20,30},(view_rect){100,200,20,30})==0);
    saved=a;
    assert(view_area_init(&a,(view_rect){100,200,20,30},(view_rect){0,0,10,10})==-1);
    assert(memcmp(&a,&saved,sizeof(a))==0);
    assert(view_area_from_insets(&a,80,60,40,0,40,0)==-1);
    assert(view_area_from_insets(&a,80,60,-1,0,0,0)==-1);
    assert(memcmp(&a,&saved,sizeof(a))==0);
    assert(view_area_from_insets(&a,INT32_MAX,INT32_MAX,INT32_MAX,0,INT32_MAX,0)==-1);
    assert(view_area_init(&a,(view_rect){INT32_MAX,0,INT32_MAX,2},
                         (view_rect){INT32_MAX,0,INT32_MAX,2})==0);
    assert(view_area_valid(&a));
    assert(view_area_from_insets(&a,800,480,200,24,200,24)==0);
    int needed=view_area_write_json(&a,text,sizeof(text));
    assert(needed>0 && (size_t)needed==strlen(text));
    assert(view_area_write_json(&a,NULL,0)==needed);
    assert(view_area_write_json(&a,tiny,sizeof(tiny))==needed && tiny[1]=='\0');
    assert(view_area_write_json(&a,NULL,1)==-1);
    a.safe.width=0; text[0]='x';
    assert(view_area_write_json(&a,text,sizeof(text))==-1 && text[0]=='\0');

    view_switch s;
    view_request first, next, bad;
    assert(view_switch_init(&s,3,0)==0);
    assert(view_switch_select(&s,3)==-1 && s.desired==0);
    assert(view_switch_select(&s,2)==0 && view_switch_next(&s,&first)==0);
    assert(view_switch_begin(&s)==0 && view_switch_next(&s,&first)==1);
    assert(first.area==2 && s.applied==VIEW_AREA_UNKNOWN);
    bad=first; ++bad.serial;
    assert(view_switch_complete(&s,bad,1)==0 && s.pending);
    bad=first; bad.area=0;
    assert(view_switch_complete(&s,bad,1)==0 && s.pending);
    assert(view_switch_select(&s,1)==0 && view_switch_select(&s,0)==0);
    assert(view_switch_next(&s,&next)==0);
    assert(view_switch_complete(&s,first,1)==1 && s.applied==2 && s.desired==0);
    assert(view_switch_next(&s,&next)==1 && next.area==0);
    assert(view_switch_complete(&s,first,1)==0 && s.pending);
    assert(view_switch_complete(&s,next,0)==1 && s.applied==VIEW_AREA_UNKNOWN);
    assert(view_switch_next(&s,&first)==1 && first.serial!=next.serial);
    assert(view_switch_complete(&s,next,1)==0);
    view_switch_end(&s);
    assert(view_switch_complete(&s,first,1)==0);
    assert(view_switch_begin(&s)==0 && view_switch_next(&s,&next)==1);
    assert(view_switch_complete(&s,first,1)==0 && s.pending);
    assert(view_switch_complete(&s,next,1)==1 && view_switch_next(&s,&first)==0);
    view_switch_end(&s); s.generation=UINT64_MAX;
    assert(view_switch_begin(&s)==-1 && !s.active);
    s.generation=1; assert(view_switch_begin(&s)==0); s.serial=UINT64_MAX;
    assert(view_switch_next(&s,&first)==-1 && !s.pending);
    puts("view boundaries passed: geometry/overflow, JSON sizing, coalescing, failure, duplicate/stale ACKs, counter exhaustion");
    return 0;
}
