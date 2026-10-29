#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
uint32_t si_fixture_vtable[27];
int mhi2q_si_transport_install(void *);
int main(void) {
    const char *keys[]={"MHI2Q_CP_SI_EPOCH","MHI2Q_CP_SI_SESSION","MHI2Q_CP_SI_LINK","MHI2Q_CP_SI_PARENT_PID","MHI2Q_DIO_OWNER_MODE"};
    for(unsigned i=0;i<5;++i)unsetenv(keys[i]);
    assert(mhi2q_si_transport_install((void *)0x300000)==0);
    setenv(keys[0],"1",1);
    assert(mhi2q_si_transport_install((void *)0x300000)==-1);
    setenv(keys[1],"1",1);setenv(keys[2],"1",1);setenv(keys[3],"1",1);setenv(keys[4],"wireless",1);
    assert(mhi2q_si_transport_install((void *)0x300000)==-1);
    char parent[32];snprintf(parent,sizeof(parent),"%ld",(long)getppid());setenv(keys[3],parent,1);
    uint32_t *table=si_fixture_vtable;
    const uint32_t expected[]={0x165348,0x16652c,0x1652a0,0x1655a4,0x164ac0,
        0x164ea8,0x164f54,0x166b60,0x166d20,0x167174,0x164738,0x164d54,0x16471c,0x16615c,0x1650dc};
    memcpy(table,expected,sizeof(expected));si_fixture_vtable[26]=0x1650d4;
    setenv(keys[4],"wired",1);
    assert(mhi2q_si_transport_install((void *)0x300000)==-1 && !memcmp(table,expected,sizeof(expected)));
    setenv(keys[4],"wireless",1);table[7]^=4;
    assert(mhi2q_si_transport_install((void *)0x300000)==-1 && table[0]==expected[0]);
    table[7]=expected[7];si_fixture_vtable[26]^=4;
    assert(mhi2q_si_transport_install((void *)0x300000)==-1 && !memcmp(table,expected,sizeof(expected)));
    si_fixture_vtable[26]=0x1650d4;
    assert(mhi2q_si_transport_install((void *)0x300000)==1);
    assert(mhi2q_si_transport_install((void *)0x300000)==1);
    assert(mhi2q_si_transport_install((void *)0x300004)==-1);
    for(unsigned i=2;i<14;++i)assert(table[i]==expected[i]);
    assert(table[0]!=expected[0] && table[1]!=expected[1] && table[14]!=expected[14]);
    assert(si_fixture_vtable[26]!=0x1650d4);
    puts("DIO SI transport guard: absent/invalid/USB/orphan/mismatch/idempotence PASS");
}
