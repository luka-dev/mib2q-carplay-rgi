/* Guest child for the spawn-injection proof: prints the variables the host
 * cares about, one per line, prefixed CHILDENV:. The file name of the PARENT
 * binary is irrelevant; the parent decides which path to spawn. */
#include <stdio.h>
#include <stdlib.h>

static void show(const char *name) {
    const char *v = getenv(name);
    printf("CHILDENV:%s=%s\n", name, v ? v : "(null)");
}

int main(void) {
    show("MHI2Q_DIO_OWNER_MODE");
    show("MHI2Q_SI_HOOK");
    show("FROM_PARENT");
    fflush(stdout);
    return 0;
}
