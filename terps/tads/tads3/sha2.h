#ifndef SHA2_H
#define SHA2_H

#include <cstddef>
#include <cstdint>

#define CRYPT_OK 0
#define CRYPT_INVALID_ARG 1
#define CRYPT_NOP 2
#define CRYPT_HASH_OVERFLOW 3
#define CRYPT_FAIL_TESTVECTOR 4

struct sha256_state {
    std::uint64_t length;
    std::uint32_t state[8], curlen;
    unsigned char buf[64];
};

union hash_state {
    sha256_state sha256;
};

int sha256_init(hash_state *md);
int sha256_process(hash_state *md, const unsigned char *in, unsigned long inlen);
int sha256_done(hash_state *md, unsigned char *hash);
int sha256_test();

/* Compatibility with Brian Gladman's SHA-256. */
using sha256_ctx = hash_state;
#define sha256_begin(ctx) sha256_init(ctx)
#define sha256_hash(data, len, ctx) sha256_process((ctx), (data), (len))
#define sha256_end(hval, ctx) sha256_done((ctx), (hval))

/* TADS-specific functions */

/* 
 *   Generate a printable version of a hash for a given buffer.  'hash' is an
 *   array of at least 65 characters to receive the hash string.  It's fine
 *   to pass in the same buffer for both 'hash' and 'data', as long as it's
 *   big enough (>=65 characters). [mjr] 
 */
void sha256_ez(char *hash, const char *data, std::size_t data_len);

/*
 *   Generate a printable version of a hash for a given data source. [mjr]
 */
void sha256_datasrc(char *hash, class CVmDataSource *src, unsigned long len);

/* 
 *   printf-style hash construction: format the string given by 'fmt' and the
 *   subsequent arguments, and hash the result 
 */
void sha256_ezf(char *hash, const char *fmt, ...);

#endif
