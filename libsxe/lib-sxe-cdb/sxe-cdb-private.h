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

#include <stdint.h> /* defines uint32_t etc */

#include "sxe-spinlock.h"

#define SXE_CDB_KERNEL_PAGE_BYTES (4096)
#define SXE_CDB_CACHE_LINE_BYTES  (64) /* todo: calculate the cache line size at runtime! */
#define SXE_CDB_CACHE_LINE_FACTOR (2)  /* figure out why this is faster than 1 or 4 :-) */
#define SXE_CDB_KEYS_PER_ROW      ((SXE_CDB_CACHE_LINE_BYTES / SXE_CDB_CACHE_LINE_FACTOR) / sizeof(uint16_t))
#define SXE_CDB_KEYS_PER_ROW_BITS (4)

typedef struct SXE_CDB_ROW_KEY__HASH_LO {
    uint16_t u16[SXE_CDB_KEYS_PER_ROW];
} __attribute__((packed)) SXE_CDB_ROW_KEY__HASH_LO;

typedef struct SXE_CDB_ROW_KEY__HASH_HI {
    uint16_t u16[SXE_CDB_KEYS_PER_ROW];
} __attribute__((packed)) SXE_CDB_ROW_KEY__HASH_HI;

typedef struct SXE_CDB_ROW_KEY_POSITION {
    uint32_t u32[SXE_CDB_KEYS_PER_ROW];
} SXE_CDB_ROW_KEY_POSITION;

typedef struct SXE_CDB_ROW {
    SXE_CDB_ROW_KEY__HASH_LO hash_lo; /* hash.u16[1]; 1 in 2048(=65536/32) chance of correct key */
    SXE_CDB_ROW_KEY__HASH_HI hash_hi; /* hash.u16[0]; used to calculate sheet */
    SXE_CDB_ROW_KEY_POSITION hkv_pos; /* 0 = cell not used */
} __attribute__((packed)) SXE_CDB_ROW;

#define SXE_CDB_ROW_BYTES           (sizeof(SXE_CDB_ROW))                              /*   256 bytes per row   */
#define SXE_CDB_SHEET_BYTES         (1<<19)                                            /*   512 KB    per sheet */
#define SXE_CDB_ROWS_PER_SHEET      (SXE_CDB_SHEET_BYTES / SXE_CDB_ROW_BYTES)          /*  2048 rows  per sheet */
#define SXE_CDB_ROWS_PER_SHEET_BITS (12)
#define SXE_CDB_KEYS_PER_SHEET      (SXE_CDB_ROWS_PER_SHEET * SXE_CDB_KEYS_PER_ROW)    /* 65536 keys  per sheet */
#define SXE_CDB_SHEETS_MAX          ((1<<30) / 8 / SXE_CDB_KEYS_PER_SHEET * 4)         /*  8192 sheet indexes   */
#define SXE_CDB_COUNTS_LISTS_MAX    (256)                                              /*   256 count lists */

/**
 * - SXE_CDB_SHEETS_MAX explanation:
 *   - Assuming smallest size for header + key + value is 8
 *     bytes, and each sheet holds a finite (e.g. 65536) number
 *     of keys, and the largest kvdata is 4GB (due to the 32bit
 *     offset), then the most sheets we'll need is 2^32 / 8 /
 *     SXE_CDB_KEYS_PER_SHEET or 8192 sheets.
 *   - This means that the most keys we can store in a 4GB
 *     kvdata address space is 2^32 / 8 or 536,870,912 keys.
 */

typedef struct SXE_CDB_SHEET {
    SXE_CDB_ROW row[SXE_CDB_ROWS_PER_SHEET];
} __attribute__((packed)) SXE_CDB_SHEET;

#define KEY_HEADER_LEN_1_KEY_BITS  3
#define KEY_HEADER_LEN_3_KEY_BITS  7
#define KEY_HEADER_LEN_5_KEY_BITS 16
#define KEY_HEADER_LEN_8_KEY_BITS 24

#define KEY_HEADER_LEN_1_VAL_BITS  4
#define KEY_HEADER_LEN_3_VAL_BITS 16
#define KEY_HEADER_LEN_5_VAL_BITS 16
#define KEY_HEADER_LEN_8_VAL_BITS 32

//why does gcc suck so badly? #define KEY_HEADER_LEN_8_VAL_LEN_MAX ((1<<KEY_HEADER_LEN_8_VAL_BITS)-1)

