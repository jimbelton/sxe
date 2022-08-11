/* Copyright (c) 2013 OpenDNS.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <assert.h>
#include <string.h>

#include "sxe-cdb-private.h"
#include "sxe-log.h"
#include "sxe-time.h"
#include "sxe-util.h"
#include "tap.h"
#include "murmurhash3.h"

#define TEST_SXE_CDB_PREPARE(KEY,NUM) sxe_cdb_prepare(KEY[NUM], KEY##_len[NUM])
#define TEST_IS(HKV,KEY,KEY_LEN,VARIANT,SUB_VARIANT) \
    hkv = HKV; \
    ok(NULL !=             hkv                                   , "%s: %s: hkv                not NULL as expected", VARIANT, SUB_VARIANT); \
    is(                    sxe_cdb_tls_hkv_part.key_len, KEY_LEN , "%s: %s: hkv_part.key_len   is       as expected", VARIANT, SUB_VARIANT); \
    ok(0    == memcmp(KEY, sxe_cdb_tls_hkv_part.key    , KEY_LEN), "%s: %s: hkv_part.key bytes are      as expected", VARIANT, SUB_VARIANT);

static void
test_runaway_variant(
    int variant,
    const char * variant_text,
    int lo2hi_key_1st,
    int lo2hi_key_2nd,
    int lo2hi_key_3rd,
    int lo2hi_key_4th,
    int hi2lo_key_1st,
    int hi2lo_key_2nd,
    int hi2lo_key_3rd,
    int hi2lo_key_4th)
{
    SXE_CDB_INSTANCE * ci = sxe_cdb_instance_new(0 /* grow from minimum size */, 0 /* grow to maximum allowed size */);
    uint32_t           cl = 0; /* counts_list */
    SXE_CDB_HKV      * hkv;
    SXE_CDB_UID        uid[4+1]; /* ignore [0] */
    uint8_t          * key[4+1]; /* ignore [0] */
    uint8_t            key_1[] = "key_1"   ;
    uint8_t            key_2[] = "_key_2"  ;
    uint8_t            key_3[] = "__key_3" ;
    uint8_t            key_4[] = "___key_4";
    uint32_t           key_len[4+1];
    int                top[2][4] = { { lo2hi_key_1st, lo2hi_key_2nd, lo2hi_key_3rd, lo2hi_key_4th }, { hi2lo_key_1st, hi2lo_key_2nd, hi2lo_key_3rd, hi2lo_key_4th } };
    int                cnt[2][4] = { { 1, 2, 2, 4 }, { 4, 2, 2, 1 } };

    key[1] = key_1; key_len[1] = sizeof(key_1);
    key[2] = key_2; key_len[2] = sizeof(key_2);
    key[3] = key_3; key_len[3] = sizeof(key_3);
    key[4] = key_4; key_len[4] = sizeof(key_4);

    TEST_SXE_CDB_PREPARE(key, 1); is(sxe_cdb_instance_inc(ci, cl), 1, "%s: key[1] has expected count", variant_text); /* start off by incrementing 3 keys */
    TEST_SXE_CDB_PREPARE(key, 2); is(sxe_cdb_instance_inc(ci, cl), 1, "%s: key[2] has expected count", variant_text);
    TEST_SXE_CDB_PREPARE(key, 3); is(sxe_cdb_instance_inc(ci, cl), 1, "%s: key[3] has expected count", variant_text);

    switch (variant) {                                                                                            /* increment all the keys again but in different variants one key always get incremented more */
    case 0:
    TEST_SXE_CDB_PREPARE(key, 1); is(sxe_cdb_instance_inc(ci, cl), 2, "%s: key[1] has expected count", variant_text);
    TEST_SXE_CDB_PREPARE(key, 1); is(sxe_cdb_instance_inc(ci, cl), 3, "%s: key[1] has expected count", variant_text);
    TEST_SXE_CDB_PREPARE(key, 1); is(sxe_cdb_instance_inc(ci, cl), 4, "%s: key[1] has expected count", variant_text);
    TEST_SXE_CDB_PREPARE(key, 2); is(sxe_cdb_instance_inc(ci, cl), 2, "%s: key[2] has expected count", variant_text);
    TEST_SXE_CDB_PREPARE(key, 3); is(sxe_cdb_instance_inc(ci, cl), 2, "%s: key[3] has expected count", variant_text);
    break;
    case 1:
    TEST_SXE_CDB_PREPARE(key, 2); is(sxe_cdb_instance_inc(ci, cl), 2, "%s: key[2] has expected count", variant_text);
    TEST_SXE_CDB_PREPARE(key, 2); is(sxe_cdb_instance_inc(ci, cl), 3, "%s: key[2] has expected count", variant_text);
    TEST_SXE_CDB_PREPARE(key, 2); is(sxe_cdb_instance_inc(ci, cl), 4, "%s: key[2] has expected count", variant_text);
    TEST_SXE_CDB_PREPARE(key, 1); is(sxe_cdb_instance_inc(ci, cl), 2, "%s: key[1] has expected count", variant_text);
    TEST_SXE_CDB_PREPARE(key, 3); is(sxe_cdb_instance_inc(ci, cl), 2, "%s: key[3] has expected count", variant_text);
    break;
    case 2:
    TEST_SXE_CDB_PREPARE(key, 3); is(sxe_cdb_instance_inc(ci, cl), 2, "%s: key[3] has expected count", variant_text);
    TEST_SXE_CDB_PREPARE(key, 3); is(sxe_cdb_instance_inc(ci, cl), 3, "%s: key[3] has expected count", variant_text);
    TEST_SXE_CDB_PREPARE(key, 3); is(sxe_cdb_instance_inc(ci, cl), 4, "%s: key[3] has expected count", variant_text);
    TEST_SXE_CDB_PREPARE(key, 1); is(sxe_cdb_instance_inc(ci, cl), 2, "%s: key[1] has expected count", variant_text);
    TEST_SXE_CDB_PREPARE(key, 2); is(sxe_cdb_instance_inc(ci, cl), 2, "%s: key[2] has expected count", variant_text);
    break;
    default:
    SXEA1(0,"ERROR: INTERNAL: unexpected variant %d", variant);
    }

    TEST_SXE_CDB_PREPARE(key, 4); is(sxe_cdb_instance_inc(ci, cl), 1 , "%s: key[4] has expected count", variant_text); /* finally a new 4th and last key gets incremented to 1 which is the lowest count */

    /* test finding the 4 keys via the uid api */

    TEST_SXE_CDB_PREPARE(key, 1); uid[1].as_u64.u = sxe_cdb_instance_get_uid(ci); ok(uid[1].as_u64.u != SXE_CDB_UID_NONE, "%s: found uid[1]", variant_text);
    TEST_SXE_CDB_PREPARE(key, 2); uid[2].as_u64.u = sxe_cdb_instance_get_uid(ci); ok(uid[2].as_u64.u != SXE_CDB_UID_NONE, "%s: found uid[2]", variant_text);
    TEST_SXE_CDB_PREPARE(key, 3); uid[3].as_u64.u = sxe_cdb_instance_get_uid(ci); ok(uid[3].as_u64.u != SXE_CDB_UID_NONE, "%s: found uid[3]", variant_text);
    TEST_SXE_CDB_PREPARE(key, 4); uid[4].as_u64.u = sxe_cdb_instance_get_uid(ci); ok(uid[4].as_u64.u != SXE_CDB_UID_NONE, "%s: found uid[4]", variant_text);

    TEST_IS(sxe_cdb_instance_get_uid_hkv(ci, uid[1]), key[1], key_len[1], variant_text, "uid2hkv");
    TEST_IS(sxe_cdb_instance_get_uid_hkv(ci, uid[2]), key[2], key_len[2], variant_text, "uid2hkv");
    TEST_IS(sxe_cdb_instance_get_uid_hkv(ci, uid[3]), key[3], key_len[3], variant_text, "uid2hkv");
    TEST_IS(sxe_cdb_instance_get_uid_hkv(ci, uid[4]), key[4], key_len[4], variant_text, "uid2hkv");

    /* test enumerating the 4 keys via the walk api; from lo2hi & hi2lo */

    int way;
    for (way = 0; way < 2; way++) {
        const char * way_text;
                     way_text = way ? "hi2lo  " : "lo2hi  ";
        sxe_cdb_tls_walk_cnt_pos = SXE_CDB_COUNT_NONE;
        TEST_IS(sxe_cdb_instance_walk(ci, way, sxe_cdb_tls_walk_cnt_pos, sxe_cdb_tls_walk_hkv_pos, cl), key[top[way][0]], key_len[top[way][0]], variant_text, way_text); is(sxe_cdb_tls_walk_count, cnt[way][0], "%s: %s: sxe_cdb_instance_walk() %d   is expected count", variant_text, way_text, cnt[way][0]);
        TEST_IS(sxe_cdb_instance_walk(ci, way, sxe_cdb_tls_walk_cnt_pos, sxe_cdb_tls_walk_hkv_pos, cl), key[top[way][1]], key_len[top[way][1]], variant_text, way_text); is(sxe_cdb_tls_walk_count, cnt[way][1], "%s: %s: sxe_cdb_instance_walk() %d   is expected count", variant_text, way_text, cnt[way][1]);
        TEST_IS(sxe_cdb_instance_walk(ci, way, sxe_cdb_tls_walk_cnt_pos, sxe_cdb_tls_walk_hkv_pos, cl), key[top[way][2]], key_len[top[way][2]], variant_text, way_text); is(sxe_cdb_tls_walk_count, cnt[way][2], "%s: %s: sxe_cdb_instance_walk() %d   is expected count", variant_text, way_text, cnt[way][2]);
        TEST_IS(sxe_cdb_instance_walk(ci, way, sxe_cdb_tls_walk_cnt_pos, sxe_cdb_tls_walk_hkv_pos, cl), key[top[way][3]], key_len[top[way][3]], variant_text, way_text); is(sxe_cdb_tls_walk_count, cnt[way][3], "%s: %s: sxe_cdb_instance_walk() %d   is expected count", variant_text, way_text, cnt[way][3]);
             is(sxe_cdb_tls_walk_cnt_pos, SXE_CDB_COUNT_NONE                                                                                                                                                   , "%s: %s: sxe_cdb_instance_walk()        expected end"   , variant_text, way_text             );
    }

    sxe_cdb_instance_destroy(ci);
} /* test_runaway_variant() */

