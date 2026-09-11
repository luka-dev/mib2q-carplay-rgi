/* Copyright (c) 2026, original project author. */
/* Adapted from the production receiver: bounded queue and sink lifecycle. */
#define _POSIX_C_SOURCE 200809L
#include "../altscreen/frame_io.h"
#include "sink.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

static volatile sig_atomic_t g_running = 1;
static volatile sig_atomic_t g_listen_fd = -1;
static volatile sig_atomic_t g_client_fd = -1;
#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK 0x7f000001UL
#endif
#define MAX_RECORD_BYTES (4u * 1024u * 1024u)

enum decode_item_type {
    DECODE_BEGIN = 1,
    DECODE_FRAME = 2,
    DECODE_END   = 3
};

typedef struct decode_item {
    struct decode_item *next;
    uint8_t *data;
    uint32_t len;
    uint32_t generation;
    int type;
} decode_item_t;

#define DECODE_QUEUE_MAX_ITEMS 1024u
#define DECODE_QUEUE_MAX_BYTES (24u * 1024u * 1024u)

static pthread_mutex_t g_decode_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_decode_cond = PTHREAD_COND_INITIALIZER;
static decode_item_t  *g_decode_head;
static decode_item_t  *g_decode_tail;
static unsigned int    g_decode_items;
static size_t          g_decode_bytes;
static uint32_t        g_decode_generation;
static volatile uint32_t g_decode_active_generation;
static int             g_decode_stop;
static pthread_t       g_decode_thread;
static int             g_decode_thread_started;
static volatile int    g_decode_thread_exited;
static unsigned long   g_decode_dropped;
static unsigned long long g_decode_last_progress_ms;
static int             g_decode_worker_busy;

static unsigned long long decode_now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (unsigned long long)ts.tv_sec * 1000ULL
         + (unsigned long long)ts.tv_nsec / 1000000ULL;
}

static void decode_item_free(decode_item_t *item) {
    if (!item) return;
    free(item->data);
    free(item);
}

static void decode_queue_clear_locked(void) {
    decode_item_t *item = g_decode_head;
    while (item) {
        decode_item_t *next = item->next;
        decode_item_free(item);
        item = next;
    }
    g_decode_head = g_decode_tail = NULL;
    g_decode_items = 0;
    g_decode_bytes = 0;
}

static int decode_queue_push_locked(int type, uint32_t generation,
                                    const uint8_t *data, uint32_t len) {
    decode_item_t *item;
    if (g_decode_stop) return -1;
    if (type == DECODE_FRAME &&
        (g_decode_items >= DECODE_QUEUE_MAX_ITEMS ||
         g_decode_bytes + (size_t)len > DECODE_QUEUE_MAX_BYTES)) {
        g_decode_dropped++;
        if (g_decode_dropped <= 3 || (g_decode_dropped % 120) == 0)
            fprintf(stderr,
                    "video_receiver: decode queue full; dropped frame #%lu len=%u\n",
                    g_decode_dropped, (unsigned)len);
        return -1;
    }
    item = (decode_item_t *)calloc(1, sizeof(*item));
    if (!item) return -1;
    if (len) {
        item->data = (uint8_t *)malloc(len);
        if (!item->data) { free(item); return -1; }
        memcpy(item->data, data, len);
    }
    item->type = type;
    item->generation = generation;
    item->len = len;
    if (g_decode_tail) g_decode_tail->next = item;
    else g_decode_head = item;
    g_decode_tail = item;
    g_decode_items++;
    g_decode_bytes += len;
    pthread_cond_signal(&g_decode_cond);
    return 0;
}

static int decode_generation_is_current(uint32_t generation) {
    int current;
    pthread_mutex_lock(&g_decode_lock);
    current = !g_decode_stop && generation == g_decode_generation;
    pthread_mutex_unlock(&g_decode_lock);
    return current;
}