#define KEY_HEADER_LEN_1_KEY_LEN_MAX (unsigned)((((1<<(KEY_HEADER_LEN_1_KEY_BITS-2))-1)<<2)+3)
#define KEY_HEADER_LEN_3_KEY_LEN_MAX (unsigned)((((1<<(KEY_HEADER_LEN_3_KEY_BITS-2))-1)<<2)+3)
#define KEY_HEADER_LEN_5_KEY_LEN_MAX (unsigned)((((1<<(KEY_HEADER_LEN_5_KEY_BITS-2))-1)<<2)+3)
#define KEY_HEADER_LEN_8_KEY_LEN_MAX (unsigned)((((1<<(KEY_HEADER_LEN_8_KEY_BITS-2))-1)<<2)+3)

#define KEY_HEADER_LEN_1_VAL_LEN_MAX (unsigned)((((1<<(KEY_HEADER_LEN_1_VAL_BITS-2))-1)<<2)+3)
#define KEY_HEADER_LEN_3_VAL_LEN_MAX (unsigned)((((1<<(KEY_HEADER_LEN_3_VAL_BITS-2))-1)<<2)+3)
#define KEY_HEADER_LEN_5_VAL_LEN_MAX (unsigned)((((1<<(KEY_HEADER_LEN_5_VAL_BITS-2))-1)<<2)+3)
#define KEY_HEADER_LEN_8_VAL_LEN_MAX (unsigned)((((1<<(KEY_HEADER_LEN_8_VAL_BITS-2))-1)<<2)+3) /* e.g. 0xffffffff */

union SXE_CDB_HKV {
    struct {
        uint8_t  flag    :                         1; /* 0 means use this struct!  */
        uint8_t  key_len : KEY_HEADER_LEN_1_KEY_BITS; /* 0 means use 32bit lengths */
        uint8_t  val_len : KEY_HEADER_LEN_1_VAL_BITS; /* 0 means use 32bit lengths */
        uint8_t  content[0]                         ; /*   key followed by val     */
    } __attribute__((packed)) header_len_1;
    struct {
        uint8_t  flag    :                         1; /* 1 means use this struct!  */
        uint8_t  key_len : KEY_HEADER_LEN_3_KEY_BITS;
        uint16_t val_len : KEY_HEADER_LEN_3_VAL_BITS;
        uint8_t  content[0]                         ; /*   key followed by val     */
    } __attribute__((packed)) header_len_3;
    struct {
        uint8_t  flag    :                         1; /* 0 means use this struct!  */
        uint8_t  xxx_len : KEY_HEADER_LEN_1_KEY_BITS; /* 0 means use this struct!  */
        uint8_t  yyy_len : KEY_HEADER_LEN_1_VAL_BITS; /* 0 means use this struct!  */
        uint32_t key_len : KEY_HEADER_LEN_5_KEY_BITS;
        uint32_t val_len : KEY_HEADER_LEN_5_VAL_BITS;
        uint8_t  content[0]                         ; /*   key followed by val     */
    } __attribute__((packed)) header_len_5;
    struct {
        uint8_t  flag    :                         1; /* 0 means use this struct!  */
        uint8_t  xxx_len : KEY_HEADER_LEN_1_KEY_BITS; /* 0 means use this struct!  */
        uint8_t  yyy_len : KEY_HEADER_LEN_1_VAL_BITS; /* 1 means use this struct!  */
        uint32_t key_len : KEY_HEADER_LEN_8_KEY_BITS;
        uint32_t val_len : KEY_HEADER_LEN_8_VAL_BITS;
        uint8_t  content[0]                         ; /*   key followed by val     */
    } __attribute__((packed)) header_len_8;
} __attribute__((packed));

#define SXE_CDB_UID_INTERNAL                                                         \
    struct {                                                                         \
        uint64_t instance           :  8                         ; /* 1 bytes     */ \
        uint64_t sheets_index_index : 16                         ; /* 2 bytes     */ \
        uint64_t row                : SXE_CDB_ROWS_PER_SHEET_BITS; /* 2 bytes     */ \
        uint64_t cell               : SXE_CDB_KEYS_PER_ROW_BITS  ; /*   row+cell  */ \
    } __attribute__((packed)) as_part;

