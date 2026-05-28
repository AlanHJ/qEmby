#ifndef AES_LITE_H
#define AES_LITE_H
















#include <stddef.h>
#include <stdint.h>

#define AES_BLOCKLEN     16   
#define AES_KEYLEN       32   
#define AES_keyExpSize   240  

struct AES_ctx
{
    uint8_t RoundKey[AES_keyExpSize];
    uint8_t Iv[AES_BLOCKLEN];
};

#ifdef __cplusplus
extern "C"
{
#endif


void AES_init_ctx_iv(struct AES_ctx *ctx, const uint8_t *key, const uint8_t *iv);


void AES_ctx_set_iv(struct AES_ctx *ctx, const uint8_t *iv);



void AES_CTR_xcrypt_buffer(struct AES_ctx *ctx, uint8_t *buf, size_t length);

#ifdef __cplusplus
} 
#endif

#endif 