static void *decode_worker_main(void *unused) {
    uint32_t workerGeneration = 0;
    (void)unused;
    for (;;) {
        decode_item_t *item;
        pthread_mutex_lock(&g_decode_lock);
        while (!g_decode_head && !g_decode_stop)
            pthread_cond_wait(&g_decode_cond, &g_decode_lock);
        if (!g_decode_head && g_decode_stop) {
            pthread_mutex_unlock(&g_decode_lock);
            break;
        }
        item = g_decode_head;
        g_decode_head = item->next;
        if (!g_decode_head) g_decode_tail = NULL;
        if (g_decode_items) g_decode_items--;
        if (g_decode_bytes >= item->len) g_decode_bytes -= item->len;
        else g_decode_bytes = 0;
        g_decode_worker_busy = 1;
        pthread_mutex_unlock(&g_decode_lock);

        if (item->type == DECODE_BEGIN) {
            if (decode_generation_is_current(item->generation)) {
                g_decode_active_generation = 0;
                __sync_synchronize();
                av_sink_session_end();
                workerGeneration = item->generation;
                __sync_synchronize();
                g_decode_active_generation = workerGeneration;
                fprintf(stderr,
                        "video_receiver: decoder generation %u begin\n",
                        (unsigned)workerGeneration);
            }
        } else if (item->type == DECODE_FRAME) {
            if (item->generation == workerGeneration &&
                decode_generation_is_current(item->generation))
                av_sink_frame(item->data, item->len);
        } else if (item->type == DECODE_END) {
            if (item->generation == workerGeneration &&
                decode_generation_is_current(item->generation)) {
                g_decode_active_generation = 0;
                __sync_synchronize();
                av_sink_session_end();
                workerGeneration = 0;
            }
        }
        pthread_mutex_lock(&g_decode_lock);
        g_decode_last_progress_ms = decode_now_ms();
        g_decode_worker_busy = 0;
        pthread_mutex_unlock(&g_decode_lock);
        decode_item_free(item);
    }
    g_decode_active_generation = 0;
    __sync_synchronize();
    av_sink_session_end();
    g_decode_thread_exited = 1;
    __sync_synchronize();
    return NULL;
}

static int decode_worker_start(void) {
    int result;
    g_decode_stop = 0;
    g_decode_thread_exited = 0;
    result = pthread_create(&g_decode_thread, NULL, decode_worker_main, NULL);
    if (result == 0) g_decode_thread_started = 1;
    return result == 0 ? 0 : -1;
}

static uint32_t decode_queue_begin(void) {
    uint32_t generation;
    pthread_mutex_lock(&g_decode_lock);
    generation = ++g_decode_generation;
    if (generation == 0) generation = ++g_decode_generation;
    
    decode_queue_clear_locked();
    if (!g_decode_worker_busy) g_decode_last_progress_ms = decode_now_ms();
    if (decode_queue_push_locked(DECODE_BEGIN, generation, NULL, 0) != 0)
        generation = 0;
    pthread_mutex_unlock(&g_decode_lock);
    return generation;
}

static int decode_queue_frame(uint32_t generation,
                              const uint8_t *data, uint32_t len) {
    int result;
    int workerStuck = 0;
    unsigned long long nowMs;
    pthread_mutex_lock(&g_decode_lock);
    if (generation != g_decode_generation) result = -1;
    else result = decode_queue_push_locked(DECODE_FRAME, generation, data, len);
    nowMs = decode_now_ms();
    if (result == 0 && g_decode_items > 2 && nowMs != 0 &&
        g_decode_last_progress_ms != 0 &&
        nowMs - g_decode_last_progress_ms > 20000ULL)
        workerStuck = 1;
    pthread_mutex_unlock(&g_decode_lock);
    if (workerStuck) {
        fprintf(stderr,
                "video_receiver: decoder worker made no progress for 20s; exiting isolated renderer\n");
        _exit(90);
    }
    return result;
}

