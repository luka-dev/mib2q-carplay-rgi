/* Independent host AEAD oracle for the framing/state wrapper. Native SDK
 * interoperability is a separate live QNX/iOS check. No test pixels enter it. */
#include "../hook/wireless/paired_screen_crypto.h"
#include <openssl/evp.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
static int oracle_open(const uint8_t *key,const uint8_t *nonce,const void *aad,size_t aad_len,
                       const void *cipher,size_t length,const uint8_t *tag,void *plain)
{
    uint8_t iv[12]={0};memcpy(iv+4,nonce,8);
    EVP_CIPHER_CTX *ctx=EVP_CIPHER_CTX_new();assert(ctx);
    int n=0,final=0;
    int ok=EVP_DecryptInit_ex(ctx,EVP_chacha20_poly1305(),NULL,key,iv)==1 &&
        EVP_DecryptUpdate(ctx,NULL,&n,aad,(int)aad_len)==1 &&
        EVP_DecryptUpdate(ctx,plain,&n,cipher,(int)length)==1 &&
        EVP_CIPHER_CTX_ctrl(ctx,EVP_CTRL_AEAD_SET_TAG,16,(void *)tag)==1 &&
        EVP_DecryptFinal_ex(ctx,(uint8_t *)plain+n,&final)==1;
    EVP_CIPHER_CTX_free(ctx);
    return ok?0:-6754;
}
static void seal(const uint8_t *key,uint64_t nonce,const uint8_t *header,const uint8_t *plain,uint8_t *wire)
{
    uint8_t iv[12]={0};for(unsigned i=0;i<8;++i)iv[4+i]=(uint8_t)(nonce>>(i*8));
    EVP_CIPHER_CTX *ctx=EVP_CIPHER_CTX_new();assert(ctx);
    int n=0,final=0;
    assert(EVP_EncryptInit_ex(ctx,EVP_chacha20_poly1305(),NULL,key,iv)==1);
    assert(EVP_EncryptUpdate(ctx,NULL,&n,header,128)==1);
    assert(EVP_EncryptUpdate(ctx,wire,&n,plain,32)==1 && n==32);
    assert(EVP_EncryptFinal_ex(ctx,wire+n,&final)==1 && !final);
    assert(EVP_CIPHER_CTX_ctrl(ctx,EVP_CTRL_AEAD_GET_TAG,16,wire+32)==1);
    EVP_CIPHER_CTX_free(ctx);
}
int main(void)
{
    uint8_t key[32],plain[32],header[128]={48,0,0,0},wire[48],copy[48];
    for(unsigned i=0;i<32;++i){key[i]=(uint8_t)i;plain[i]=(uint8_t)(0x80+i);}
    cp_screen_crypto state={0,0};uint32_t length=48;
    seal(key,0,header,plain,wire);memcpy(copy,wire,48);
    assert(!cp_screen_open(&state,key,header,copy,&length,oracle_open));
    assert(length==32 && state.nonce==1 && !memcmp(copy,plain,32));
    length=48;memcpy(copy,wire,48);
    assert(cp_screen_open(&state,key,header,copy,&length,oracle_open));
    assert(state.failed && state.nonce==1 && length==48); /* replay */
    uint32_t rng=111;
    for(unsigned n=0;n<1000;++n){
        rng=rng*1664525+1013904223;
        uint8_t h[128],k[32];memcpy(h,header,128);memcpy(k,key,32);memcpy(copy,wire,48);
        unsigned changed=rng%(128+32+48);
        if(changed<128)h[changed]^=1;
        else if(changed<160)k[changed-128]^=1;
        else copy[changed-160]^=1;
        state=(cp_screen_crypto){0,0};length=48;
        assert(cp_screen_open(&state,k,h,copy,&length,oracle_open));
        assert(state.failed && !state.nonce && length==48);
        length=48;memcpy(copy,wire,48);
        assert(cp_screen_open(&state,key,header,copy,&length,oracle_open)); /* fail closed */
    }
    state=(cp_screen_crypto){1,0};length=48;
    seal(key,1,header,plain,copy);
    assert(!cp_screen_open(&state,key,header,copy,&length,oracle_open));
    assert(state.nonce==2 && !memcmp(copy,plain,32));
    puts("independent ChaCha AEAD oracle: valid frames, replay and 1000 header/key/cipher/tag mutations passed");
    return 0;
}
