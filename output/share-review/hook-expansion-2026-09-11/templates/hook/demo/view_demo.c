/* Copyright (c) 2026, original project author. */
/* Synthetic dimensions and locally simulated completions. No transport. */
#include "view_area.h"
#include "view_switch.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    view_area wide, compact;
    view_switch state;
    view_request first, second;
    char json[512];
    int length;
    assert(view_area_from_insets(&wide,800,480,40,24,40,24)==0);
    assert(view_area_from_insets(&compact,800,480,200,24,200,24)==0);
    length=view_area_write_json(&compact,json,sizeof(json));
    assert(length>0 && (size_t)length<sizeof(json));
    puts(json);
    assert(view_switch_init(&state,2,0)==0 && view_switch_begin(&state)==0);
    assert(view_switch_next(&state,&first)==1);
    assert(view_switch_select(&state,1)==0);
    assert(view_switch_next(&state,&second)==0);
    assert(view_switch_complete(&state,first,1)==1);
    assert(state.applied==0 && state.desired==1);
    assert(view_switch_next(&state,&second)==1 && second.area==1);
    assert(view_switch_complete(&state,second,1)==1);
    view_switch_end(&state);
    assert(view_switch_begin(&state)==0 && view_switch_complete(&state,second,1)==0);
    assert(view_switch_next(&state,&first)==1 && first.area==1);
    assert(view_switch_complete(&state,first,1)==1);
    puts("View demo: desired/applied separation, coalescing and stale-session ACK rejection passed.");
    return 0;
}
