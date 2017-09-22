/* Copyright (c) 2017 LiteSpeed Technologies Inc.  See LICENSE. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "lsquic_types.h"
#include "lsquic_parse.h"
#include "lsquic_rechist.h"
#include "lsquic_util.h"
#include "lsquic.h"

static const struct parse_funcs *const pf = select_pf_by_ver(LSQVER_040);


static lsquic_packno_t
n_acked (const ack_info_t *acki)
{
    lsquic_packno_t n = 0;
    unsigned i;
    for (i = 0; i < acki->n_ranges; ++i)
        n += acki->ranges[i].high - acki->ranges[i].low + 1;
    return n;
}


static void
test1 (void)
{
    /* Test taken from quic_framer_test.cc -- NewAckFrameOneAckBlock */
    unsigned char ack_buf[] = {
     /* TYPE   N        LL       MM */
        0xA0 | 0 << 4 | 1 << 2 | 1,
		0x00,                   /* Number of timestamps */
		0x12, 0x34,             /* Largest acked        */
		0x00, 0x00,             /* Zero delta time      */
		0x12, 0x34,             /* first ack block length */
    };

    ack_info_t acki;
    memset(&acki, 0xF1, sizeof(acki));

    int len = pf->pf_parse_ack_frame(ack_buf, sizeof(ack_buf), &acki);
    assert(("Parsed length is correct (8)", len == sizeof(ack_buf)));
    assert(("Number of ranges is 1", acki.n_ranges == 1));
    assert(("Largest acked is 0x1234", acki.ranges[0].high == 0x1234));
    assert(("Lowest acked is 1", acki.ranges[0].low == 1));
    assert(("Number of timestamps is 0", acki.n_timestamps == 0));
    unsigned n = n_acked(&acki);
    assert(("Number of acked packets is 0x1234", n == 0x1234));
    lsquic_packno_t ack_high = pf->pf_parse_ack_high(ack_buf, sizeof(ack_buf));
    assert(0x1234 == ack_high);

    {
        size_t sz;
        for (sz = 1; sz < sizeof(ack_buf); ++sz)
        {
            len = pf->pf_parse_ack_frame(ack_buf, sz, &acki);
            assert(("Parsing truncated frame failed", len < 0));
        }
    }
}


static void
test2 (void)
{
    /* Test taken from quic_framer_test.cc -- NewAckFrameOneAckBlock */
    unsigned char ack_buf[] = {
     /* TYPE   N        LL       MM */
        0xA0 | 1 << 4 | 1 << 2 | 1,
        0x04,                       /* Num Blocks */
        0x02,                       /* Num TS */
        0x12, 0x34,                 /* Largest acked */
        0x00, 0x00,                 /* Zero delta time. */
        0x00, 0x01,                 /* First ack block length. */
        0x01,                       /* Gap to next block. */
        0x0e, 0xaf,                 /* Ack block length. */
        0xff,                       /* Gap to next block. */
        0x00, 0x00,                 /* Ack block length. */
        0x91,                       /* Gap to next block. */
        0x01, 0xea,                 /* Ack block length. */
        0x05,                       /* Gap to next block. */
        0x00, 0x04,                 /* Ack block length. */
        0x01,                       /* Delta from largest observed. */
        0x10, 0x32, 0x54, 0x76,     /* Delta time. */   /* XXX do we use this at all? */
        0x02,                       /* Delta from largest observed. */
        0x10, 0x32,                 /* Delta time. */
    };

    /* We should get the following array of ranges:
     *    high      low
     *    0x1234    0x1234
     *    0x1232    0x384
     *    0x1F3     0xA
     *    0x4       0x1
     */
    static const struct { unsigned high, low; } ranges[] = {
        {   0x1234, 0x1234 },
        {   0x1232, 0x384 },
        {   0x1F3,  0xA },
        {   0x4,    0x1 },
    };

    ack_info_t acki;
    memset(&acki, 0xF1, sizeof(acki));

    int len = pf->pf_parse_ack_frame(ack_buf, sizeof(ack_buf), &acki);
    assert(("Parsed length is correct (29)", len == sizeof(ack_buf)));
    assert(("Number of ranges is 4", acki.n_ranges == 4));
    assert(("Largest acked is 0x1234", acki.ranges[0].high == 0x1234));
    assert(("Number of timestamps is 2", acki.n_timestamps == 2));
    unsigned n = n_acked(&acki);
    assert(("Number of acked packets is 4254", n == 4254));
    lsquic_packno_t ack_high = pf->pf_parse_ack_high(ack_buf, sizeof(ack_buf));
    assert(0x1234 == ack_high);

    for (n = 0; n < 4; ++n)
        assert(("Range checks out", ranges[n].high == acki.ranges[n].high
                                &&  ranges[n].low  == acki.ranges[n].low));

    {
        size_t sz;
        for (sz = 1; sz < sizeof(ack_buf); ++sz)
        {
            len = pf->pf_parse_ack_frame(ack_buf, sz, &acki);
            assert(("Parsing truncated frame failed", len < 0));
        }
    }
}


