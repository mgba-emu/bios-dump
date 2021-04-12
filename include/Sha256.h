/* Sha256.h -- SHA-256 Hash
2013-01-18 : Igor Pavlov : Public domain */

#ifndef __CRYPTO_SHA256_H
#define __CRYPTO_SHA256_H

#include <gba_types.h>
typedef uint64_t u64;

#define SHA256_DIGEST_SIZE 32

typedef struct
{
  u32 state[8];
  u64 count;
  u8 buffer[64];
} CSha256;

void Sha256_Init(CSha256 *p);
void Sha256_Update(CSha256 *p, const u8 *data, size_t size);
void Sha256_Final(CSha256 *p, u8 *digest);

#endif