typedef struct SXE_CDB_COUNT {
    uint64_t count : 48; /* 6 bytes; unique count that key(s) have reached    */
    uint32_t next      ; /* 4 bytes; next higher count in list                */
    uint32_t last      ; /* 4 bytes; last lower  count in list                */
    uint32_t hkv1      ; /* 4 bytes; first hkv in list with this unique count */
} __attribute__((packed)) SXE_CDB_COUNT;

typedef struct SXE_CDB_HKV_LIST {
    uint32_t count_to_use; /* SXE_CDB_COUNT index to ->counts[] */
    uint32_t next_hkv_pos; /* next hkv pos at same count */
    uint32_t last_hkv_pos; /* last hkv pos at same count */
} __attribute__((packed)) SXE_CDB_HKV_LIST;

#define SXE_CDB_COUNT_BYTES    (sizeof(SXE_CDB_COUNT))
#define SXE_CDB_HKV_LIST_BYTES (sizeof(SXE_CDB_HKV_LIST))

// SXE_CDB_CACHE_LINE_BYTES : 64
// SXE_CDB_ROW_BYTES        : 128
// SXE_CDB_KEYS_PER_ROW     : 16
// SXE_CDB_SHEET_BYTES      : 524288
// SXE_CDB_ROWS_PER_SHEET   : 4096
// SXE_CDB_KEYS_PER_SHEET   : 65536
// SXE_CDB_SHEETS_MAX       : 8192
// SXE_CDB_KERNEL_PAGE_BYTES: 4096
// SXE_CDB_COUNT_BYTES      : 24

struct SXE_CDB_INSTANCE {
    SXE_CDB_COUNT * counts                             ; /* pointer to mremap()able key       count memory */
    SXE_CDB_SHEET * sheets                             ; /* pointer to mremap()able key       index memory */
    uint8_t       * kvdata                             ; /* pointer to mremap()able key,value store memory */
    uint32_t        kvdata_size                        ; /* bytes allocated & used or not to store key,value pairs */
    uint32_t        kvdata_used                        ; /* bytes allocated & used        to store key,value pairs */
    uint32_t        kvdata_maximum                     ; /* bytes allocated max threshold to store key,value pairs */
    uint32_t        sheets_size                        ; /* bytes allocated : sheets_size * SXE_CDB_SHEET_BYTES */
    uint32_t        sheets_cells_size                  ; /* cells allocated & used or not to index a key */
    uint32_t        sheets_cells_used                  ; /* cells allocated & used        to index a key */
    uint32_t        sheets_split                       ; /* times 1 sheet split into 2 sheets */
    uint64_t        sheets_split_keys                  ; /* accumulated total of all keys examined during splits */
    uint64_t        keylen_misses                      ; /* times hash   matched but keylen didn't match */
    uint64_t        memcmp_misses                      ; /* times keylen matched but key    didn't match */
    uint32_t        keys_at_start                      ; /* sxe_cdb_instance_new() copy for sxe_cdb_instance_reboot() */
    uint32_t        counts_pages                       ; /* kernel pages @ counts */ //todo: add _pages to sheets & kvdata
    uint32_t        counts_size                        ; /* bytes allocated : counts_size * SXE_CDB_COUNT_BYTES */
    uint32_t        counts_next_free                   ; /* unused   next in generic    double linked *counts* list */
    uint32_t        counts_free                        ; /* unused  total in generic    double linked *counts* list */
    uint32_t        counts_used                        ; /*   used  total in generic    double linked *counts* list */
    uint32_t        counts_hi[SXE_CDB_COUNTS_LISTS_MAX]; /* highest count in particular double linked *counts* list */
    uint32_t        counts_lo[SXE_CDB_COUNTS_LISTS_MAX]; /* lowest  count in particular double linked *counts* list */
    uint16_t        sheets_index[SXE_CDB_SHEETS_MAX]   ;
} __attribute__((packed));

struct SXE_CDB_ENSEMBLE {
           uint32_t            cdb_count         ; /* instances of   SXE_CDB_INSTANCE; max 4GB kvdata per instance */
    struct SXE_CDB_INSTANCE ** cdb_instances     ; /* pointers  to   SXE_CDB_INSTANCE */
           SXE_SPINLOCK      * cdb_instance_locks; /* locks for each SXE_CDB_INSTANCE */
           uint32_t            cdb_is_locked : 1 ; /* use locks for  SXE_CDB_ENSEMBLE? */
} __attribute__((packed));

#include "sxe-cdb.h"

