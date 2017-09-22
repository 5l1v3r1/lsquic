/* Copyright (c) 2017 LiteSpeed Technologies Inc.  See LICENSE. */
#ifndef __LSQUIC_CRT_COMPRESS_H__
#define __LSQUIC_CRT_COMPRESS_H__

#include <stdint.h>

struct lsquic_str;

#ifdef __cplusplus
extern "C" {
#endif

enum entry_type {
    END_OF_LIST = 0,
    ENTRY_COMPRESSED = 1,
    ENTRY_CACHED = 2,
    ENTRY_COMMON = 3,
};

typedef struct cert_entry_st {
    enum entry_type type;
    uint32_t index;
    uint64_t hash;
    uint64_t set_hash;
} cert_entry_t;

typedef struct common_cert_st
{
    size_t num_certs;
    const unsigned char* const* certs;
    const size_t* lens;
    uint64_t hash;
} common_cert_t;

struct lsquic_str * get_common_certs_hash();

int get_certs_count(struct lsquic_str *compressed_crt_buf);
int decompress_certs(const unsigned char *in, const unsigned char *in_end,
                     struct lsquic_str *cached_certs, size_t cached_certs_count,
                     struct lsquic_str **out_certs, 
                     size_t *out_certs_count);

void
lsquic_crt_cleanup (void);

#ifdef __cplusplus
}
#endif

#endif //__LSQUIC_CRT_COMPRESS_H__
