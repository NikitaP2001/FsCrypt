#ifndef _SHA256_H_
#define _SHA256_H_
#include <stdint.h>

#define SHA256_BLKSIZE 64
#define SHA256_KEYSIZE 32
#define SHA256_STDWCNT 8

struct sha256_state {
    uint32_t hash_data[SHA256_STDWCNT];
};

#ifdef __cplusplus
extern "C" {
#endif

void sha256_state_init(struct sha256_state* _state);

void sha256_process_x86(struct sha256_state* _state, const uint8_t data[],
    uint32_t length);

#if __cplusplus 
bool  sha256_cmp(const struct sha256_state* first,
    const struct sha256_state* second);
#else
_Bool sha256_cmp(const struct sha256_state* first,
    const struct sha256_state* second);
#endif

#ifdef __cplusplus
}   /* extern | "C"*/
#endif

#endif /* _SHA256_H_ */