int
main(void)
{
    SXE_TIME start_time;
    double   elapsed_time;
    uint32_t i;
    unsigned keys = 1750000/2/2; /* to make test run faster */
    uint8_t  header_len_3_key[KEY_HEADER_LEN_3_KEY_LEN_MAX]; /* 127 bytes */
    uint8_t  header_len_5_key[KEY_HEADER_LEN_5_KEY_LEN_MAX]; /* 65535 bytes */
    uint8_t  header_len_8_key[KEY_HEADER_LEN_5_KEY_LEN_MAX + 1 /* 2^24 too big :-) */];

    plan_tests(223);

    /* tests for key count double double linked lists; different runaway variants test different linked list fine details :-) */

    test_runaway_variant(0, "runaway key1", 4,3,2,1, 1,3,2,4); // expected walk key order: lo2hi 4 3 2 1, hi2lo 1 3 2 4
    test_runaway_variant(1, "runaway key2", 4,3,1,2, 2,3,1,4); // expected walk key order: lo2hi 4 3 1 2, hi2lo 2 3 1 4
    test_runaway_variant(2, "runaway key3", 4,2,1,3, 3,2,1,4); // expected walk key order: lo2hi 4 2 1 3, hi2lo 3 2 1 4

    /* tests for coverage */

    {
        SXE_CDB_INSTANCE * cdb_instance = sxe_cdb_instance_new(0 /* grow from minimum size */, 0 /* grow to maximum allowed size */);
        uint32_t           counts_list  = 0;

           sxe_cdb_prepare         (&header_len_3_key[0], sizeof(header_len_3_key));
        is(sxe_cdb_instance_get_uid(cdb_instance         ),   SXE_CDB_UID_NONE, "coverage: missed header_len_3_key as expected");

           sxe_cdb_prepare         (&header_len_5_key[0], sizeof(header_len_5_key));
        is(sxe_cdb_instance_get_uid(cdb_instance         ),   SXE_CDB_UID_NONE, "coverage: missed header_len_5_key as expected");

           sxe_cdb_prepare         (&header_len_8_key[0], sizeof(header_len_8_key));
        is(sxe_cdb_instance_get_uid(cdb_instance         ),   SXE_CDB_UID_NONE, "coverage: missed header_len_8_key as expected");

        ok(sxe_cdb_instance_put_val(cdb_instance, NULL, 0) != SXE_CDB_UID_NONE, "coverage: append header_len_3_key as expected");
        ok(sxe_cdb_instance_get_uid(cdb_instance         ) != SXE_CDB_UID_NONE, "coverage: found  header_len_3_key as expected");

           sxe_cdb_prepare         (&header_len_5_key[0], sizeof(header_len_5_key));
        ok(sxe_cdb_instance_put_val(cdb_instance, NULL, 0) != SXE_CDB_UID_NONE, "coverage: append header_len_5_key as expected");
        ok(sxe_cdb_instance_get_uid(cdb_instance         ) != SXE_CDB_UID_NONE, "coverage: found  header_len_5_key as expected");

           sxe_cdb_prepare         (&header_len_8_key[0], sizeof(header_len_8_key));
        ok(sxe_cdb_instance_put_val(cdb_instance, NULL, 0) != SXE_CDB_UID_NONE, "coverage: found  header_len_8_key as expected");
        ok(sxe_cdb_instance_get_uid(cdb_instance         ) != SXE_CDB_UID_NONE, "coverage: found  header_len_8_key as expected");

        sxe_cdb_prepare(NULL,                                0); is(sxe_cdb_instance_put_val(cdb_instance, NULL, 0), SXE_CDB_UID_NONE, "coverage: invalid key length causes sxe_cdb_instance_put() to fail");
        sxe_cdb_prepare(NULL, KEY_HEADER_LEN_8_KEY_LEN_MAX + 1); is(sxe_cdb_instance_put_val(cdb_instance, NULL, 0), SXE_CDB_UID_NONE, "coverage: invalid key length causes sxe_cdb_instance_put() to fail");

        /* test for sxe_cdb_instance_walk_pos_is_bad() coverage */

              sxe_cdb_prepare     ((const uint8_t *) "foo", 3);
        SXEA1(sxe_cdb_instance_inc(cdb_instance, counts_list), "ERROR: INTERNAL: sxe_cdb_instance_inc() unexpectedly failing"); /* create a single counter instance for tests below */

        is(sxe_cdb_instance_walk_pos_is_bad(cdb_instance, 1 + cdb_instance->counts_size, 1 + cdb_instance->kvdata_used, "test: coverage: sxe_cdb_instance_walk_pos_is_bad(): cnt_pos is too large"), 1, "coverage: sxe_cdb_instance_walk_pos_is_bad: cnt_pos is too large");
        is(sxe_cdb_instance_walk_pos_is_bad(cdb_instance, 1                            , 1 + cdb_instance->kvdata_used, "test: coverage: sxe_cdb_instance_walk_pos_is_bad(): hkv_pos is too large"), 1, "coverage: sxe_cdb_instance_walk_pos_is_bad: hkv_pos is too large");

           SXE_CDB_HKV_LIST hkv_list = { UINT32_MAX, 0, UINT32_MAX }; /* set everything to the max to create overflow situation below */
           sxe_cdb_prepare         ((const uint8_t *) "bar", 3);
        ok(sxe_cdb_instance_put_val(cdb_instance, (const uint8_t *) &hkv_list, sizeof(hkv_list)) != SXE_CDB_UID_NONE, "coverage: hkv_pos + header len is out of range: added bogus SXE_CDB_HKV_LIST");
           SXE_CDB_HKV * hkv     = sxe_cdb_instance_get_hkv_raw(cdb_instance);
           uint32_t      hkv_pos = (uint8_t *) hkv - cdb_instance->kvdata;
        is(sxe_cdb_instance_walk_pos_is_bad(cdb_instance, 1, hkv_pos + 3 + sizeof(hkv_list) - 1, "test: coverage: sxe_cdb_instance_walk_pos_is_bad(): hkv_pos + header len is out of range"                                       ), 1, "coverage: sxe_cdb_instance_walk_pos_is_bad: hkv_pos + header len is out of range"                                       );
        is(sxe_cdb_instance_walk_pos_is_bad(cdb_instance, 1, hkv_pos + 0 + sizeof(hkv_list) - 1, "test: coverage: sxe_cdb_instance_walk_pos_is_bad(): hkv_pos + given hkv_pos + hkv_len is out of range"                          ), 1, "coverage: sxe_cdb_instance_walk_pos_is_bad: given hkv_pos + hkv_len is out of range"                                    );
        is(sxe_cdb_instance_walk_pos_is_bad(cdb_instance, 1, hkv_pos                           , "test: coverage: sxe_cdb_instance_walk_pos_is_bad(): val_len incorrect, or, value as SXE_CDB_HKV_LIST does not reference cnt_pos"), 1, "coverage: sxe_cdb_instance_walk_pos_is_bad: val_len incorrect, or, value as SXE_CDB_HKV_LIST does not reference cnt_pos");

        ok(sxe_cdb_instance_walk(cdb_instance, 1, 0                            , 0                            , SXE_CDB_COUNTS_LISTS_MAX) == NULL, "coverage:                                     sxe_cdb_instance_walk()");
        ok(sxe_cdb_instance_walk(cdb_instance, 1, 1 + cdb_instance->counts_used, 1 + cdb_instance->kvdata_used, counts_list             ) == NULL, "coverage: sxe_cdb_instance_walk_pos_is_bad(): sxe_cdb_instance_walk()");

        /* test for sxe_cdb_instance_inc() coverage */

           sxe_cdb_prepare         ((const uint8_t *) "bad", 3);
        ok(sxe_cdb_instance_put_val(cdb_instance, NULL, 0     ) != SXE_CDB_UID_NONE, "coverage: bad key counter: put regular key");
        ok(sxe_cdb_instance_inc    (cdb_instance, counts_list ) == 0               , "coverage: bad key counter: try to increment regular key");

           sxe_cdb_prepare         ((const uint8_t *) "bar", 3);
        ok(sxe_cdb_instance_inc    (cdb_instance, counts_list ) == 0               , "coverage: bad key counter: try to increment key with counter size value with bad reference");

           sxe_cdb_instance_destroy(cdb_instance);
    }

    /* tests for ensemble swap & reboot */

    {
        SXE_CDB_UID        uid             ;
        SXE_CDB_UID        uids[keys]      ;
        uint32_t           instances    = 8;
        uint32_t           keys_to_swap = 5;
        SXE_CDB_ENSEMBLE * this_cdb_ensemble = sxe_cdb_ensemble_new(0 /* grow from minimum size */, 0 /* grow to maximum allowed size */, instances /* number of cdb instances */, 1 /*   locked */);
        SXE_CDB_ENSEMBLE * that_cdb_ensemble = sxe_cdb_ensemble_new(0 /* grow from minimum size */, 0 /* grow to maximum allowed size */, instances /* number of cdb instances */, 0 /* unlocked */);

        for (i = 0; i < (2 * keys_to_swap); i++) { /* put half of the keys in this ensemble and the other half in that ensemble */
                                                         sxe_cdb_prepare         ((const uint8_t *) &i, sizeof(i));
                                      uids[i].as_u64.u = sxe_cdb_ensemble_put_val(i < keys_to_swap ? this_cdb_ensemble : that_cdb_ensemble, (const uint8_t *) &i, sizeof(i));
            SXEA1(SXE_CDB_UID_NONE != uids[i].as_u64.u, "ERROR: INTERNAL: sxe_cdb_ensemble_put_val() unexpectedly failing");
        }

        sxe_cdb_ensemble_swap_instances(this_cdb_ensemble, that_cdb_ensemble); /* note: 'this_cdb_ensemble : that_cdb_ensemble' before and 'that_cdb_ensemble : this_cdb_ensemble' after :-) */

        for (i = 0; i < (2 * keys_to_swap); i++) {
                           sxe_cdb_prepare         ((const uint8_t *) &i, sizeof(i));
            uid.as_u64.u = sxe_cdb_ensemble_get_uid(i < keys_to_swap ? that_cdb_ensemble : this_cdb_ensemble); SXEA1(uids[i].as_u64.u == uid.as_u64.u, "ERROR: INTERNAL: unexpected uid at i=%u", i);
        }

        sxe_cdb_ensemble_reboot(this_cdb_ensemble);

        for (i = 0; i < (2 * keys_to_swap); i++) {
                           sxe_cdb_prepare         ((const uint8_t *) &i, sizeof(i));
            uid.as_u64.u = sxe_cdb_ensemble_get_uid(i < keys_to_swap ? that_cdb_ensemble : this_cdb_ensemble); SXEA1((i < keys_to_swap ? uids[i].as_u64.u : SXE_CDB_UID_NONE) == uid.as_u64.u, "ERROR: INTERNAL: unexpected uid at i=%u", i);
        }

        sxe_cdb_ensemble_destroy(that_cdb_ensemble);
        sxe_cdb_ensemble_destroy(this_cdb_ensemble);
    }

    /* test sxe_cdb_ensemble_set_uid_hkv() & sxe_cdb_ensemble_get_hkv_locked() & sxe_cdb_ensemble_get_hkv_unlock() */

    {
        uint32_t           instances  = 8;
        SXE_CDB_ENSEMBLE * cdb_ensemble = sxe_cdb_ensemble_new(0 /* grow from minimum size */, 0 /* grow to maximum allowed size */, instances /* number of cdb instances */, 1 /* locked */);
        SXE_CDB_UID        uid    ;
        SXE_CDB_HKV      * tls_hkv;

                                                                                            sxe_cdb_prepare                        (              (const uint8_t *) "foo", 3);
                                                                             uid.as_u64.u = sxe_cdb_ensemble_put_val               (cdb_ensemble, (const uint8_t *) "BAR", 3);
                                                                             tls_hkv      = sxe_cdb_ensemble_get_hkv_raw_locked    (cdb_ensemble     ); ok(tls_hkv != NULL                  , "Update same-sized value: non-NULL tls_hkv                as expected");
                                                                                                                                                        is(sxe_cdb_tls_hkv_part.val[0] , 'B', "Update same-sized value: before   direct update: got 'B' as expected");
                                                                                                                                                        is(sxe_cdb_tls_hkv_part.val[1] , 'A', "Update same-sized value: before   direct update: got 'A' as expected");
                                                                                                                                                        is(sxe_cdb_tls_hkv_part.val[2] , 'R', "Update same-sized value: before   direct update: got 'R' as expected");
                                                                                                                                                           sxe_cdb_tls_hkv_part.val[0] = 'b';
                                                                                                                                                           sxe_cdb_tls_hkv_part.val[1] = 'r';
                                                                                                                                                           sxe_cdb_tls_hkv_part.val[2] = 'a';
                                                                                                                                                           sxe_cdb_tls_hkv_part.val    = NULL; /* ensure sxe_cdb_ensemble_get_uid_hkv_raw_locked() sets .val */
                                                                                            sxe_cdb_ensemble_get_hkv_raw_unlock    (cdb_ensemble     );
                                                                                            sxe_cdb_ensemble_get_uid_hkv_raw_locked(cdb_ensemble, uid); ok(tls_hkv != NULL                  , "Update same-sized value: non-NULL tls_hkv                as expected");
                                                                                                                                                        is(sxe_cdb_tls_hkv_part.val[0] , 'b', "Update same-sized value: before   direct update: got 'b' as expected");
                                                                                                                                                        is(sxe_cdb_tls_hkv_part.val[1] , 'r', "Update same-sized value: before   direct update: got 'r' as expected");
                                                                                                                                                        is(sxe_cdb_tls_hkv_part.val[2] , 'a', "Update same-sized value: before   direct update: got 'a' as expected");
                                                                                                                                                           sxe_cdb_tls_hkv_part.val[0] = 'b';
                                                                                                                                                           sxe_cdb_tls_hkv_part.val[1] = 'a';
                                                                                                                                                           sxe_cdb_tls_hkv_part.val[2] = 'r';
                                                                                                                                                           sxe_cdb_tls_hkv_part.val    = NULL; /* ensure sxe_cdb_ensemble_get_uid_hkv() sets .val */
                                                                                            sxe_cdb_ensemble_get_uid_hkv_raw_unlock(cdb_ensemble, uid);
                                                                                            sxe_cdb_ensemble_get_uid_hkv           (cdb_ensemble, uid); is(sxe_cdb_tls_hkv_part.val[0] , 'b', "Update same-sized value: before indirect update: got 'b' as expected");
                                                                                                                                                        is(sxe_cdb_tls_hkv_part.val[1] , 'a', "Update same-sized value: before indirect update: got 'a' as expected");
                                                                                                                                                        is(sxe_cdb_tls_hkv_part.val[2] , 'r', "Update same-sized value: before indirect update: got 'r' as expected");
        sxe_cdb_tls_hkv_part.val[0] = 'f'; sxe_cdb_ensemble_set_uid_hkv(cdb_ensemble, uid); sxe_cdb_ensemble_get_uid_hkv           (cdb_ensemble, uid); is(sxe_cdb_tls_hkv_part.val[0] , 'f', "Update same-sized value: update indirect char 1: got 'f' as expected");
                                                                                                                                                        is(sxe_cdb_tls_hkv_part.val[1] , 'a', "Update same-sized value: update indirect char 1: got 'a' as expected");
                                                                                                                                                        is(sxe_cdb_tls_hkv_part.val[2] , 'r', "Update same-sized value: update indirect char 1: got 'r' as expected");
        sxe_cdb_tls_hkv_part.val[1] = 'i'; sxe_cdb_ensemble_set_uid_hkv(cdb_ensemble, uid); sxe_cdb_ensemble_get_uid_hkv           (cdb_ensemble, uid); is(sxe_cdb_tls_hkv_part.val[0] , 'f', "Update same-sized value: update indirect char 2: got 'f' as expected");
                                                                                                                                                        is(sxe_cdb_tls_hkv_part.val[1] , 'i', "Update same-sized value: update indirect char 2: got 'i' as expected");
                                                                                                                                                        is(sxe_cdb_tls_hkv_part.val[2] , 'r', "Update same-sized value: update indirect char 2: got 'r' as expected");
        sxe_cdb_tls_hkv_part.val[2] = 't'; sxe_cdb_ensemble_set_uid_hkv(cdb_ensemble, uid); sxe_cdb_ensemble_get_uid_hkv           (cdb_ensemble, uid); is(sxe_cdb_tls_hkv_part.val[0] , 'f', "Update same-sized value: update indirect char 3: got 'f' as expected");
                                                                                                                                                        is(sxe_cdb_tls_hkv_part.val[1] , 'i', "Update same-sized value: update indirect char 3: got 'i' as expected");
                                                                                                                                                        is(sxe_cdb_tls_hkv_part.val[2] , 't', "Update same-sized value: update indirect char 3: got 't' as expected");

        sxe_cdb_ensemble_destroy(cdb_ensemble);
    }

    /* stress test cdb instance put/get/miss */

    putenv(SXE_CAST_NOCONST(char *, "SXE_LOG_LEVEL_LIBSXE_LIB_SXE_CDB=5")); /* Set to 5 to suppress sxe-cdb logging during test */
    sxe_log_control_forget_all_levels();

    {
        SXE_CDB_INSTANCE * cdb_instance = sxe_cdb_instance_new(0 /* grow from minimum size */, 0 /* grow to maximum allowed size */);
        SXE_CDB_UID        uid       ;
        SXE_CDB_UID        uids[keys];
        SXE_CDB_HKV      * hkv       ;

        start_time = sxe_time_get();
        for (i = 0; i < keys; i++) {
            sxe_cdb_prepare((const uint8_t *) &i, sizeof(i));
        }
        elapsed_time = sxe_time_to_double_seconds(sxe_time_get() - start_time);
        SXEL5("test: onlyhash: prepare %u keys in %6.2f seconds or %8u keys per second", keys, elapsed_time, (unsigned)(((uint64_t)keys << 32) / (sxe_time_get() - start_time)));

        start_time = sxe_time_get();
        for (i = 0; i < keys; i++) {
                                                         sxe_cdb_prepare         ((const uint8_t *) &i, sizeof(i));
                                      uids[i].as_u64.u = sxe_cdb_instance_put_val(cdb_instance, (const uint8_t *) &i, sizeof(i));
            SXEA1(SXE_CDB_UID_NONE != uids[i].as_u64.u, "ERROR: INTERNAL: sxe_cdb_instance_put_val() unexpectedly failing");
        }
        elapsed_time = sxe_time_to_double_seconds(sxe_time_get() - start_time);
        SXEL5("test: instance: put-val %u keys in %6.2f seconds or %8u keys per second with %u sheets_split & avg sheet keys during split %lu", keys, elapsed_time, (unsigned)(((uint64_t)keys << 32) / (sxe_time_get() - start_time)), cdb_instance->sheets_split, cdb_instance->sheets_split_keys / cdb_instance->sheets_split);

        start_time = sxe_time_get();
        for (i = 0; i < keys; i++) {
                  sxe_cdb_prepare             ((const uint8_t *) &i, sizeof(i));
            hkv = sxe_cdb_instance_get_hkv_raw(cdb_instance     ); SXEA1(hkv, "ERROR: INTERNAL: unexpected NULL hkv at i=%u", i);
            SXEA1(               sxe_cdb_tls_hkv_part.key_len  == sizeof(i) , "ERROR: INTERNAL: unexpected key len  at i=%u", i);
            SXEA1(               sxe_cdb_tls_hkv_part.val_len  == sizeof(i) , "ERROR: INTERNAL: unexpected val len  at i=%u", i);
            SXEA1(*((uint32_t *) sxe_cdb_tls_hkv_part.key    ) ==        i  , "ERROR: INTERNAL: unexpected key      at i=%u", i);
            SXEA1(*((uint32_t *) sxe_cdb_tls_hkv_part.val    ) ==        i  , "ERROR: INTERNAL: unexpected val      at i=%u", i);
        }
        elapsed_time = sxe_time_to_double_seconds(sxe_time_get() - start_time);
        SXEL5("test: instance: get-hkv %u keys in %6.2f seconds or %8u keys per second // keylen_misses %lu; memcmp_misses %lu", keys, elapsed_time, (unsigned)(((uint64_t)keys << 32) / (sxe_time_get() - start_time)), cdb_instance->keylen_misses, cdb_instance->memcmp_misses);

        start_time = sxe_time_get();
        for (i = 0; i < keys; i++) {
                           sxe_cdb_prepare         ((const uint8_t *) &i, sizeof(i));
            uid.as_u64.u = sxe_cdb_instance_get_uid(cdb_instance); SXEA1(uids[i].as_u64.u == uid.as_u64.u, "ERROR: INTERNAL: unexpected SXE_CDB_UID_NONE at i=%u", i);
        }
        elapsed_time = sxe_time_to_double_seconds(sxe_time_get() - start_time);
        SXEL5("test: instance: get-uid %u keys in %6.2f seconds or %8u keys per second // keylen_misses %lu; memcmp_misses %lu", keys, elapsed_time, (unsigned)(((uint64_t)keys << 32) / (sxe_time_get() - start_time)), cdb_instance->keylen_misses, cdb_instance->memcmp_misses);

        start_time = sxe_time_get();
        for (i = 0; i < keys; i++) {
            uid.as_u64.u = uids[i].as_u64.u;
            hkv          = sxe_cdb_instance_get_uid_hkv(cdb_instance, uid); SXEA1(hkv, "ERROR: INTERNAL: unexpected NULL hkv at i=%u for uid %010lx=ii[%04x]%03x-%01x", i, uid.as_u64.u, uid.as_part.sheets_index_index, uid.as_part.row, uid.as_part.cell);
            SXEA1(               sxe_cdb_tls_hkv_part.key_len  == sizeof(i)          , "ERROR: INTERNAL: unexpected key len  at i=%u for uid %010lx=ii[%04x]%03x-%01x", i, uid.as_u64.u, uid.as_part.sheets_index_index, uid.as_part.row, uid.as_part.cell);
            SXEA1(               sxe_cdb_tls_hkv_part.val_len  == sizeof(i)          , "ERROR: INTERNAL: unexpected val len  at i=%u for uid %010lx=ii[%04x]%03x-%01x", i, uid.as_u64.u, uid.as_part.sheets_index_index, uid.as_part.row, uid.as_part.cell);
            SXEA1(*((uint32_t *) sxe_cdb_tls_hkv_part.key    ) ==        i           , "ERROR: INTERNAL: unexpected key      at i=%u for uid %010lx=ii[%04x]%03x-%01x", i, uid.as_u64.u, uid.as_part.sheets_index_index, uid.as_part.row, uid.as_part.cell);
            SXEA1(*((uint32_t *) sxe_cdb_tls_hkv_part.val    ) ==        i           , "ERROR: INTERNAL: unexpected val      at i=%u for uid %010lx=ii[%04x]%03x-%01x", i, uid.as_u64.u, uid.as_part.sheets_index_index, uid.as_part.row, uid.as_part.cell);
        }
        elapsed_time = sxe_time_to_double_seconds(sxe_time_get() - start_time);
        SXEL5("test: instance: tls-hkv %u keys in %6.2f seconds or %8u keys per second // keylen_misses %lu; memcmp_misses %lu", keys, elapsed_time, (unsigned)(((uint64_t)keys << 32) / (sxe_time_get() - start_time)), cdb_instance->keylen_misses, cdb_instance->memcmp_misses);

        start_time = sxe_time_get();
        for (i = keys; i < (keys * 2); i++) {
                           sxe_cdb_prepare         ((const uint8_t *) &i, sizeof(i));
            uid.as_u64.u = sxe_cdb_instance_get_uid(cdb_instance     ); SXEA1(SXE_CDB_UID_NONE == uid.as_u64.u, "ERROR: INTERNAL: unexpected existing uid at i=%u", i);
        }
        elapsed_time = sxe_time_to_double_seconds(sxe_time_get() - start_time);
        SXEL5("test: instance:  no-uid %u keys in %6.2f seconds or %8u keys per second // keylen_misses %lu; memcmp_misses %lu", keys, elapsed_time, (unsigned)(((uint64_t)keys << 32) / (sxe_time_get() - start_time)), cdb_instance->keylen_misses, cdb_instance->memcmp_misses);

        start_time = sxe_time_get();
        for (i = keys; i < (keys * 2); i++) {
                  sxe_cdb_prepare             ((const uint8_t *) &i, sizeof(i));
            hkv = sxe_cdb_instance_get_hkv_raw(cdb_instance    ); SXEA1(NULL == hkv, "ERROR: INTERNAL: unexpected existing hkv at i=%u", i);
        }
        elapsed_time = sxe_time_to_double_seconds(sxe_time_get() - start_time);
        SXEL5("test: instance:  no-hkv %u keys in %6.2f seconds or %8u keys per second // keylen_misses %lu; memcmp_misses %lu", keys, elapsed_time, (unsigned)(((uint64_t)keys << 32) / (sxe_time_get() - start_time)), cdb_instance->keylen_misses, cdb_instance->memcmp_misses);

        SXEL5("test: instance: %.1fMB total memory in sheets, %.1fMB total memory in kvdata; cells used %u, size %u or %u%% full; kv used %u, size %u",
              cdb_instance->sheets_size * SXE_CDB_SHEET_BYTES / 1024 / 1024.0,
              cdb_instance->kvdata_used                       / 1024 / 1024.0,
              cdb_instance->sheets_cells_used,
              cdb_instance->sheets_cells_size,
              cdb_instance->sheets_cells_used * 100 / cdb_instance->sheets_cells_size,
              cdb_instance->kvdata_used,
              cdb_instance->kvdata_size);

        sxe_cdb_instance_destroy(cdb_instance);
    }

    /* stress test cdb instance inc */

    {
        SXE_CDB_INSTANCE * cdb_instance = sxe_cdb_instance_new(0 /* grow from minimum size */, 0 /* grow to maximum allowed size */);
        uint32_t           counts_list  = 0;
        uint64_t           manual_count[keys];
        uint64_t           manual_count_hi = 0;

        start_time = sxe_time_get();
        for (i = 0; i < keys; i++) {
            manual_count[i] = 1;
                                     sxe_cdb_prepare     ((const uint8_t *) &i, sizeof(i));
            SXEA1(manual_count[i] == sxe_cdb_instance_inc(cdb_instance, counts_list      ), "ERROR: INTERNAL: sxe_cdb_instance_inc() unexpectedly failing");
        }
        elapsed_time = sxe_time_to_double_seconds(sxe_time_get() - start_time);
        SXEL5("test: instance: inc-1st %u keys in %6.2f seconds or %8u keys per second with %u sheets_split & avg sheet keys during split %lu, ->counts_used=%u", keys, elapsed_time, (unsigned)(((uint64_t)keys << 32) / (sxe_time_get() - start_time)), cdb_instance->sheets_split, cdb_instance->sheets_split_keys / cdb_instance->sheets_split, cdb_instance->counts_used);

        start_time = sxe_time_get();
        for (i = 0; i < keys; i++) {
            manual_count[i] ++;
                                     sxe_cdb_prepare     ((const uint8_t *) &i, sizeof(i));
            SXEA1(manual_count[i] == sxe_cdb_instance_inc(cdb_instance, counts_list      ), "ERROR: INTERNAL: sxe_cdb_instance_inc() unexpectedly failing");
        }
        elapsed_time = sxe_time_to_double_seconds(sxe_time_get() - start_time);
        SXEL5("test: instance: inc-2nd %u keys in %6.2f seconds or %8u keys per second with %u sheets_split & avg sheet keys during split %lu, ->counts_used=%u", keys, elapsed_time, (unsigned)(((uint64_t)keys << 32) / (sxe_time_get() - start_time)), cdb_instance->sheets_split, cdb_instance->sheets_split_keys / cdb_instance->sheets_split, cdb_instance->counts_used);

        start_time = sxe_time_get();
        for (i = 0; i < keys; i++) {
            uint32_t myhash[4];
            MurmurHash3_xnn_128((const uint8_t *) &i, sizeof(i), 98765, &myhash[0]);
            uint32_t myrandom = myhash[0] % (keys / 4096 * 2); /* incrementing deterministically smaller number of random keys will cause ->counts mremap() (and will cause code to run faster due to cache line re-usage) */
                  manual_count[myrandom] ++;
                                            sxe_cdb_prepare     ((const uint8_t *) &myrandom, sizeof(myrandom));
            SXEA1(manual_count[myrandom] == sxe_cdb_instance_inc(cdb_instance, counts_list                    ), "ERROR: INTERNAL: sxe_cdb_instance_inc() unexpectedly failing");
            manual_count_hi = manual_count[myrandom] > manual_count_hi ? manual_count[myrandom] : manual_count_hi;
        }
        elapsed_time = sxe_time_to_double_seconds(sxe_time_get() - start_time);
        SXEL5("test: instance: inc-rnd %u keys in %6.2f seconds or %8u keys per second with %u sheets_split & avg sheet keys during split %lu, ->counts_used=%u", keys, elapsed_time, (unsigned)(((uint64_t)keys << 32) / (sxe_time_get() - start_time)), cdb_instance->sheets_split, cdb_instance->sheets_split_keys / cdb_instance->sheets_split, cdb_instance->counts_used);

        sxe_cdb_tls_walk_cnt_pos = SXE_CDB_COUNT_NONE;
        for (i = 0; i < 5; i++) {
            sxe_cdb_instance_walk(cdb_instance, 1 /* hi2lo */, sxe_cdb_tls_walk_cnt_pos, sxe_cdb_tls_walk_hkv_pos, counts_list);
            if (0 == i) {
                is(manual_count_hi, sxe_cdb_tls_walk_count, "test: just for fun top key: has expected count");
            }
            SXEL5("test: just for fun top key #%u: binary key=count; %08x=%lu", i, *((uint32_t *) sxe_cdb_tls_hkv_part.key), sxe_cdb_tls_walk_count);
        }

        SXEL5("test: instance: %.1fMB total memory in sheets, %.1fMB total memory in kvdata; cells used %u, size %u or %u%% full; kv used %u, size %u",
              cdb_instance->sheets_size * SXE_CDB_SHEET_BYTES / 1024 / 1024.0,
              cdb_instance->kvdata_used                       / 1024 / 1024.0,
              cdb_instance->sheets_cells_used,
              cdb_instance->sheets_cells_size,
              cdb_instance->sheets_cells_used * 100 / cdb_instance->sheets_cells_size,
              cdb_instance->kvdata_used,
              cdb_instance->kvdata_size);

        sxe_cdb_instance_destroy(cdb_instance);
    }

    //sxe_cdb_debug_validate(cdb, "final validate");

    /* tests for maximum size */

    SXE_CDB_INSTANCE * cdb_instance_with_limit = sxe_cdb_instance_new(0 /* grow from minimum size */, 99999 /* grow to max. 10000 bytes */);

       sxe_cdb_prepare         (&header_len_5_key[0], sizeof(header_len_5_key));
    ok(sxe_cdb_instance_put_val(cdb_instance_with_limit, NULL, 0) != SXE_CDB_UID_NONE, "append header_len_5_key as expected");
    ok(sxe_cdb_instance_put_val(cdb_instance_with_limit, NULL, 0) != SXE_CDB_UID_NONE, "append header_len_5_key as expected // straddles     kvdata limit");
    is(sxe_cdb_instance_put_val(cdb_instance_with_limit, NULL, 0) ,  SXE_CDB_UID_NONE, "append header_len_5_key as expected // failed due to kvdata limit");

    /* tests for ensemble functionality */

    //debug putenv(SXE_CAST_NOCONST(char *, "SXE_LOG_LEVEL_LIBSXE_LIB_SXE_CDB=7")); /* Set to 5 to suppress sxe-cdb logging during test */
    //debug sxe_log_control_forget_all_levels();

    {
        uint32_t           instances  = 8;
        SXE_CDB_ENSEMBLE * cdb_ensemble = sxe_cdb_ensemble_new(0 /* grow from minimum size */, 0 /* grow to maximum allowed size */, instances /* number of cdb instances */, 1 /* locked */);
        SXE_CDB_UID        uid       ;
        SXE_CDB_UID        uids[keys];
        SXE_CDB_HKV      * hkv       ;

        start_time = sxe_time_get();
        for (i = 0; i < keys; i++) {
                                                         sxe_cdb_prepare         ((const uint8_t *) &i, sizeof(i));
                                      uids[i].as_u64.u = sxe_cdb_ensemble_put_val(cdb_ensemble, (const uint8_t *) &i, sizeof(i));
            SXEA1(SXE_CDB_UID_NONE != uids[i].as_u64.u, "ERROR: INTERNAL: sxe_cdb_ensemble_put_val() unexpectedly failing");
        }
        elapsed_time = sxe_time_to_double_seconds(sxe_time_get() - start_time);
        SXEL5("test: ensemble: put-val %u keys in %6.2f seconds or %8u keys per second", keys, elapsed_time, (unsigned)(((uint64_t)keys << 32) / (sxe_time_get() - start_time)));

        start_time = sxe_time_get();
        for (i = 0; i < keys; i++) {
                           sxe_cdb_prepare         ((const uint8_t *) &i, sizeof(i));
            uid.as_u64.u = sxe_cdb_ensemble_get_uid(cdb_ensemble); SXEA1(uids[i].as_u64.u == uid.as_u64.u, "ERROR: INTERNAL: unexpected SXE_CDB_UID_NONE at i=%u", i);
        }
        elapsed_time = sxe_time_to_double_seconds(sxe_time_get() - start_time);
        SXEL5("test: ensemble: get-uid %u keys in %6.2f seconds or %8u keys per second", keys, elapsed_time, (unsigned)(((uint64_t)keys << 32) / (sxe_time_get() - start_time)));

        start_time = sxe_time_get();
        for (i = 0; i < keys; i++) {
            uid.as_u64.u = uids[i].as_u64.u;
            hkv          = sxe_cdb_ensemble_get_uid_hkv(cdb_ensemble, uid); SXEA1(hkv, "ERROR: INTERNAL: unexpected NULL hkv at i=%u for uid %010lx=ii[%04x]%03x-%01x", i, uid.as_u64.u, uid.as_part.sheets_index_index, uid.as_part.row, uid.as_part.cell);
            SXEA1(               sxe_cdb_tls_hkv_part.key_len  == sizeof(i)          , "ERROR: INTERNAL: unexpected key len  at i=%u for uid %010lx=ii[%04x]%03x-%01x", i, uid.as_u64.u, uid.as_part.sheets_index_index, uid.as_part.row, uid.as_part.cell);
            SXEA1(               sxe_cdb_tls_hkv_part.val_len  == sizeof(i)          , "ERROR: INTERNAL: unexpected val len  at i=%u for uid %010lx=ii[%04x]%03x-%01x", i, uid.as_u64.u, uid.as_part.sheets_index_index, uid.as_part.row, uid.as_part.cell);
            SXEA1(*((uint32_t *) sxe_cdb_tls_hkv_part.key    ) ==        i           , "ERROR: INTERNAL: unexpected key      at i=%u for uid %010lx=ii[%04x]%03x-%01x", i, uid.as_u64.u, uid.as_part.sheets_index_index, uid.as_part.row, uid.as_part.cell);
            SXEA1(*((uint32_t *) sxe_cdb_tls_hkv_part.val    ) ==        i           , "ERROR: INTERNAL: unexpected val      at i=%u for uid %010lx=ii[%04x]%03x-%01x", i, uid.as_u64.u, uid.as_part.sheets_index_index, uid.as_part.row, uid.as_part.cell);
        }
        elapsed_time = sxe_time_to_double_seconds(sxe_time_get() - start_time);
        SXEL5("test: ensemble: tls-hkv %u keys in %6.2f seconds or %8u keys per second", keys, elapsed_time, (unsigned)(((uint64_t)keys << 32) / (sxe_time_get() - start_time)));

        for (i = 0; i < instances; i++) {
            SXEL5("test: ensemble: %.1fMB total memory in sheets, %.1fMB total memory in kvdata; cells used %u, size %u or %u%% full; kv used %u, size %u; instance %u, splits %u",
                  cdb_ensemble->cdb_instances[i]->sheets_size * SXE_CDB_SHEET_BYTES / 1024 / 1024.0,
                  cdb_ensemble->cdb_instances[i]->kvdata_used                       / 1024 / 1024.0,
                  cdb_ensemble->cdb_instances[i]->sheets_cells_used,
                  cdb_ensemble->cdb_instances[i]->sheets_cells_size,
                  cdb_ensemble->cdb_instances[i]->sheets_cells_used * 100 / cdb_ensemble->cdb_instances[i]->sheets_cells_size,
                  sxe_cdb_ensemble_kvdata_used(cdb_ensemble, i),
                  cdb_ensemble->cdb_instances[i]->kvdata_size,
                  i,
                  cdb_ensemble->cdb_instances[i]->sheets_split);
        }

        sxe_cdb_ensemble_destroy(cdb_ensemble);
    }

    /* stress test cdb ensemble inc */

    {
        uint32_t           instances  = 8;
        SXE_CDB_ENSEMBLE * cdb_ensemble = sxe_cdb_ensemble_new(0 /* grow from minimum size */, 0 /* grow to maximum allowed size */, instances /* number of cdb instances */, 1 /* locked */);
        uint32_t           counts_list = 0;
        uint64_t           manual_count[keys];

        start_time = sxe_time_get();
        for (i = 0; i < keys; i++) {
                 manual_count[i] = 1;
                                     sxe_cdb_prepare     ((const uint8_t *) &i, sizeof(i));
            SXEA1(manual_count[i] == sxe_cdb_ensemble_inc(cdb_ensemble, counts_list      ), "ERROR: INTERNAL: sxe_cdb_ensemble_inc() unexpectedly failing");
        }
        elapsed_time = sxe_time_to_double_seconds(sxe_time_get() - start_time);
        SXEL5("test: ensemble: inc-1st %u keys in %6.2f seconds or %8u keys per second", keys, elapsed_time, (unsigned)(((uint64_t)keys << 32) / (sxe_time_get() - start_time)));

        start_time = sxe_time_get();
        for (i = 0; i < keys; i++) {
                  manual_count[i] ++;
                                     sxe_cdb_prepare     ((const uint8_t *) &i, sizeof(i));
            SXEA1(manual_count[i] == sxe_cdb_ensemble_inc(cdb_ensemble, counts_list      ), "ERROR: INTERNAL: sxe_cdb_ensemble_inc() unexpectedly failing");
        }
        elapsed_time = sxe_time_to_double_seconds(sxe_time_get() - start_time);
        SXEL5("test: ensemble: inc-2nd %u keys in %6.2f seconds or %8u keys per second", keys, elapsed_time, (unsigned)(((uint64_t)keys << 32) / (sxe_time_get() - start_time)));

        start_time = sxe_time_get();
        for (i = 0; i < keys; i++) {
            uint32_t myhash[4];
            MurmurHash3_xnn_128((const uint8_t *) &i, sizeof(i), 98765, &myhash[0]);
            uint32_t myrandom = myhash[0] % (keys / 4096 * 2); /* incrementing deterministically smaller number of random keys will cause ->counts mremap() (and will cause code to run faster due to cache line re-usage) */
                  manual_count[myrandom] ++;
                                            sxe_cdb_prepare     ((const uint8_t *) &myrandom, sizeof(myrandom));
            SXEA1(manual_count[myrandom] == sxe_cdb_ensemble_inc(cdb_ensemble, counts_list                    ), "ERROR: INTERNAL: sxe_cdb_ensemble_inc() unexpectedly failing");
        }
        elapsed_time = sxe_time_to_double_seconds(sxe_time_get() - start_time);
        SXEL5("test: ensemble: inc-rnd %u keys in %6.2f seconds or %8u keys per second", keys, elapsed_time, (unsigned)(((uint64_t)keys << 32) / (sxe_time_get() - start_time)));

        uint32_t instance;
        for (instance = 0; instance < 8; instance++) {
            sxe_cdb_tls_walk_cnt_pos = SXE_CDB_COUNT_NONE;
            for (i = 0; i < 2; i++) {
                sxe_cdb_ensemble_walk(cdb_ensemble, 1 /* hi2lo */, sxe_cdb_tls_walk_cnt_pos, sxe_cdb_tls_walk_hkv_pos, instance, counts_list);
                SXEL5("test: just for fun: instance #%u: top key #%u: binary key=count; %08x=%lu", instance, i, *((uint32_t *) sxe_cdb_tls_hkv_part.key), sxe_cdb_tls_walk_count);
            }
        }

        for (i = 0; i < instances; i++) {
            SXEL5("test: ensemble: %.1fMB total memory in sheets, %.1fMB total memory in kvdata; cells used %u, size %u or %u%% full; kv used %u, size %u; instance %u, splits %u",
                  cdb_ensemble->cdb_instances[i]->sheets_size * SXE_CDB_SHEET_BYTES / 1024 / 1024.0,
                  cdb_ensemble->cdb_instances[i]->kvdata_used                       / 1024 / 1024.0,
                  cdb_ensemble->cdb_instances[i]->sheets_cells_used,
                  cdb_ensemble->cdb_instances[i]->sheets_cells_size,
                  cdb_ensemble->cdb_instances[i]->sheets_cells_used * 100 / cdb_ensemble->cdb_instances[i]->sheets_cells_size,
                  cdb_ensemble->cdb_instances[i]->kvdata_used,
                  cdb_ensemble->cdb_instances[i]->kvdata_size,
                  i,
                  cdb_ensemble->cdb_instances[i]->sheets_split);
        }

        sxe_cdb_ensemble_destroy(cdb_ensemble);
    }

    return exit_status();
} /* main() */

