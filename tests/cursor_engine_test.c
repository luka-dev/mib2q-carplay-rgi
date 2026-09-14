#include "../cursor_render/engine.h"
#include <assert.h>
#include <stdio.h>
static cursor_frame command = {.mode=CURSOR_SHOW,.width=1024,.height=480,.vw=1024,.vh=480,.x=512,.y=240};
static cursor_frame output;
static void frame(cursor_engine *e, uint64_t now) { cursor_engine_frame(e, now, &output); }
int main(void) {
    cursor_engine e; cursor_engine_reset(&e);
    command.flags=CURSOR_INPUT_DISMISS; command.revision=1;
    cursor_engine_apply(&e,&command,1000); frame(&e,1000); assert(!output.alpha);
    command.flags=CURSOR_INPUT_TOUCH; command.activity=1; command.revision=2;
    cursor_engine_apply(&e,&command,1100); frame(&e,1100); assert(output.alpha==255);
    // State/lease heartbeats must not keep a stationary cursor alive.
    unsigned previous=255;
    for (uint64_t t=1100;t<=2400;t+=25) {
        cursor_engine_apply(&e,&command,t); frame(&e,t);
        assert(output.alpha<=previous); previous=output.alpha;
    }
    assert(!output.alpha && !cursor_engine_click_ttl(&e,2400));
    command.flags=CURSOR_INPUT_RELEASED; command.revision++;
    cursor_engine_apply(&e,&command,2500); frame(&e,2500); assert(!output.alpha);
    // A complete tap coalesced into one snapshot must still count as new input.
    command.activity++; command.revision++;
    cursor_engine_apply(&e,&command,2600); frame(&e,2600); assert(output.alpha==255);
    command.flags=CURSOR_INPUT_FORCE_HIDE; command.revision++;
    cursor_engine_apply(&e,&command,2700); frame(&e,2700); assert(!output.alpha);
    cursor_engine_apply(&e,&command,2800); frame(&e,2800); assert(!output.alpha);
    command.flags=CURSOR_INPUT_TOUCH; command.activity++; command.revision++;
    cursor_engine_apply(&e,&command,2900);
    command.flags=CURSOR_INPUT_RELEASED; command.revision++;
    cursor_engine_apply(&e,&command,3000); frame(&e,3900); assert(output.alpha==255);
    frame(&e,4075); assert(output.alpha>=127 && output.alpha<=128);
    assert(cursor_engine_click_ttl(&e,4075)>0 && cursor_engine_click_ttl(&e,4240)==0);
    frame(&e,4250); assert(!output.alpha);
    // Graceful hide starts at the current alpha, is not restarted by a heartbeat,
    // and relinquishes click ownership immediately while still fading visually.
    command.flags=CURSOR_INPUT_TOUCH; command.activity++; command.revision++;
    cursor_engine_apply(&e,&command,4300); frame(&e,4300); assert(output.alpha==255);
    command.flags=CURSOR_INPUT_DISMISS; command.revision++;
    cursor_engine_apply(&e,&command,4400); frame(&e,4400); assert(output.alpha==255);
    assert(cursor_engine_click_ttl(&e,4400)==0);
    frame(&e,4500); unsigned fading=output.alpha; assert(fading>128 && fading<255);
    cursor_engine_apply(&e,&command,4500); frame(&e,4575); assert(output.alpha>=127 && output.alpha<=128);
    frame(&e,4750); assert(!output.alpha);
    command.flags=CURSOR_INPUT_TOUCH; command.activity++; command.revision++;
    cursor_engine_apply(&e,&command,4800);
    command.flags=CURSOR_INPUT_DISMISS; command.revision++;
    cursor_engine_apply(&e,&command,4810); frame(&e,4850); assert(output.alpha>0);
    command.flags=CURSOR_INPUT_FORCE_HIDE; command.revision++;
    cursor_engine_apply(&e,&command,4850); frame(&e,4850); assert(!output.alpha);
    // Animation reaches its endpoint without any Java animation packets.
    command.mode=CURSOR_LOADER; command.flags=0; command.revision=10;
    command.vx=100; command.vy=20; command.vw=800; command.vh=440;
    cursor_engine_apply(&e,&command,5000); frame(&e,8400);
    assert(output.alpha==255 && output.x==3400 && output.vx==0 && output.vw==1024);
    command.flags=1; cursor_engine_apply(&e,&command,8400);
    frame(&e,8550); unsigned middle=output.alpha; assert(middle<255 && middle>128);
    frame(&e,10400); assert(!output.alpha && e.done);
    cursor_engine_apply(&e,&command,10500); frame(&e,10500); assert(!output.alpha && e.done);
    command.flags=0; command.revision=11; cursor_engine_apply(&e,&command,10600);
    frame(&e,10600); assert(output.alpha==255 && !e.done); // Epoch invalidates previous video.
    command.flags=1; cursor_engine_apply(&e,&command,10700); frame(&e,10900);
    command.flags=0; cursor_engine_apply(&e,&command,10900); frame(&e,12900); assert(output.alpha==255);
    cursor_engine_reset(&e); frame(&e,13000); assert(!output.alpha);
    command.mode=CURSOR_SHOW; command.flags=CURSOR_INPUT_TOUCH;
    command.activity++; command.revision++; command.x=400; command.y=200;
    cursor_engine_apply(&e,&command,14000);
    command.activity++; command.x=445; cursor_engine_apply(&e,&command,14050);
    frame(&e,14100); assert(output.motion_x>.2 && fabs(output.motion_y)<.001);
    assert(output.x==445 && output.y==200); // Optical response never moves the click point.
    float light=output.light_phase;
    frame(&e,14700); assert(fabs(output.motion_x)<.02);
    assert(output.light_phase>light); // Optical motion continues after pointer motion settles.
    command.activity++; command.x=400; cursor_engine_apply(&e,&command,14725);
    frame(&e,14775); assert(output.motion_x<-.1);
    frame(&e,15600); assert(fabs(output.motion_x)<.01 && fabs(output.motion_y)<.01);
    puts("cursor_engine_test: native graceful/force hide, directional glass response, idle/fade/release/coalescing, heartbeat immunity, click expiry, full-canvas PRISM, readiness and epoch reset PASS");
}