static void
test3 (void)
{
    /* Generated by our own code, but failed to parse... */
    unsigned char ack_buf[] = {
     /* TYPE   N        LL       MM */
        0xA0 | 1 << 4 | 0 << 2 | 0,
        0x01,         /* Num ACK block ranges */
        0x00,         /* Number of timestamps */
        0x06,         /* Largest ACKed */
        0x00, 0x00,   /* Delta time */
        0x01,         /* First ACK block length */
        0x02,         /* Gap to next block */
        0x03,         /* Ack block length */
    };

    /* We should get the following array of ranges:
     *    high      low
     *    6         6
     *    3         1
     */
    static const struct { unsigned high, low; } ranges[] = {
        {   6,      6, },
        {   3,      1, },
    };

    ack_info_t acki;
    memset(&acki, 0xF1, sizeof(acki));

    int len = pf->pf_parse_ack_frame(ack_buf, sizeof(ack_buf), &acki);
    assert(("Parsed length is correct (9)", len == sizeof(ack_buf)));
    assert(("Number of ranges is 2", acki.n_ranges == 2));
    assert(("Largest acked is 6", acki.ranges[0].high == 6));
    assert(("Number of timestamps is 0", acki.n_timestamps == 0));
    unsigned n = n_acked(&acki);
    assert(("Number of acked packets is 4", n == 4));
    lsquic_packno_t ack_high = pf->pf_parse_ack_high(ack_buf, sizeof(ack_buf));
    assert(6 == ack_high);

    for (n = 0; n < 2; ++n)
        assert(("Range checks out", ranges[n].high == acki.ranges[n].high
                                &&  ranges[n].low  == acki.ranges[n].low));

    {
        size_t sz;
        for (sz = 1; sz < sizeof(ack_buf); ++sz)
        {
            len = pf->pf_parse_ack_frame(ack_buf, sz, &acki);
            assert(("Parsing truncated frame failed", len < 0));
        }
    }
}


static void
test4 (void)
{
    unsigned char ack_buf[] = {
     /* TYPE   N        LL       MM */
        0xA0 | 1 << 4 | 0 << 2 | 0,
        0x01,       /* Num ACK block ranges */
        0x00,       /* Number of timestamps */
        0x03,       /* Largest ACKed */
        0x23, 0x00, /* Delta time */
        0x01,       /* First ack block length */
        0x01,       /* Gap */
        0x01,       /* Ack block length */
    };

    /* We should get the following array of ranges:
     *    high      low
     *    6         6
     *    3         1
     */
    static const struct { unsigned high, low; } ranges[] = {
        {   3,      3, },
        {   1,      1, },
    };

    ack_info_t acki;
    memset(&acki, 0xF1, sizeof(acki));

    int len = pf->pf_parse_ack_frame(ack_buf, sizeof(ack_buf), &acki);
    assert(("Parsed length is correct (9)", len == sizeof(ack_buf)));
    assert(("Number of ranges is 2", acki.n_ranges == 2));
    assert(("Largest acked is 3", acki.ranges[0].high == 3));
    assert(("Number of timestamps is 0", acki.n_timestamps == 0));
    unsigned n = n_acked(&acki);
    assert(("Number of acked packets is 2", n == 2));
    lsquic_packno_t ack_high = pf->pf_parse_ack_high(ack_buf, sizeof(ack_buf));
    assert(3 == ack_high);

    for (n = 0; n < 2; ++n)
        assert(("Range checks out", ranges[n].high == acki.ranges[n].high
                                &&  ranges[n].low  == acki.ranges[n].low));

    {
        size_t sz;
        for (sz = 1; sz < sizeof(ack_buf); ++sz)
        {
            len = pf->pf_parse_ack_frame(ack_buf, sz, &acki);
            assert(("Parsing truncated frame failed", len < 0));
        }
    }
}


