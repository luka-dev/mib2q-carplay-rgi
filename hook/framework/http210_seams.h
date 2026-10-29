#ifndef CARPLAY_HTTP210_SEAMS_H
#define CARPLAY_HTTP210_SEAMS_H
/* Compose a fallback behind the framework's pairing/module router. Only a
 * router installed from the pinned stock callback can accept it. Unknown
 * callbacks and conflicting replacements remain untouched. */
int hook_http210_set_request_fallback(void *router, void *expected_stock,
                                     int (*fallback)(void *, void *));
#endif
