#include <dlfcn.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

typedef uint64_t (*get_features_fn)(void);
/* Only for loading the stock library in this narrow probe. Feature retrieval
 * does not call this ARM conversion helper; the full receiver has it. */
float __aeabi_l2f(long long value) {
    (void)value;
    return 0.0f;
}

int main(void) {
    void *handle = dlopen("libairplay.so", RTLD_NOW);
    if (!handle) {
        printf("dlopen failed: %s\n", dlerror());
        return 2;
    }
    get_features_fn get_features = (get_features_fn)dlsym(handle, "AirPlayGetFeatures");
    if (!get_features) {
        printf("dlsym failed: %s\n", dlerror());
        return 3;
    }
    uint64_t features = get_features();
    printf("AirPlayGetFeatures=0x%016" PRIx64 " bit37=%u\n",
           features, (unsigned)((features >> 37) & 1));
    dlclose(handle);
    return 0;
}