/* Four-byte packet numbers */
static void
test5 (void)
{
    unsigned char ack_buf[] = {
     /* TYPE   N        LL       MM */
        0xA0 | 1 << 4 | 2 << 2 | 2,
        0x01,                       /* Num ack blocks ranges. */
        0x00,                       /* Number of timestamps. */
        0x23, 0x45, 0x67, 0x89,
        0x00, 0x00,                 /* Zero delta time. */
        0x00, 0x00, 0x00, 0x01,     /* First ack block length. */
        33 - 1,                     /* Gap to next block. */
        0x23, 0x45, 0x67, 0x68,     /* Ack block length. */
    };

    /* We should get the following array of ranges:
     *    high      low
     *    6         6
     *    3         1
     */
    static const struct { unsigned high, low; } ranges[] = {
        {   0x23456789,      0x23456789, },
        {   0x23456768,      1, },
    };

    ack_info_t acki;
    memset(&acki, 0xF1, sizeof(acki));

    int len = pf->pf_parse_ack_frame(ack_buf, sizeof(ack_buf), &acki);
    assert(("Parsed length is correct (9)", len == sizeof(ack_buf)));
    assert(("Number of ranges is 2", acki.n_ranges == 2));
    assert(("Largest acked is 0x23456789", acki.ranges[0].high == 0x23456789));
    assert(("Number of timestamps is 0", acki.n_timestamps == 0));
    lsquic_packno_t n = n_acked(&acki);
    assert(("Number of acked packets is correct", n == 0x23456768 + 1));

    for (n = 0; n < 2; ++n)
        assert(("Range checks out", ranges[n].high == acki.ranges[n].high
                                &&  ranges[n].low  == acki.ranges[n].low));

    {
        size_t sz;
        for (sz = 1; sz < sizeof(ack_buf); ++sz)
        {
            len = pf->pf_parse_ack_frame(ack_buf, sz, &acki);
            assert(("Parsing truncated frame failed", len < 0));
        }
    }
}


/* Six-byte packet numbers */
static void
test6 (void)
{
    unsigned char ack_buf[] = {
     /* TYPE   N        LL       MM */
        0xA0 | 1 << 4 | 3 << 2 | 3,
        0x01,                       /* Num ack blocks ranges. */
        0x00,                       /* Number of timestamps. */
        0x00, 0x00, 0xAB, 0xCD, 0x23, 0x45, 0x67, 0x89,
        0x00, 0x00,                 /* Zero delta time. */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, /* First ack block length. */
        33 - 1,                     /* Gap to next block. */
        0x00, 0x00, 0xAB, 0xCD, 0x23, 0x45, 0x67, 0x68, /* Ack block length. */
    };

    static const struct { lsquic_packno_t high, low; } ranges[] = {
        {   0xABCD23456789,      0xABCD23456789, },
        {   0xABCD23456768,      1, },
    };

    ack_info_t acki;
    memset(&acki, 0xF1, sizeof(acki));

    int len = pf->pf_parse_ack_frame(ack_buf, sizeof(ack_buf), &acki);
    assert(("Parsed length is correct", len == sizeof(ack_buf)));
    assert(("Number of ranges is 2", acki.n_ranges == 2));
    assert(("Largest acked is 0xABCD23456789", acki.ranges[0].high == 0xABCD23456789));
    assert(("Number of timestamps is 0", acki.n_timestamps == 0));
    lsquic_packno_t n = n_acked(&acki);
    assert(("Number of acked packets is correct", n == 0xABCD23456768 + 1));

    for (n = 0; n < 2; ++n)
        assert(("Range checks out", ranges[n].high == acki.ranges[n].high
                                &&  ranges[n].low  == acki.ranges[n].low));

    {
        size_t sz;
        for (sz = 1; sz < sizeof(ack_buf); ++sz)
        {
            len = pf->pf_parse_ack_frame(ack_buf, sz, &acki);
            assert(("Parsing truncated frame failed", len < 0));
        }
    }
}


