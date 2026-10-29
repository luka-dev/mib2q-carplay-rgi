/* Native (host) unit test for si_envv_apply — compiled and executed by
 * wireless/si/test_si_stage1.py. Exit code 0 = pass. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "si_envv.h"

static char *env_a = "LD_LIBRARY_PATH=/eso/lib";
static char *env_b = "IPL_CONFIG_DIR_DIO_MANAGER=/etc/eso/production";
static char *env_c = "MHI2Q_SI_HOOK=1";
static char *env_d = "PATH=/bin";

int main(void) {
    char *const envp[] = {env_a, env_b, env_c, env_d, NULL};
    const char *out[SI_ENVV_MAX_ENTRIES + 1];
    int count = 0, dropped = 0, appended = 0;
    char arena[512];

    /* 1. inject only */
    const char *inj1[] = {"MHI2Q_DIO_OWNER_MODE=wireless"};
    assert(si_envv_apply(envp, NULL, 1, inj1, 1, arena, sizeof(arena),
                         out, &count, &dropped, &appended) == 0);
    assert(count == 5 && dropped == 0 && appended == 1);
    assert(strcmp(out[4], "MHI2Q_DIO_OWNER_MODE=wireless") == 0);
    assert(out[5] == NULL);

    /* 2. drop + inject */
    assert(si_envv_apply(envp, "MHI2Q_SI_HOOK", 1, inj1, 1, arena, sizeof(arena),
                         out, &count, &dropped, &appended) == 0);
    assert(count == 4 && dropped == 1 && appended == 1);
    assert(strcmp(out[3], "MHI2Q_DIO_OWNER_MODE=wireless") == 0);
    assert(out[4] == NULL);

    /* 3. drop without inject: injected value must NOT survive from arena reuse */
    assert(si_envv_apply(envp, "MHI2Q_SI_HOOK", 0, NULL, 0, arena, sizeof(arena),
                         out, &count, &dropped, &appended) == 0);
    assert(count == 3 && dropped == 1 && appended == 0);
    for (int i = 0; i < count; ++i)
        assert(strstr(out[i], "MHI2Q_SI_HOOK") == NULL);

    /* 4. overflow is an error, never truncation */
    char tiny[8];
    assert(si_envv_apply(envp, NULL, 1, inj1, 1, tiny, sizeof(tiny),
                         out, &count, &dropped, &appended) == -1);

    /* 5. path matching */
    assert(si_envv_path_matches("/mnt/app/root/hooks/carplay_startup.sh", "carplay_startup.sh") == 1);
    assert(si_envv_path_matches("/mnt/app/eso/bin/apps/gal", "carplay_startup.sh") == 0);
    assert(si_envv_path_matches(NULL, "x") == 0);
    assert(si_envv_path_matches("x", "") == 0);
    assert(!si_envv_path_matches("/tmp/carplay_startup.sh_probe", "carplay_startup.sh"));
    assert(!si_envv_path_matches("/tmp/not_carplay_startup.sh", "carplay_startup.sh"));
    assert(!si_envv_path_matches("/tmp/carplay_startup.sh", "/hooks/carplay_startup.sh"));

    /* 6. name-prefix drop does not touch unrelated variables */
    char *const envp2[] = {env_a, env_d, NULL};
    assert(si_envv_apply(envp2, "PATH", 0, NULL, 0, arena, sizeof(arena),
                         out, &count, &dropped, &appended) == 0);
    assert(count == 1 && dropped == 1);
    assert(strcmp(out[0], env_a) == 0);

    /* Strip the whole namespace, preserve similarly spelled unrelated keys,
     * replace all duplicates of the injected NAME, including an empty value. */
    char *const dirty[] = {"MHI2Q_SI_HOOK=1", "MHI2Q_SI_HOOK_ROOT=0xbad",
        "MHI2Q_SI_HOOK_CHILD=x", "MHI2Q_SI_HOOKISH=keep",
        "MHI2Q_DIO_OWNER_MODE=wired", "MHI2Q_DIO_OWNER_MODE=", "PATH=/bin", NULL};
    assert(!si_envv_apply(dirty, "MHI2Q_SI_HOOK", 1, inj1, 1, arena, sizeof(arena),
                         out, &count, &dropped, &appended));
    assert(count == 3 && dropped == 5 && appended == 1);
    assert(!strcmp(out[0], "MHI2Q_SI_HOOKISH=keep"));
    assert(!strcmp(out[2], "MHI2Q_DIO_OWNER_MODE=wireless"));
    assert(!si_envv_apply(dirty, "MHI2Q_SI_HOOK", 0, NULL, 0, arena, sizeof(arena),
                         out, &count, &dropped, &appended));
    assert(count == 4 && dropped == 3 && appended == 0);
    const char *bad1[] = {"NO_EQUALS"}, *bad2[] = {"=empty"};
    const char *bad3[] = {"MODE=1", "MODE=2"}, *bad4[] = {"MHI2Q_SI_HOOK_ROOT=bad"};
    assert(si_envv_apply(dirty, NULL, 1, bad1, 1, arena, sizeof(arena), out, &count, &dropped, &appended) == -1);
    assert(si_envv_apply(dirty, NULL, 1, bad2, 1, arena, sizeof(arena), out, &count, &dropped, &appended) == -1);
    assert(si_envv_apply(dirty, NULL, 1, bad3, 2, arena, sizeof(arena), out, &count, &dropped, &appended) == -1);
    assert(si_envv_apply(dirty, "MHI2Q_SI_HOOK", 1, bad4, 1, arena, sizeof(arena), out, &count, &dropped, &appended) == -1);

    /* Deterministic input mutation: repeated keys, empty values, namespace
     * neighbours and alternating injected/unmatched children reuse arenas. */
    unsigned seed = 47426351u;
    char *pool[] = {"MHI2Q_SI_HOOK=1", "MHI2Q_SI_HOOK_ROOT=123",
        "MHI2Q_SI_HOOK_CHILD=x", "MHI2Q_SI_HOOKISH=keep",
        "MHI2Q_DIO_OWNER_MODE=wired", "MHI2Q_DIO_OWNER_MODE=", "PATH=/bin"};
    for (int turn = 0; turn < 10000; ++turn) {
        char *mutated[13];
        for (int i = 0; i < 12; ++i) {
            seed = seed * 1664525u + 1013904223u;
            mutated[i] = pool[(seed >> 16) % 7];
        }
        mutated[12] = NULL;
        int inject = turn & 1, before_modes = 0, after_modes = 0;
        for (int i = 0; i < 12; ++i)
            if (!strncmp(mutated[i], "MHI2Q_DIO_OWNER_MODE=", 21)) ++before_modes;
        assert(!si_envv_apply(mutated, "MHI2Q_SI_HOOK", inject, inj1, 1,
                             arena, sizeof(arena), out, &count, &dropped, &appended));
        for (int i = 0; i < count; ++i) {
            assert(strncmp(out[i], "MHI2Q_SI_HOOK=", 14));
            assert(strncmp(out[i], "MHI2Q_SI_HOOK_", 14));
            if (!strncmp(out[i], "MHI2Q_DIO_OWNER_MODE=", 21)) {
                ++after_modes;
                if (inject) assert(!strcmp(out[i], inj1[0]));
            }
        }
        assert(after_modes == (inject ? 1 : before_modes));
        assert(out[count] == NULL);
    }

    puts("si_envv native tests: PASS (10000 mutations, seed=47426351)");
    return 0;
}
