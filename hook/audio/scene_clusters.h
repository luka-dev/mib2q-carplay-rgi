/* scene_clusters.h — StageClusters of tools/stereo-upmix-render/scene_upmix.py on the
 * struct scene_clusters / scene_place / scene_memory of scene_engine.h (slow pan clustering: the
 * rear stage is a small set of PLACES, spec §13.2 item 2). Float32, no allocation, standalone. */
#ifndef SCENE_CLUSTERS_H
#define SCENE_CLUSTERS_H

#include "scene_engine.h"

/* StageClusters(cfg, frame_s): zeroes *c, copies *cfg into c->cfg, builds the histogram centres and
 * the pick cadence (pick_every = round(pick_every_s / frame_s), a_centre per PICK). */
void scene_clusters_init(struct scene_clusters *c, const struct scene_config *cfg, float frame_s);

/* update(p, ps, ok): p[G] per-group pan, ps[G] primary power, ok[G] eligibility (0/1). Histogram EMA,
 * silence gate, the pick every pick_every frames, admission / shadow / song memory / gain ramps,
 * removal. Call once per analysis frame, before membership(). */
void scene_clusters_update(struct scene_clusters *c, const float *p, const float *ps, const int *ok, int G);

/* membership(p_slow, e_slow): per element i < n (n <= SCENE_BINS) the member weight w in [0,1], the
 * image pan q and the alpha scale; the place pan goes to c->last_pc. e_slow may be NULL (= ones).
 * p_slow / e_slow are kept in c->last_p / c->last_e for the shadow test of the next admission. */
void scene_clusters_membership(struct scene_clusters *c, const float *p_slow, const float *e_slow, int n,
                               float *w, float *q, float *a_scale);

/* reset(): track change — every place fades out (dying), histogram and song memory cleared. */
void scene_clusters_reset(struct scene_clusters *c);

/* number of places with gain > 0 (the n_clusters statistic) */
int  scene_clusters_n_live(const struct scene_clusters *c);

#endif
