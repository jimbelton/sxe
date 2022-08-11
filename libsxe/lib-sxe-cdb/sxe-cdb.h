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

#ifndef __SXE_CDB_H__
#define __SXE_CDB_H__

/**
 * - What is sxe-cdb? sxe-cdb tries to have similar advantages
 *   to cdb "constant database" but with less disadvantages.
 *   Differences:
 *   - cdb has two distinct phases; creation & reading. sxe-cdb
 *     merges these phases; keys can be appended during the
 *     reading phase.
 *     - Both the hash table index & the key,value memory areas
 *       are mremap()able meaning that e.g. n 100 million keys
 *       can be serialized quickly to disk by saving 2 blocks of
 *       memory rather than iterating over n 100 million keys.
 *   - cdb is very fast but algorithm is stone-age. sxe-cdb is
 *     designed to take advantage of the cache-line-based
 *     architecture of modern CPUs.
 *     - The hash table index starts off small -- about 512KB
 *       per cdb instance -- and expands in size gracefully
 *       without ever having to rehash all keys; there is never
 *       any rehashing, although hash table indexes may be moved
 *       around but this is limited to 64k indexes in contiguous
 *       cache-line memory and is therefore very fast.
 *   - cdb has a 4GB size limitation. sxe-cdb has the same
 *     limitation -- in order to keep the data structures
 *     compact -- but multiple sxe-cdb instances can be used in
 *     parallel to achieve any multiple of 4GB, e.g. 128GB.
 *     sxe-cdb decides which instance to put a unique key in.
 *     - sxe-cdb allows individual instances to be locked,
 *       meaning that in a multi-threaded environment there is
 *       less lock contention than if using one big lock.
 *   - cdb is a key,value database. sxe-cdb has an instant sort
 *     feature where sxe_cdb_instance_inc() counters get
 *     automatically sorted on the fly and the only over-head is
 *     memory, not CPU.
 */

/*
 * How it works:
 *
 *          +--+ <-- cdb_instance->sheet_index[<65536]
 *          |  |     Initially, all sheet indexes are zero.
 *          |  |     Min/max size is 128KB/128KB.
 *          +--+
 *          |  |
 *          |  |
 *          +--+
 *          :  :
 *          +--+
 *          |  |
 *          |  |
 *          +--+
 *
 * +--+--+..+--+ <-- cdb_instance->sheets[<65536].row[<4096].hkv_pos.u32[<16]
 * |  |  |  |  |     Grows by 512 KB via mremap() after sxe_cdb_split_sheet().
 * |  |  |  |  |     sxe_cdb_split_sheet() called upon all row cells used.
 * +--+--+..+--+     sxe_cdb_instance_put() inserts in row a or b with fewest cells.
 * |  |  |  |  |     Key overhead is 8 bytes.
 * |  |  |  |  |     Min/max size is 512KB/32GB.
 * +--+--+..+--+
 * :  :  :  :  :
 * +--+--+..+--+
 * |  |  |  |  |
 * |  |  |  |  |
 * +------..---+
 *
 * +--+--+..+--+ <-- cdb_instance->kvdata[<2^32]
 * |  |  |  |  |     Grows by n*4 KB via mremap() after sxe_cdb_instance_put().
 * |  |  |  |  |     Key/value length header is 1, 3, 5, or 8 bytes.
 * +--+--+..+--+     Min/max size is 4KB/4GB.
 *
 * +--+--+..+--+ <-- cdb_instance->counts[<2^32]
 * |  |  |  |  |     Grows by 4 KB via mremap() after sxe_cdb_instance_inc().
 * |  |  |  |  |     Min/max size is 0KB/4GB.
 * +--+--+..+--+
 *
 * Note: Delete all key/values by deleting the 3 contiguous memory blocks.
 * Note: Save   all key/values by saving   the 3 contiguous memory blocks.
 * Note: Load   all key/values by loading  the 3 contiguous memory blocks.
 * Note: 28M 4 byte key/values = 511 splits, 256MB sheet & 240MB kvdata.
 *
 * - Important characteristics:
 *   - row/cell never changes; not even for sheet split.
 *   - sheet index index never changes; although sheet index will change.
 *   - <1 byte instance><2 byte sheet index index><2 byte row/cell>:
 *     - Is a short UID to any of 1 trillion hkv (header, key, value).
 *     - Will always reference the hkv even if mremap() moves instance base.
 *   - The cost of using a uid as key counter is:
 *     - kvdata: 18 bytes: <1 byte header><5 byte uid><12 byte SXE_CDB_HKV_LIST>
 *     - counts: 18 bytes:                            <12 byte SXE_CDB_COUNT>
 */