static void
test_max_ack (void)
{
    lsquic_rechist_t rechist;
    lsquic_time_t now;
    unsigned i;
    int has_missing, sz[2];
    const struct lsquic_packno_range *range;
    unsigned char buf[1500];
    struct ack_info acki;

    lsquic_rechist_init(&rechist, 12345);
    now = lsquic_time_now();

    for (i = 1; i <= 300; ++i)
    {
        lsquic_rechist_received(&rechist, i * 10, now);
        now += i * 1000;
    }

    memset(buf, 0xAA, sizeof(buf));

    sz[0] = pf->pf_gen_ack_frame(buf, sizeof(buf),
        (gaf_rechist_first_f)        lsquic_rechist_first,
        (gaf_rechist_next_f)         lsquic_rechist_next,
        (gaf_rechist_largest_recv_f) lsquic_rechist_largest_recv,
        &rechist, now, &has_missing);
    assert(sz[0] > 0);
    assert(sz[0] <= (int) sizeof(buf));
    assert(has_missing);

    assert(0 == buf[ 2 ]);  /* Number of timestamps */
    assert(0xAA == buf[ sz[0] ]);

    sz[1] = pf->pf_parse_ack_frame(buf, sizeof(buf), &acki);
    assert(sz[1] == sz[0]);
    assert(256 == acki.n_ranges);

    for (range = lsquic_rechist_first(&rechist), i = 0;
                        range && i < acki.n_ranges;
                                    range = lsquic_rechist_next(&rechist), ++i)
    {
        assert(range->high == acki.ranges[i].high);
        assert(range->low  == acki.ranges[i].low);
    }
    assert(i == 256);

    lsquic_rechist_cleanup(&rechist);
}


static void
test_ack_truncation (void)
{
    lsquic_rechist_t rechist;
    lsquic_time_t now;
    unsigned i;
    int has_missing, sz[2];
    const struct lsquic_packno_range *range;
    unsigned char buf[1500];
    struct ack_info acki;
    size_t bufsz;

    lsquic_rechist_init(&rechist, 12345);
    now = lsquic_time_now();

    for (i = 1; i <= 300; ++i)
    {
        lsquic_rechist_received(&rechist, i * 10, now);
        now += i * 1000;
    }

    for (bufsz = 200; bufsz < 210; ++bufsz)
    {
        memset(buf, 0xAA, sizeof(buf));
        sz[0] = pf->pf_gen_ack_frame(buf, bufsz,
            (gaf_rechist_first_f)        lsquic_rechist_first,
            (gaf_rechist_next_f)         lsquic_rechist_next,
            (gaf_rechist_largest_recv_f) lsquic_rechist_largest_recv,
            &rechist, now, &has_missing);
        assert(sz[0] > 0);
        assert(sz[0] <= (int) bufsz);
        assert(has_missing);

        assert(0 == buf[ 2 ]);  /* Number of timestamps */
        assert(0xAA == buf[ sz[0] ]);

        sz[1] = pf->pf_parse_ack_frame(buf, sizeof(buf), &acki);
        assert(sz[1] == sz[0]);
        assert(acki.n_ranges < 256);

        for (range = lsquic_rechist_first(&rechist), i = 0;
                            range && i < acki.n_ranges;
                                        range = lsquic_rechist_next(&rechist), ++i)
        {
            assert(range->high == acki.ranges[i].high);
            assert(range->low  == acki.ranges[i].low);
        }
    }

    lsquic_rechist_cleanup(&rechist);
}


int
main (void)
{
    test1();
    test2();
    test3();
    test4();
    test5();
    test6();
    test_max_ack();
    test_ack_truncation();
    return 0;
}
