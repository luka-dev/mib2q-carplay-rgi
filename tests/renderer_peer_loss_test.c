/* Exercise the production disconnect handler over real loopback TCP. */
#include "../maneuver_render/server.c"
#include <assert.h>
#include <signal.h>

static void peer_loss(int reset) {
    int listener=socket(AF_INET,SOCK_STREAM,0), peer, i;
    struct sockaddr_in addr;
    socklen_t len=sizeof(addr);
    unsigned char events[2*CR_PKT_SIZE];
    int received=0;
    assert(listener>=0);
    memset(&addr,0,sizeof(addr));addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    assert(bind(listener,(struct sockaddr *)&addr,sizeof(addr))==0);
    assert(getsockname(listener,(struct sockaddr *)&addr,&len)==0);
    assert(listen(listener,1)==0);
    cr_server_clear_peer_closed();
    assert(cr_server_init(ntohs(addr.sin_port))==0);
    peer=accept(listener,NULL,NULL);assert(peer>=0);
    struct timeval timeout={2,0};
    setsockopt(peer,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
    cr_server_mark_ready();cr_server_mark_frame_ready();
    while(received<(int)sizeof(events)) {
        int n=(int)recv(peer,events+received,sizeof(events)-received,0);
        assert(n>0);received+=n;
    }
    assert(events[0]==EVT_READY && events[CR_PKT_SIZE]==EVT_FRAME_READY);
    if(reset) {
        struct linger linger_reset={1,0};
        assert(setsockopt(peer,SOL_SOCKET,SO_LINGER,&linger_reset,sizeof(linger_reset))==0);
    }
    close(peer);
    for(i=0;i<100 && !cr_server_peer_closed();i++) {
        cr_server_poll();
        if(!cr_server_peer_closed()) usleep(1000);
    }
    assert(cr_server_peer_closed());
    assert(g_server_fd<0 && g_recv_len==0 && g_event_count==0);
    assert(!g_frame_ready_sent); /* reconnect must not replay stale FRAME_READY */
    cr_server_shutdown();close(listener);
}
int main(void) {
    signal(SIGPIPE,SIG_IGN);
    peer_loss(0);peer_loss(1);
    puts("renderer peer loss: FIN/RST invalidate content and frame-ready PASS");
    return 0;
}