static void decode_queue_end(uint32_t generation) {
    pthread_mutex_lock(&g_decode_lock);
    if (generation == g_decode_generation)
        decode_queue_push_locked(DECODE_END, generation, NULL, 0);
    pthread_mutex_unlock(&g_decode_lock);
}

static void decode_worker_stop(void) {
    unsigned int waited = 0;
    if (!g_decode_thread_started) return;
    pthread_mutex_lock(&g_decode_lock);
    g_decode_stop = 1;
    decode_queue_clear_locked();
    pthread_cond_broadcast(&g_decode_cond);
    pthread_mutex_unlock(&g_decode_lock);
    while (!g_decode_thread_exited && waited < 1500) {
        struct timespec pause;
        pause.tv_sec = 0;
        pause.tv_nsec = 10L * 1000000L;
        nanosleep(&pause, NULL);
        waited += 10;
    }
    if (!g_decode_thread_exited) {
        fprintf(stderr,
                "video_receiver: decoder worker stop TIMEOUT; exiting isolated renderer\n");
        _exit(88);
    }
    pthread_join(g_decode_thread, NULL);
    g_decode_thread_started = 0;
}

static void on_signal(int sig) {
    int fd;
    (void)sig;
    g_running = 0;
    fd = (int)g_client_fd;
    g_client_fd = -1;
    if (fd >= 0) close(fd);
    fd = (int)g_listen_fd;
    g_listen_fd = -1;
    if (fd >= 0) close(fd);
}

static int listen_socket(unsigned short port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }

    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind"); close(fd); return -1; }
    if (listen(fd, 1) < 0) { perror("listen"); close(fd); return -1; }
    return fd;
}

/* Adapted receive loop. Input is a complete, already-plain Annex-B record.
 * Data-plane silence is allowed; socket EOF/errors end a session.
 */
static void serve_client(int fd, uint32_t generation)
{
    int one=1;
    uint8_t *buffer=NULL;
    uint32_t capacity=0, length=0;
    setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&one,sizeof(one));
    setsockopt(fd,SOL_SOCKET,SO_KEEPALIVE,&one,sizeof(one));
    while (g_running) {
        int result=frame_read(fd,&buffer,&capacity,&length,MAX_RECORD_BYTES);
        if (result != 0) {
            fprintf(stderr,"video_receiver: record reader ended status=%d\n",result);
            break;
        }
        if (length && decode_queue_frame(generation,buffer,length) != 0) break;
    }
    free(buffer);
}

int main(int argc, char **argv)
{
    char *end=NULL;
    long port;
    int lfd;
    if (argc!=2) { fprintf(stderr,"usage: %s PORT\n",argv[0]); return 2; }
    errno=0;
    port=strtol(argv[1],&end,10);
    if (errno || !*argv[1] || *end || port<1 || port>65535) return 2;
    signal(SIGINT,on_signal);
    signal(SIGTERM,on_signal);
    signal(SIGPIPE,SIG_IGN);
    setvbuf(stderr,NULL,_IONBF,0);
    if (av_sink_open()!=0) return 1;
    if (decode_worker_start()!=0) { av_sink_close(); return 1; }
    lfd=listen_socket((unsigned short)port);
    if (lfd<0) { decode_worker_stop(); av_sink_close(); return 1; }
    g_listen_fd=lfd;
    fprintf(stderr,"video_receiver: listening on loopback port %ld\n",port);
    while (g_running) {
        int fd=accept(lfd,NULL,NULL);
        if (fd<0) { if (errno==EINTR) continue; break; }
        g_client_fd=fd;
        uint32_t generation=decode_queue_begin();
        if (generation) {
            serve_client(fd,generation);
            decode_queue_end(generation);
        }
        if ((int)g_client_fd==fd) { g_client_fd=-1; close(fd); }
    }
    if ((int)g_listen_fd==lfd) { g_listen_fd=-1; close(lfd); }
    decode_worker_stop();
    av_sink_close();
    return 0;
}
