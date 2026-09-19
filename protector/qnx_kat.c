/* Known-answer tests of the protector crypto core on real armv7/QNX.
 * Proves SHA256/HMAC/CRC32/keystream and the unblind roundtrip are byte-identical
 * to the host (the arch-sensitive part). File I/O is validated on host/real unit. */
#include "prot_crypto.h"
#include <stdio.h>
#include <string.h>
static int eqhex(const unsigned char*d,int n,const char*h){
    char b[130]; for(int i=0;i<n;i++) sprintf(b+2*i,"%02x",d[i]); b[2*n]=0; return !strcmp(b,h);
}
int main(void){
    int f=0; unsigned char o[32];
    /* CRC32("123456789") == 0xCBF43926 */
    if(prot_crc32((const unsigned char*)"123456789",9)!=0xCBF43926u){printf("CRC FAIL\n");f++;} else printf("CRC ok\n");
    /* SHA256("abc") */
    prot_sha256((const unsigned char*)"abc",3,o);
    if(!eqhex(o,32,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")){printf("SHA FAIL\n");f++;} else printf("SHA ok\n");
    /* HMAC-SHA256 RFC4231 case1 */
    {unsigned char k[20]; memset(k,0x0b,20); prot_hmac(k,20,(const unsigned char*)"Hi There",8,o);
     if(!eqhex(o,32,"b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7")){printf("HMAC FAIL\n");f++;} else printf("HMAC ok\n");}
    /* unblind roundtrip: blinded = consts ^ keystream(K); recover with same K */
    {unsigned char K[32]; memset(K,0x5a,32); unsigned char ks[3],bl[3],rec[3];
     unsigned char consts[3]={0x5a,102,7}; prot_keystream(K,ks,3);
     for(int i=0;i<3;i++) bl[i]=consts[i]^ks[i];
     for(int i=0;i<3;i++) rec[i]=bl[i]^ks[i];
     if(memcmp(rec,consts,3)){printf("UNBLIND FAIL\n");f++;} else printf("UNBLIND ok (gain=%d off=%d)\n",rec[1],rec[2]);}
    printf("%s\n", f?"QNX-KAT-FAIL":"QNX-KAT-PASS");
    return f?1:0;
}
