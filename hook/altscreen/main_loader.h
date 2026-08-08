#ifndef CARPLAY_MAIN_LOADER_H
#define CARPLAY_MAIN_LOADER_H

/*
 * Early stock-main-screen loader.
 *
 * ScreenRegister is the verified dio_manager -> stock libairplay boundary.
 * main_loader.c interposes it, prepares displayable 59 with stock
 * CScreenRender, and installs the three stock-210 internal GOT seams needed
 * to hand the exact same render object to OmxVideoImpl later.
 */
void main_loader_shutdown(void);

#endif /* CARPLAY_MAIN_LOADER_H */