/**
 * - FAQ:
 *   - Can I reference an hkv by its 64bit memory location? No,
 *     because the ->kvdata can get mremap()ed at any time.
 *   - If I store a key then is there an alternative (shorter?)
 *     way to reference it -- e.g. in other keys and values, or
 *     within the data structures of the caller -- rather than
 *     use the key bytes again? Yes, by using the 5 byte UID of
 *     the key.
 *     - Example of common value:
 *       - common-value-1 = very-very-long-value
 *       - key-1 = <uid>common-value-1
 *       - key-2 = <uid>common-value-1
 *     - Example of hash in hash (is key in sub-hash?):
 *       - domain.one = val
 *       - domain.two = val
 *       - domain.three = val
 *       - unique-sub-hash-1<uid>domain.one = val
 *       - unique-sub-hash-1<uid>domain.two = val
 *       - unique-sub-hash-2<uid>domain.two = val
 *       - unique-sub-hash-2<uid>domain.three = val
 *   - Can I call sxe_cdb_*_walk() iteratively while calling
 *     sxe_cdb_*_inc() in between? No, otherwise the walk
 *     results will be inaccurate.
 *   - What happens if I sxe_cdb_*_walk() with the wrong cnt_pos
 *     and/or hkv_pos? sxe_cdb_instance_walk_pos_is_bad() will
 *     hopefully detect this :-)
 *   - Can I store a key without a value? Yes.
 */

/**
 * - TO DO:
 *   - Todo: sxe_cdb_instance_update_val().
 *   - Todo: sxe_cdb_instance_del().
 *     - Mark keys as deleted somehow.
 *     - Consider e.g. sxe_instance_compact().
 *     - Consider auto compact on granularized ->kvdata.
 *   - Todo: sxe_cdb_instance_(push|unshift)().
 *     - E.g. similar to Perl's push(@{$hash->{key}}, "value");
 *     - Consider how to sxe_cdb_instance_walk() array.
 *     - E.g. useful for replacing text log with sxe cdb?
 */

typedef struct SXE_CDB_INSTANCE SXE_CDB_INSTANCE;
typedef struct SXE_CDB_ENSEMBLE SXE_CDB_ENSEMBLE;
typedef union  SXE_CDB_HKV      SXE_CDB_HKV     ;

#ifndef SXE_CDB_UID_INTERNAL
#define SXE_CDB_UID_INTERNAL
#endif

typedef union SXE_CDB_UID {
    SXE_CDB_UID_INTERNAL;
    struct { uint64_t u : 32; /* 4 bytes; instance level */ } __attribute__((packed)) as_u32;
    struct { uint64_t u : 40; /* 5 bytes; ensemble level */ } __attribute__((packed)) as_u40;
    struct { uint64_t u     ; /* 8 bytes; c/api    level */ } __attribute__((packed)) as_u64;
} SXE_CDB_UID;

typedef struct SXE_CDB_HKV_PART {
    uint32_t   hkv_len;
    uint8_t  * key    ;
    uint32_t   key_len;
    uint8_t  * val    ;
    uint32_t   val_len;
} __attribute__((packed)) SXE_CDB_HKV_PART;

typedef union SXE_CDB_HASH {
    uint64_t u64[2];
    uint32_t u32[4];
    uint16_t u16[8];
    uint8_t  u08[16];
} __attribute__((packed)) SXE_CDB_HASH;


#define SXE_CDB_COUNT_NONE   0
#define SXE_CDB_HKV_POS_NONE 0
#define SXE_CDB_UID_NONE     UINT64_MAX

/**
 * - With the exception of sxe_cdb_instance_get_hkv() then all
 *   functions returning an hkv pointer actually return a
 *   pointer to a tls copy of the hkv.
 *   - Why? Because if we returned an hkv pointer to the real
 *     hkv then we cannot guarantee that it won't change in the
 *     future due to an mremap().
 *   - Why not return the hkv copy via malloc()? Because
 *     performance tests showed that malloc() is about 50%
 *     slower than using tls.
 */

extern __thread uint32_t           sxe_cdb_tls_hkv_len_max  ; /* tls: realloc()ed buffer current size */
extern __thread SXE_CDB_HKV      * sxe_cdb_tls_hkv          ; /* tls: realloc()ed buffer used for copying hkv into for caller */
extern __thread SXE_CDB_HKV_PART   sxe_cdb_tls_hkv_part     ; /* tls: realloc()ed buffer parts */
extern __thread uint32_t           sxe_cdb_tls_walk_cnt_pos ; /* tls: walk: SXE_CDB_COUNT_NONE   means start of list, or current index for count */
extern __thread uint32_t           sxe_cdb_tls_walk_hkv_pos ; /* tls: walk: SXE_CDB_HKV_POS_NONE means start of list, or current hkv   for count */
extern __thread uint64_t           sxe_cdb_tls_walk_count   ; /* tls: walk: 64bit count of key just walked to */
extern __thread SXE_CDB_HASH       sxe_cdb_hash             ; /* tls: hash after last sxe_cdb_prepare() */

#include "lib-sxe-cdb-proto.h"

#endif /* __SXE_CDB_H__ */

