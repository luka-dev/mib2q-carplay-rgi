#include "../maneuver_render/lane_guidance.h"
#include <assert.h>
#include <stdio.h>
static cr_cmd_t command(int code,int token) {cr_cmd_t c={0};c.cmd=code;c.payload[3]=token;return c;}
int main(int argc,char **argv) {
    cr_lane_decoder_t d;cr_lane_guidance_t state;cr_lane_clear(&d,&state);
    cr_cmd_t begin=command(CMD_LANES_BEGIN,1),lane=command(CMD_LANES_LANE,1),commit=command(CMD_LANES_COMMIT,1);
    begin.payload[4]=1;begin.payload[5]=0;begin.payload[6]=1;begin.payload[11]=73;
    lane.payload[8]=2;lane.payload[9]=3;lane.payload[10]=232;
    lane.payload[11]=252;lane.payload[12]=24;lane.payload[13]=0;lane.payload[14]=45;
    assert(!cr_lane_receive(&d,&begin,&state));assert(state.count==0);
    assert(!cr_lane_receive(&d,&lane,&state));assert(state.count==0);
    assert(cr_lane_receive(&d,&commit,&state));assert(state.count==1 && state.showing && !state.complete);
    assert(state.event_index==73 && state.lanes[0].primary==1000 && state.lanes[0].angles[0]==-1000);
    cr_cmd_t maneuver=command(CMD_MANEUVER,0);
    cr_lane_guidance_t saved=state;assert(!cr_lane_receive(&d,&maneuver,&state));assert(!memcmp(&state,&saved,sizeof(state)));
    // Missing/duplicated/malformed records cannot retain stale guidance.
    cr_lane_receive(&d,&begin,&state);assert(cr_lane_receive(&d,&commit,&state));assert(!state.showing);
    cr_lane_receive(&d,&begin,&state);cr_lane_receive(&d,&lane,&state);cr_lane_receive(&d,&lane,&state);
    assert(cr_lane_receive(&d,&commit,&state));assert(!state.showing);
    for(int n=9;n<256;++n) {begin.payload[4]=n;cr_lane_receive(&d,&begin,&state);assert(cr_lane_receive(&d,&commit,&state));assert(!state.count);}
    begin.payload[4]=1;cr_lane_receive(&d,&begin,&state);lane.payload[3]=2;
    cr_lane_receive(&d,&lane,&state);assert(!d.seen);cr_lane_clear(&d,&state);
    assert(!cr_lane_receive(&d,&commit,&state) && state.event_index==-1);
    if(argc>1) {
        FILE *f=fopen(argv[1],"rb");assert(f);cr_cmd_t c;int commits=0;
        while(fread(&c,1,sizeof(c),f)==sizeof(c)) commits+=cr_lane_receive(&d,&c,&state);
        assert(feof(f));fclose(f);assert(commits==1 && state.count==8 && state.event_index==73 && !state.complete);
        for(int i=0;i<8;++i) {assert(state.lanes[i].position==i && state.lanes[i].angle_count==16);
            assert(state.lanes[i].primary==(i%2==0?1000:-1000));
            for(int j=0;j<16;++j)assert(state.lanes[i].angles[j]==(j%2==0?1000:-1000));}
    }
    puts("Independent lane receiver: atomic commit, maneuver isolation, invalid input, reset and Java wire parity PASS");
}
