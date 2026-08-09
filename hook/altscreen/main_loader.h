#ifndef CARPLAY_MAIN_LOADER_H
#define CARPLAY_MAIN_LOADER_H

/*
 * Early stock-main-screen loader.
 *
 * ScreenRegister is the verified dio_manager -> stock libairplay boundary.
 * main_loader.c interposes it, prepares displayable 59 with stock
 * CScreenRender, and installs the stock-210 internal GOT seams needed to hand
 * the exact same render object to OmxVideoImpl later.  The narrowly-scoped
 * COMXVideoDecoder::start seam also provides the normal, timer-free stock-main
 * readiness barrier for stream 111.  The receive path retains a bounded
 * fail-safe for a missing/rejected seam so altScreen can never gate forever.
 */
/* Returns 1 for a new generation, 0 for an idempotent repeat. */
int  main_loader_session_begin(void *session);
void main_loader_session_end(void *session);
/* Re-arm the readiness barrier while retaining the current control session. */
void main_loader_main_stream_stopped(void *session);
int  main_loader_main_omx_ready(void *session);
int  main_loader_main_omx_gate_available(void);
unsigned int main_loader_session_generation(void *session);
void main_loader_shutdown(void);

#endif /* CARPLAY_MAIN_LOADER_H */
