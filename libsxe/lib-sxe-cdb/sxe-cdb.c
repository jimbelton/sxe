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

#define _GNU_SOURCE
#include <sys/mman.h> /* for mremap() */
#include <string.h> /* for memset() */

#include "murmurhash3.h"
#include "sxe-alloc.h"
#include "sxe-log.h"
#include "sxe-util.h"
#include "sxe-spinlock.h"
#include "sxe-cdb-private.h"

       __thread       uint32_t           sxe_cdb_tls_hkv_len_max = 0   ;
       __thread       SXE_CDB_HKV      * sxe_cdb_tls_hkv         = NULL;
       __thread       SXE_CDB_HKV_PART   sxe_cdb_tls_hkv_part          ;
       __thread       uint32_t           sxe_cdb_tls_walk_cnt_pos      ;
       __thread       uint32_t           sxe_cdb_tls_walk_hkv_pos      ;
       __thread       uint64_t           sxe_cdb_tls_walk_count        ;
       __thread       SXE_CDB_HASH       sxe_cdb_hash                  ;

static                SXE_SPINLOCK       sxe_cdb_ensemble_lock = { 0 }; /* usid by sxe_cdb_ensemble_(new|destroy)() */
static __thread const uint8_t          * sxe_cdb_key                  ; /* used by sxe_cdb_prepare() */
static __thread       uint32_t           sxe_cdb_key_len              ; /* used by sxe_cdb_prepare() */

SXE_CDB_HKV *
sxe_cdb_copy_hkv_to_tls(SXE_CDB_INSTANCE * cdb_instance, uint32_t hkv_pos)
{
    SXE_CDB_HKV * hkv = (SXE_CDB_HKV *) &cdb_instance->kvdata[hkv_pos];
    sxe_cdb_hkv_unpack(hkv, &sxe_cdb_tls_hkv_part);
    if (sxe_cdb_tls_hkv_part.hkv_len > sxe_cdb_tls_hkv_len_max) {
        SXEL7("(){} // realloc() from %u bytes to %u bytes", sxe_cdb_tls_hkv_len_max, sxe_cdb_tls_hkv_part.hkv_len);
        sxe_cdb_tls_hkv         = sxe_realloc(sxe_cdb_tls_hkv , sxe_cdb_tls_hkv_part.hkv_len);
        sxe_cdb_tls_hkv_len_max = sxe_cdb_tls_hkv ? sxe_cdb_tls_hkv_part.hkv_len : 0 /* realloc() failed :-( */;
        SXEA1(sxe_cdb_tls_hkv, ": realloc() failed for %zu bytes", sizeof(sxe_cdb_tls_hkv_part.hkv_len));
    }
    if (sxe_cdb_tls_hkv_len_max >= sxe_cdb_tls_hkv_part.hkv_len) {
        memcpy(sxe_cdb_tls_hkv,  hkv, sxe_cdb_tls_hkv_part.hkv_len); /* copy hkv into a tls buffer (for caller) */
        sxe_cdb_tls_hkv_part.key = &((uint8_t *)sxe_cdb_tls_hkv)[sxe_cdb_tls_hkv_part.hkv_len - sxe_cdb_tls_hkv_part.key_len
                                 - sxe_cdb_tls_hkv_part.val_len]; /* as a courtesy to the caller, */
        sxe_cdb_tls_hkv_part.val = &((uint8_t *)sxe_cdb_tls_hkv)[sxe_cdb_tls_hkv_part.hkv_len - sxe_cdb_tls_hkv_part.val_len]; /* pretend we've just unpacked the tls */
    }
    return sxe_cdb_tls_hkv;
} /* sxe_cdb_copy_hkv_to_tls() */

void
sxe_cdb_copy_tls_to_hkv(SXE_CDB_INSTANCE * cdb_instance, uint32_t hkv_pos)
{
    SXE_CDB_HKV * hkv = (SXE_CDB_HKV *) &cdb_instance->kvdata[hkv_pos];
    memcpy(hkv, sxe_cdb_tls_hkv, sxe_cdb_tls_hkv_part.hkv_len); /* copy tls buffer (from caller) into hkv */
    return;
} /* sxe_cdb_copy_tls_to_hkv() */

void
sxe_cdb_prepare(const uint8_t * key, uint32_t key_len)
{
    if (key) {
        MurmurHash3_xnn_128(key, key_len, 12345 /* todo: handle seed better :-) */, &sxe_cdb_hash.u64[0]);
        if (sxe_cdb_hash.u16[1] == sxe_cdb_hash.u16[2]) { /* guarantee that sxe_cdb_hash.u16[1] and sxe_cdb_hash.u16[2] are unique ... (in the worst possible way?) */
            sxe_cdb_hash.u16[1]  = sxe_cdb_hash.u16[3];
            if (sxe_cdb_hash.u16[1] == sxe_cdb_hash.u16[2]) {
                sxe_cdb_hash.u16[1]  = sxe_cdb_hash.u16[4]; /* COVERAGE EXCLUSION: todo: calculate some murmurhash3 collisions to coverage this line! */
            }
            if (sxe_cdb_hash.u16[1] == sxe_cdb_hash.u16[2]) {
                sxe_cdb_hash.u16[1]  = sxe_cdb_hash.u16[2] + 1; /* COVERAGE EXCLUSION: todo: calculate some murmurhash3 collisions to coverage this line! */
            }
        }
    }
    sxe_cdb_key     = key    ;
    sxe_cdb_key_len = key_len;
    SXEL6("%s(key=%.*s, key_len=%u){} // 0xhash=%04x-%04x-%04x", __FUNCTION__, key_len, key, key_len, sxe_cdb_hash.u16[0], sxe_cdb_hash.u16[1], sxe_cdb_hash.u16[2]);
} /* sxe_cdb_hash() */

static void
sxe_cdb_instance_new_init(
    SXE_CDB_INSTANCE * cdb_instance  ,
    uint32_t           keys_at_start ,
    uint32_t           kvdata_maximum) /* maximum bytes for kvdata memory or zero means no limit (i.e. up to 4GB) */
{
    unsigned cl;
    unsigned si;
    unsigned sheet_index     = 0;
    unsigned sheet_index_max = keys_at_start / SXE_CDB_KEYS_PER_SHEET * 2;
    SXEL6("initializing sheet indexes // sheet_index_max=%u", sheet_index_max);
    for (si = 0; si < SXE_CDB_SHEETS_MAX; si++) { /* loop over all sheet indexes */
        cdb_instance->sheets_index[si] = sheet_index;
        sheet_index ++;
        sheet_index = sheet_index >= sheet_index_max ? 0 : sheet_index;
    }

    cdb_instance->sheets_size       = sheet_index_max ? sheet_index_max : 1; /* always create at least one sheet */
    cdb_instance->kvdata_size       = SXE_CDB_KERNEL_PAGE_BYTES;
    cdb_instance->kvdata_used       = 1; /* 0 means cell is unused in table; yeah, we're 'wasting' 1 byte here :-) */
    cdb_instance->sheets_cells_size = SXE_CDB_KEYS_PER_SHEET;
    cdb_instance->sheets_cells_used = 0;
    cdb_instance->sheets_split      = 0;
    cdb_instance->sheets_split_keys = 0;
    cdb_instance->counts            = NULL; /* mmap()ed on demand */
    cdb_instance->counts_pages      = 0;
    cdb_instance->counts_size       = 0;
    cdb_instance->counts_next_free  = 1;
    cdb_instance->counts_free       = 0;
    cdb_instance->counts_used       = 0;
    cdb_instance->keylen_misses     = 0;
    cdb_instance->memcmp_misses     = 0;
    cdb_instance->kvdata_maximum    = kvdata_maximum;
    cdb_instance->keys_at_start     = keys_at_start;

    for (cl = 0; cl < SXE_CDB_COUNTS_LISTS_MAX; cl ++) {
        cdb_instance->counts_hi[cl] = SXE_CDB_COUNT_NONE;
        cdb_instance->counts_lo[cl] = SXE_CDB_COUNT_NONE;
    }

    cdb_instance->sheets = mmap(NULL /* kernel chooses addr */, SXE_CDB_SHEET_BYTES       * cdb_instance->sheets_size , PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    cdb_instance->kvdata = mmap(NULL /* kernel chooses addr */,                             cdb_instance->kvdata_size , PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    SXEL7("cdb_instance->sheets             : %p // 4k kernel pages: %u", cdb_instance->sheets, SXE_CDB_SHEET_BYTES * cdb_instance->sheets_size / 4096);
    SXEL7("cdb_instance->kvdata             : %p // 4k kernel pages: %u", cdb_instance->kvdata,                       cdb_instance->kvdata_size / 4096);
    SXEA1(MAP_FAILED != cdb_instance->sheets, "ERROR: FATAL: expected mmap() not to fail // %s(){}", __FUNCTION__);
    SXEA1(MAP_FAILED != cdb_instance->kvdata, "ERROR: FATAL: expected mmap() not to fail // %s(){}", __FUNCTION__);
} /* sxe_cdb_instance_new_init() */

SXE_CDB_INSTANCE *
sxe_cdb_instance_new(
    uint32_t keys_at_start ,
    uint32_t kvdata_maximum) /* maximum bytes for kvdata memory or zero means no limit (i.e. up to 4GB) */
{
    SXE_CDB_INSTANCE * cdb_instance = sxe_malloc(sizeof(*cdb_instance));
    SXEA1(cdb_instance, "ERROR: INTERNAL: malloc() failed for %zu bytes // %s(){}", sizeof(*cdb_instance), __FUNCTION__);

    SXEE6("(keys_at_start=%u, kvdata_maximum=%u)", keys_at_start, kvdata_maximum);

    SXEA6(getpagesize() == SXE_CDB_KERNEL_PAGE_BYTES, "ERROR: INTERNAL: expected %u=getpagesize() but got %u; todo: change code to work with other page sizes", SXE_CDB_KERNEL_PAGE_BYTES, getpagesize());

    SXEA6(SXE_CDB_KEYS_PER_ROW   == (1<<SXE_CDB_KEYS_PER_ROW_BITS                           ), "ERROR: INTERNAL: SXE_CDB_KEYS_PER_ROW_BITS   is used in SXE_CDB_UID and must reflect SXE_CDB_KEYS_PER_ROW"  );
    SXEA6(SXE_CDB_ROWS_PER_SHEET == (1<<SXE_CDB_ROWS_PER_SHEET_BITS                         ), "ERROR: INTERNAL: SXE_CDB_ROWS_PER_SHEET_BITS is used in SXE_CDB_UID and must reflect SXE_CDB_ROWS_PER_SHEET");
    SXEA6(16                     == (SXE_CDB_ROWS_PER_SHEET_BITS + SXE_CDB_KEYS_PER_ROW_BITS), "ERROR: INTERNAL: expected 16==SXE_CDB_ROWS_PER_SHEET_BITS + SXE_CDB_KEYS_PER_ROW_BITS");

    SXEA6(0x00000007 == KEY_HEADER_LEN_1_KEY_LEN_MAX, "ERROR: INTERNAL: expected KEY_HEADER_LEN_1_KEY_LEN_MAX to be %08x but got %08x; compiler flag problem?", 0x00000007, KEY_HEADER_LEN_1_KEY_LEN_MAX);
    SXEA6(0x0000007f == KEY_HEADER_LEN_3_KEY_LEN_MAX, "ERROR: INTERNAL: expected KEY_HEADER_LEN_3_KEY_LEN_MAX to be %08x but got %08x; compiler flag problem?", 0x0000007f, KEY_HEADER_LEN_3_KEY_LEN_MAX);
    SXEA6(0x0000ffff == KEY_HEADER_LEN_5_KEY_LEN_MAX, "ERROR: INTERNAL: expected KEY_HEADER_LEN_5_KEY_LEN_MAX to be %08x but got %08x; compiler flag problem?", 0x0000ffff, KEY_HEADER_LEN_5_KEY_LEN_MAX);
    SXEA6(0x00ffffff == KEY_HEADER_LEN_8_KEY_LEN_MAX, "ERROR: INTERNAL: expected KEY_HEADER_LEN_8_KEY_LEN_MAX to be %08x but got %08x; compiler flag problem?", 0x00ffffff, KEY_HEADER_LEN_8_KEY_LEN_MAX);

    SXEA6(0x0000000f == KEY_HEADER_LEN_1_VAL_LEN_MAX, "ERROR: INTERNAL: expected KEY_HEADER_LEN_1_VAL_LEN_MAX to be %08x but got %08x; compiler flag problem?", 0x0000000f, KEY_HEADER_LEN_1_VAL_LEN_MAX);
    SXEA6(0x0000ffff == KEY_HEADER_LEN_3_VAL_LEN_MAX, "ERROR: INTERNAL: expected KEY_HEADER_LEN_3_VAL_LEN_MAX to be %08x but got %08x; compiler flag problem?", 0x0000ffff, KEY_HEADER_LEN_3_VAL_LEN_MAX);
    SXEA6(0x0000ffff == KEY_HEADER_LEN_5_VAL_LEN_MAX, "ERROR: INTERNAL: expected KEY_HEADER_LEN_5_VAL_LEN_MAX to be %08x but got %08x; compiler flag problem?", 0x0000ffff, KEY_HEADER_LEN_5_VAL_LEN_MAX);
    SXEA6(0xffffffff == KEY_HEADER_LEN_8_VAL_LEN_MAX, "ERROR: INTERNAL: expected KEY_HEADER_LEN_8_VAL_LEN_MAX to be %08x but got %08x; compiler flag problem?", 0xffffffff, KEY_HEADER_LEN_8_VAL_LEN_MAX);

    #define SIZEOF_MEMBER(STRUCT,MEMBER) sizeof(((STRUCT *)0)->MEMBER)
    SXEA6( 1 == SIZEOF_MEMBER(SXE_CDB_HKV     ,header_len_1), "ERROR: INTERNAL: unexpected sizeof header_len_1");
    SXEA6( 3 == SIZEOF_MEMBER(SXE_CDB_HKV     ,header_len_3), "ERROR: INTERNAL: unexpected sizeof header_len_3");
    SXEA6( 5 == SIZEOF_MEMBER(SXE_CDB_HKV     ,header_len_5), "ERROR: INTERNAL: unexpected sizeof header_len_5");
    SXEA6( 8 == SIZEOF_MEMBER(SXE_CDB_HKV     ,header_len_8), "ERROR: INTERNAL: unexpected sizeof header_len_8");
    SXEA6(18 ==        sizeof(SXE_CDB_COUNT                ), "ERROR: INTERNAL: unexpected sizeof SXE_CDB_COUNT");
    SXEA6(12 ==        sizeof(SXE_CDB_HKV_LIST             ), "ERROR: INTERNAL: unexpected sizeof SXE_CDB_HKV_LIST");
    SXEA6( 5 == SIZEOF_MEMBER(SXE_CDB_UID     ,as_part     ), "ERROR: INTERNAL: unexpected sizeof SXE_CDB_UID");
    SXEA6( 5 == SIZEOF_MEMBER(SXE_CDB_UID     ,as_u40      ), "ERROR: INTERNAL: unexpected sizeof SXE_CDB_UID");
    SXEA6( 8 == SIZEOF_MEMBER(SXE_CDB_UID     ,as_u64      ), "ERROR: INTERNAL: unexpected sizeof SXE_CDB_UID");

    SXEL7("SXE_CDB_CACHE_LINE_BYTES : %u" , SXE_CDB_CACHE_LINE_BYTES );
    SXEL7("SXE_CDB_ROW_BYTES        : %zu", SXE_CDB_ROW_BYTES        );
    SXEL7("SXE_CDB_KEYS_PER_ROW     : %zu", SXE_CDB_KEYS_PER_ROW     );
    SXEL7("SXE_CDB_SHEET_BYTES      : %u" , SXE_CDB_SHEET_BYTES      );
    SXEL7("SXE_CDB_ROWS_PER_SHEET   : %zu", SXE_CDB_ROWS_PER_SHEET   );
    SXEL7("SXE_CDB_KEYS_PER_SHEET   : %zu", SXE_CDB_KEYS_PER_SHEET   );
    SXEL7("SXE_CDB_SHEETS_MAX       : %zu", SXE_CDB_SHEETS_MAX       );
    SXEL7("SXE_CDB_KERNEL_PAGE_BYTES: %u" , SXE_CDB_KERNEL_PAGE_BYTES);
    SXEL7("SXE_CDB_COUNT_BYTES      : %zu", SXE_CDB_COUNT_BYTES      );

    sxe_cdb_instance_new_init(cdb_instance, keys_at_start, kvdata_maximum);

    SXER6("return %p=cdb_instance", cdb_instance);
    return cdb_instance;
} /* sxe_cdb_instance_new() */

static void
sxe_cdb_instance_destroy_mmaps(SXE_CDB_INSTANCE * cdb_instance)
{
    SXEL6("%s(cdb_instance=?){}", __FUNCTION__);
                                SXEA1(0 == munmap(cdb_instance->sheets, SXE_CDB_SHEET_BYTES       * cdb_instance->sheets_size ), "ERROR: INTERNAL: munmap() failed for sheets");
                                SXEA1(0 == munmap(cdb_instance->kvdata,                             cdb_instance->kvdata_size ), "ERROR: INTERNAL: munmap() failed for kvdata");
    if (cdb_instance->counts) { SXEA1(0 == munmap(cdb_instance->counts, SXE_CDB_KERNEL_PAGE_BYTES * cdb_instance->counts_pages), "ERROR: INTERNAL: munmap() failed for counts"); }
} /* sxe_cdb_instance_destroy_mmaps() */

void
sxe_cdb_instance_destroy(SXE_CDB_INSTANCE * cdb_instance)
{
    SXEL6("%s(cdb_instance=?){}", __FUNCTION__);
    sxe_cdb_instance_destroy_mmaps(cdb_instance);
    sxe_free(cdb_instance);
} /* sxe_cdb_instance_destroy() */

void
sxe_cdb_instance_reboot(SXE_CDB_INSTANCE * cdb_instance)
{
    SXEL6("%s(cdb_instance=?){}", __FUNCTION__);

    sxe_cdb_instance_destroy_mmaps(cdb_instance                                                           ); /* goodbye mmaps */
    sxe_cdb_instance_new_init     (cdb_instance, cdb_instance->keys_at_start, cdb_instance->kvdata_maximum); /*   hello mmaps */
} /* sxe_cdb_instance_reboot() */

#if SXE_DEBUG
void
sxe_cdb_instance_debug_validate(SXE_CDB_INSTANCE * cdb_instance, const char * debug)
{
    uint16_t sheet;
    unsigned row;
    unsigned cell;

    for (sheet = 0; sheet < cdb_instance->sheets_size; sheet ++) {
        unsigned key_total     = 0;
        unsigned key_used      = 0;
        unsigned row_count_hi  = 0;
        unsigned row_count_avg = 0;
        for (row = 0; row < SXE_CDB_ROWS_PER_SHEET; row ++) {
            unsigned row_count = 0;
            for (cell = 0; cell < SXE_CDB_KEYS_PER_ROW; cell ++) {
                key_total ++;
                if (cdb_instance->sheets[sheet].row[row].hkv_pos.u32[cell]) { /* if cell used */
                    key_used      ++;
                    row_count     ++;
                    row_count_avg ++;
                    uint16_t hash_hi = cdb_instance->sheets[sheet].row[row].hash_hi.u16[cell];
                    uint16_t mysheet = cdb_instance->sheets_index[hash_hi % SXE_CDB_SHEETS_MAX];
                    SXEA1(mysheet == sheet, "ERROR: INTERNAL: unexpected sheet index %u for sheet %u, row %u, cell %u; // %s", mysheet, sheet, row, cell, debug);
                }
            } // for (cell
            row_count_hi = row_count > row_count_hi ? row_count : row_count_hi;
        } // for (row
        SXEL5("sheet %5u has %5u used from %5u keys or %2u%%; row_count_hi %u, row_count_avg %zu", sheet, key_used, key_total, key_used * 100 / key_total, row_count_hi, row_count_avg / SXE_CDB_ROWS_PER_SHEET);
    } // for (sheet
} /* sxe_cdb_instance_debug_validate() */
#endif

void
sxe_cdb_instance_split_sheet(SXE_CDB_INSTANCE * cdb_instance, uint16_t sheet)
{
    uint16_t this_sheet =               sheet      ;
    uint16_t that_sheet = cdb_instance->sheets_size;
    unsigned row;
    unsigned cell;

    SXEE6("(cdb_instance=?, sheet=%u)", sheet);

    //debug sxe_cdb_debug_validate(cdb, "a");

    SXEL7("cdb_instance->sheets             : %p // old base", cdb_instance->sheets);
           cdb_instance->sheets = mremap(cdb_instance->sheets, SXE_CDB_SHEET_BYTES * cdb_instance->sheets_size, SXE_CDB_SHEET_BYTES * (1 + cdb_instance->sheets_size), MREMAP_MAYMOVE);
    SXEL7("cdb_instance->sheets             : %p // new base after mremap()", cdb_instance->sheets);
           cdb_instance->sheets_size       ++; /* count extra sheet */
           cdb_instance->sheets_cells_size += SXE_CDB_KEYS_PER_SHEET;

    SXEA1(MAP_FAILED != cdb_instance->sheets, "ERROR: FATAL: expected mremap() not to fail // %s(){}", __FUNCTION__);

    SXEL7("split sheet indexes into roughly two");
    unsigned si;
    unsigned sheet_toggle  = 0;
    unsigned sheet_toggles = 0;
    for (si = 0; si < SXE_CDB_SHEETS_MAX; si++) { /* loop over all sheet indexes */
        SXEA6(cdb_instance->sheets_index[si] <= that_sheet, "ERROR: INTERNAL: expected ->sheets_index[%u] <= %u=that_sheet but found %u", si, that_sheet, cdb_instance->sheets_index[si]);
        if (this_sheet == cdb_instance->sheets_index[si]) { /* if this is the full sheet? */
            cdb_instance->sheets_index[si] = sheet_toggle ? that_sheet : this_sheet; /* split */
            sheet_toggle = sheet_toggle ? 0 : 1;
            sheet_toggles ++;
        }
    }
    SXEA6(sheet_toggles > 1, "ERROR: INTERNAL: too many keys? need at least two toggles to grow cdb!");

#if SXE_DEBUG
    unsigned keys_total = 0;
    unsigned keys_moved = 0;
#endif
    SXEL7("split sheet        into roughly two; visit %zu keys * %zu rows", SXE_CDB_KEYS_PER_ROW, SXE_CDB_ROWS_PER_SHEET);
    for (row = 0; row < SXE_CDB_ROWS_PER_SHEET; row ++) {
        for (cell = 0; cell < SXE_CDB_KEYS_PER_ROW; cell ++) {
            if (cdb_instance->sheets[this_sheet].row[row].hkv_pos.u32[cell]) { /* if cell used */
                cdb_instance->sheets_split_keys ++;
#if SXE_DEBUG
                keys_total ++;
#endif
                uint16_t hash_hi = cdb_instance->sheets[this_sheet].row[row].hash_hi.u16[cell];
                uint16_t mysheet = cdb_instance->sheets_index[hash_hi % SXE_CDB_SHEETS_MAX];
                SXEA6((mysheet == this_sheet) || (mysheet == that_sheet), "ERROR: INTERNAL: expecting sheet %u or %u but got %u (while splitting cell %u in row %u with hash_hi %u)", this_sheet, that_sheet, mysheet, cell, row, hash_hi);
                if    (mysheet == that_sheet) { /* cell should split to that other sheet? */
                    cdb_instance->sheets[that_sheet].row[row].hash_lo.u16[cell] = cdb_instance->sheets[this_sheet].row[row].hash_lo.u16[cell]; /* copy cell    */
                    cdb_instance->sheets[that_sheet].row[row].hash_hi.u16[cell] = cdb_instance->sheets[this_sheet].row[row].hash_hi.u16[cell]; /* from old     */
                    cdb_instance->sheets[that_sheet].row[row].hkv_pos.u32[cell] = cdb_instance->sheets[this_sheet].row[row].hkv_pos.u32[cell]; /* to new sheet */

                    cdb_instance->sheets[this_sheet].row[row].hkv_pos.u32[cell] = 0; /* mark cell as unused */
#if SXE_DEBUG
                    keys_moved ++;
#endif
                }
            }
        } // for (cell
    } // for (row
    SXEL6("split %u from %u sheet %5u keys to sheet %5u; now %u from %u keys total", keys_moved, keys_total, this_sheet, that_sheet, cdb_instance->sheets_cells_used, cdb_instance->sheets_cells_size);

    //debug sxe_cdb_debug_validate(cdb, "b");

    cdb_instance->sheets_split ++;

    SXER6("return");
} /* sxe_cdb_instance_split_sheet() */

uint64_t /* SXE_CDB_UID; SXE_CDB_UID_NONE means something went wrong and key not appended */
sxe_cdb_instance_put_val(SXE_CDB_INSTANCE * cdb_instance, const uint8_t * val, uint32_t val_len)
{
    SXE_CDB_UID uid;

    uid.as_u64.u = SXE_CDB_UID_NONE;

    if ((cdb_instance->kvdata_maximum > 0) && (cdb_instance->kvdata_size > cdb_instance->kvdata_maximum)) { /* test here so sheet splits can no longer happen */
        SXEL6("%s(cdb_instance=?, val=?, val_len=%u){} // return %010lx=ii[%04x]%03x-%01x=%s; <-- Want %u but reached caller set maximum ->kvdata_maximum=%u; early out with no append",
              __FUNCTION__, val_len, uid.as_u64.u, uid.as_part.sheets_index_index, uid.as_part.row, uid.as_part.cell, SXE_CDB_UID_NONE == uid.as_u64.u ? "failure" : "success", cdb_instance->kvdata_size, cdb_instance->kvdata_maximum);
        goto SXE_EARLY_OUT;
    }

    unsigned header_len = 0;
    if      ( sxe_cdb_key_len ==                            0                                              ) { SXEL3("WARNING: %s(): unexpected %u=key_len and/or %u=val_len; early out with no append for key #%u", __FUNCTION__, sxe_cdb_key_len, val_len, cdb_instance->sheets_cells_used); goto SXE_EARLY_OUT; }
    else if ((sxe_cdb_key_len <= KEY_HEADER_LEN_1_KEY_LEN_MAX) && (val_len <= KEY_HEADER_LEN_1_VAL_LEN_MAX)) { header_len = 1; }
    else if ((sxe_cdb_key_len <= KEY_HEADER_LEN_3_KEY_LEN_MAX) && (val_len <= KEY_HEADER_LEN_3_VAL_LEN_MAX)) { header_len = 3; }
    else if ((sxe_cdb_key_len <= KEY_HEADER_LEN_5_KEY_LEN_MAX) && (val_len <= KEY_HEADER_LEN_5_VAL_LEN_MAX)) { header_len = 5; }
    else if ((sxe_cdb_key_len <= KEY_HEADER_LEN_8_KEY_LEN_MAX) && (val_len <= KEY_HEADER_LEN_8_VAL_LEN_MAX)) { header_len = 8; }
    else                                                                                                     { SXEL3("WARNING: %s(): unexpected %u=key_len and/or %u=val_len; early out with no append for key #%u", __FUNCTION__, sxe_cdb_key_len, val_len, cdb_instance->sheets_cells_used); goto SXE_EARLY_OUT; }

SXE_CDB_ADD_RETRY_ROW_SCAN:;
    uint16_t sheet_index = sxe_cdb_hash.u16[0] % SXE_CDB_SHEETS_MAX;
    uint16_t sheet       = cdb_instance->sheets_index[sheet_index];
    SXEA6(sheet < cdb_instance->sheets_size, "ERROR: INTERNAL: %u=sheet < %u=cdb->sheets_size", sheet, cdb_instance->sheets_size);

    unsigned row;
    unsigned cell;
    unsigned row_1 = sxe_cdb_hash.u16[1] & (SXE_CDB_ROWS_PER_SHEET - 1);
    unsigned row_2 = sxe_cdb_hash.u16[2] & (SXE_CDB_ROWS_PER_SHEET - 1);
    unsigned row_1_cell = SXE_CDB_KEYS_PER_ROW, row_1_used = 0;
    unsigned row_2_cell = SXE_CDB_KEYS_PER_ROW, row_2_used = 0;
    for (cell = 0; cell < SXE_CDB_KEYS_PER_ROW; cell ++) {
        if (cdb_instance->sheets[sheet].row[row_1].hkv_pos.u32[cell]) { row_1_used ++; } else { row_1_cell = SXE_CDB_KEYS_PER_ROW == row_1_cell ? cell : row_1_cell; }
        if (cdb_instance->sheets[sheet].row[row_2].hkv_pos.u32[cell]) { row_2_used ++; } else { row_2_cell = SXE_CDB_KEYS_PER_ROW == row_2_cell ? cell : row_2_cell; }
    }

    if ((SXE_CDB_KEYS_PER_ROW == row_1_used)
    &&  (SXE_CDB_KEYS_PER_ROW == row_2_used)) { /* if both sheet rows full */
        if (cdb_instance->sheets_size >= SXE_CDB_SHEETS_MAX) { SXEL3("WARNING: %s(): cannot split at maximum sheet; early out with no append for key #%u", __FUNCTION__, cdb_instance->sheets_cells_used); goto SXE_EARLY_OUT; }
        sxe_cdb_instance_split_sheet(cdb_instance, sheet);
        goto SXE_CDB_ADD_RETRY_ROW_SCAN;
    }

    cell = row_1_used < row_2_used ? row_1_cell : row_2_cell; /* last free cell on row to use */
    row  = row_1_used < row_2_used ? row_1      : row_2     ; /*                   row to use */

    uint32_t k              =                             cdb_instance->kvdata_used;
    uint64_t key_bytes_free = cdb_instance->kvdata_size - cdb_instance->kvdata_used;
    uint64_t key_bytes_want = header_len + sxe_cdb_key_len + val_len;

    if (key_bytes_want > key_bytes_free) { /* come here to mremap() more key space! */
        uint32_t want_size_rounded_to_kernel_pages = (((key_bytes_want + (SXE_CDB_KERNEL_PAGE_BYTES - 1)) / SXE_CDB_KERNEL_PAGE_BYTES) * SXE_CDB_KERNEL_PAGE_BYTES) + SXE_CDB_KERNEL_PAGE_BYTES;
        if (cdb_instance->kvdata_size + want_size_rounded_to_kernel_pages < cdb_instance->kvdata_size) { SXEL3("WARNING: %s(): avoiding 4GB kvdata wrap; early out with no append for key #%u", __FUNCTION__, cdb_instance->sheets_cells_used); goto SXE_EARLY_OUT; }
            cdb_instance->kvdata       = mremap(cdb_instance->kvdata, cdb_instance->kvdata_size, cdb_instance->kvdata_size + want_size_rounded_to_kernel_pages, MREMAP_MAYMOVE);
            cdb_instance->kvdata_size += want_size_rounded_to_kernel_pages;
            SXEA1(MAP_FAILED != cdb_instance->kvdata, "ERROR: FATAL: expected mremap() not to fail // %s(){}", __FUNCTION__);
    }

    cdb_instance->sheets[sheet].row[row].hash_lo.u16[cell] = sxe_cdb_hash.u16[1];
    cdb_instance->sheets[sheet].row[row].hash_hi.u16[cell] = sxe_cdb_hash.u16[0];
    cdb_instance->sheets[sheet].row[row].hkv_pos.u32[cell] = k;

    SXE_CDB_HKV * hkv = (SXE_CDB_HKV *) &cdb_instance->kvdata[k];
    if      (1 == header_len) { hkv->header_len_1.flag = 0;                                                               hkv->header_len_1.key_len = sxe_cdb_key_len; hkv->header_len_1.val_len = val_len; memcpy(&hkv->header_len_1.content[0], sxe_cdb_key, sxe_cdb_key_len); memcpy(&hkv->header_len_1.content[sxe_cdb_key_len], val, val_len); }
    else if (3 == header_len) { hkv->header_len_3.flag = 1;                                                               hkv->header_len_3.key_len = sxe_cdb_key_len; hkv->header_len_3.val_len = val_len; memcpy(&hkv->header_len_3.content[0], sxe_cdb_key, sxe_cdb_key_len); memcpy(&hkv->header_len_3.content[sxe_cdb_key_len], val, val_len); }
    else if (5 == header_len) { hkv->header_len_5.flag = 0; hkv->header_len_5.xxx_len = 0; hkv->header_len_5.yyy_len = 0; hkv->header_len_5.key_len = sxe_cdb_key_len; hkv->header_len_5.val_len = val_len; memcpy(&hkv->header_len_5.content[0], sxe_cdb_key, sxe_cdb_key_len); memcpy(&hkv->header_len_5.content[sxe_cdb_key_len], val, val_len); }
    else if (8 == header_len) { hkv->header_len_8.flag = 0; hkv->header_len_8.xxx_len = 0; hkv->header_len_8.yyy_len = 1; hkv->header_len_8.key_len = sxe_cdb_key_len; hkv->header_len_8.val_len = val_len; memcpy(&hkv->header_len_8.content[0], sxe_cdb_key, sxe_cdb_key_len); memcpy(&hkv->header_len_8.content[sxe_cdb_key_len], val, val_len); }

    cdb_instance->kvdata_used       += key_bytes_want;
    cdb_instance->sheets_cells_used ++;

    uid.as_u64.u                   = 0          ;
    uid.as_part.sheets_index_index = sheet_index;
    uid.as_part.row                = row        ;
    uid.as_part.cell               = cell       ; /* uid to new key */

    SXEL6("%s(cdb_instance=?, val=?, val_len=%u){} // return %010lx=ii[%04x]%03x-%01x=%s; sheet=%05u row_1/2=%04u/%04u row_1/2_used=%02u/%02u row=%04u cell=%02u key_bytes_want=%lu",
          __FUNCTION__, val_len, uid.as_u64.u, uid.as_part.sheets_index_index, uid.as_part.row, uid.as_part.cell, SXE_CDB_UID_NONE == uid.as_u64.u ? "failure" : "success", sheet, row_1, row_2, row_1_used, row_2_used, row, cell, key_bytes_want);

    //todo: fantasize about mremap() for each sheet? look through 64k keys to remove deletions

SXE_EARLY_OUT:;
    return uid.as_u64.u;
} /* sxe_cdb_instance_put_val() */

void
sxe_cdb_hkv_unpack(
    SXE_CDB_HKV      * hkv     ,
    SXE_CDB_HKV_PART * hkv_part)
{
    if      (0 == hkv->header_len_8.flag && hkv->header_len_8.xxx_len == 0 && hkv->header_len_8.yyy_len == 1) { hkv_part->hkv_len = sizeof(hkv->header_len_8) + hkv->header_len_8.key_len + hkv->header_len_8.val_len; hkv_part->key_len = hkv->header_len_8.key_len; hkv_part->val_len = hkv->header_len_8.val_len; hkv_part->key = &hkv->header_len_8.content[0]; hkv_part->val = &hkv->header_len_8.content[hkv_part->key_len]; }
    else if (0 == hkv->header_len_5.flag && hkv->header_len_5.xxx_len == 0 && hkv->header_len_5.yyy_len == 0) { hkv_part->hkv_len = sizeof(hkv->header_len_5) + hkv->header_len_5.key_len + hkv->header_len_5.val_len; hkv_part->key_len = hkv->header_len_5.key_len; hkv_part->val_len = hkv->header_len_5.val_len; hkv_part->key = &hkv->header_len_5.content[0]; hkv_part->val = &hkv->header_len_5.content[hkv_part->key_len]; }
    else if (0 == hkv->header_len_1.flag && hkv->header_len_1.key_len != 0                                  ) { hkv_part->hkv_len = sizeof(hkv->header_len_1) + hkv->header_len_1.key_len + hkv->header_len_1.val_len; hkv_part->key_len = hkv->header_len_1.key_len; hkv_part->val_len = hkv->header_len_1.val_len; hkv_part->key = &hkv->header_len_1.content[0]; hkv_part->val = &hkv->header_len_1.content[hkv_part->key_len]; }
    else            /* header_len_3 */                                                                        { hkv_part->hkv_len = sizeof(hkv->header_len_3) + hkv->header_len_3.key_len + hkv->header_len_3.val_len; hkv_part->key_len = hkv->header_len_3.key_len; hkv_part->val_len = hkv->header_len_3.val_len; hkv_part->key = &hkv->header_len_3.content[0]; hkv_part->val = &hkv->header_len_3.content[hkv_part->key_len]; }
} /* sxe_cdb_hkv_unpack() */

uint32_t
sxe_cdb_h_len(SXE_CDB_HKV * hkv)
{
    uint32_t h_len;
    if      (0 == hkv->header_len_8.flag && hkv->header_len_8.xxx_len == 0 && hkv->header_len_8.yyy_len == 1) { h_len = sizeof(hkv->header_len_8); }
    else if (0 == hkv->header_len_5.flag && hkv->header_len_5.xxx_len == 0 && hkv->header_len_5.yyy_len == 0) { h_len = sizeof(hkv->header_len_5); }
    else if (0 == hkv->header_len_1.flag && hkv->header_len_1.key_len != 0                                  ) { h_len = sizeof(hkv->header_len_1); }
    else            /* header_len_3 */                                                                        { h_len = sizeof(hkv->header_len_3); }
    SXEL6("%s(hkv=?){} // %u=h_len", __FUNCTION__, h_len);
    return h_len;
} /* sxe_cdb_hkv_len() */

#define SXE_CDB_GET_HKV_IN_CELL_IN(ROW) \
    if ((sxe_cdb_hash.u16[1] == cdb_instance->sheets[sheet].row[ROW].hash_lo.u16[cell])            \
    &&  (sxe_cdb_hash.u16[0] == cdb_instance->sheets[sheet].row[ROW].hash_hi.u16[cell])            \
    &&  (                       cdb_instance->sheets[sheet].row[ROW].hkv_pos.u32[cell])) {         \
        hkv_pos               = cdb_instance->sheets[sheet].row[ROW].hkv_pos.u32[cell];            \
        tmp_hkv               = (SXE_CDB_HKV *) &cdb_instance->kvdata[hkv_pos];                    \
        sxe_cdb_hkv_unpack(tmp_hkv, &sxe_cdb_tls_hkv_part);                                        \
        if (sxe_cdb_key_len ==        sxe_cdb_tls_hkv_part.key_len) {                              \
            if (0           == memcmp(sxe_cdb_tls_hkv_part.key, sxe_cdb_key, sxe_cdb_key_len)) {   \
                tls_hkv      = tmp_hkv; /* key exists! */                                          \
                goto SXE_EARLY_OUT;                                                                \
            }                                                                                      \
            else {                                                                                 \
                cdb_instance->memcmp_misses ++;                                                    \
            }                                                                                      \
        }                                                                                          \
        else {                                                                                     \
            cdb_instance->keylen_misses ++;                                                        \
        }                                                                                          \
    }

#define SXE_CDB_GET_UID_IN_CELL_IN(ROW) \
    if ((sxe_cdb_hash.u16[1] == cdb_instance->sheets[sheet].row[ROW].hash_lo.u16[cell])            \
    &&  (sxe_cdb_hash.u16[0] == cdb_instance->sheets[sheet].row[ROW].hash_hi.u16[cell])            \
    &&  (                       cdb_instance->sheets[sheet].row[ROW].hkv_pos.u32[cell])) {         \
        hkv_pos               = cdb_instance->sheets[sheet].row[ROW].hkv_pos.u32[cell];            \
        tmp_hkv               = (SXE_CDB_HKV *) &cdb_instance->kvdata[hkv_pos];                    \
        sxe_cdb_hkv_unpack(tmp_hkv, &sxe_cdb_tls_hkv_part);                                        \
        if (sxe_cdb_key_len ==        sxe_cdb_tls_hkv_part.key_len) {                              \
            if (0           == memcmp(sxe_cdb_tls_hkv_part.key, sxe_cdb_key, sxe_cdb_key_len)) {   \
                uid.as_part.row  = ROW ;                                                           \
                uid.as_part.cell = cell; /* uid to existing key */                                 \
                goto SXE_EARLY_OUT;                                                                \
            }                                                                                      \
            else {                                                                                 \
                cdb_instance->memcmp_misses ++;                                                    \
            }                                                                                      \
        }                                                                                          \
        else {                                                                                     \
            cdb_instance->keylen_misses ++;                                                        \
        }                                                                                          \
    }

SXE_CDB_HKV * /* NULL or *dangerous* direct pointer to original SXE_CDB_HKV; header + key + value bytes */
sxe_cdb_instance_get_hkv_raw( /* NOTE: hkv only usable in-between sxe_cdb_*() calls in single-threaded environment due to mremap() and/or sxe_cdb_ensemble_swap_instances() possibility; pulling the memory rug from under us */
    SXE_CDB_INSTANCE  * cdb_instance)
{
    SXE_CDB_HKV * tls_hkv = NULL; /* result */

    uint16_t sheet = cdb_instance->sheets_index[sxe_cdb_hash.u16[0] % SXE_CDB_SHEETS_MAX];
    SXEA6(sheet < cdb_instance->sheets_size, "ERROR: INTERNAL: %u=sheet < %u=cdb_instance->sheets_size", sheet, cdb_instance->sheets_size);

    uint32_t      hkv_pos    ;
    SXE_CDB_HKV * tmp_hkv    ;
    uint32_t      cell       ;
    uint32_t      row_1 = sxe_cdb_hash.u16[1] & (SXE_CDB_ROWS_PER_SHEET - 1);
    uint32_t      row_2 = sxe_cdb_hash.u16[2] & (SXE_CDB_ROWS_PER_SHEET - 1);
#if SXE_DEBUG
    sxe_cdb_tls_hkv_part.key_len = 0;
    sxe_cdb_tls_hkv_part.val_len = 0;
#endif
    for (cell = 0; cell < SXE_CDB_KEYS_PER_ROW; cell ++) {
        SXE_CDB_GET_HKV_IN_CELL_IN(row_1);
        SXE_CDB_GET_HKV_IN_CELL_IN(row_2);
    }

SXE_EARLY_OUT:;
    SXEL6("%s(cdb_instance=?){} // return %p (%s); sxe_cdb_tls_hkv_part.val_len=%u", __FUNCTION__, tls_hkv, tls_hkv ? "key exists" : "key doesn't exist", sxe_cdb_tls_hkv_part.val_len);
    return tls_hkv;
} /* sxe_cdb_instance_get_hkv_raw() */

uint64_t /* SXE_CDB_UID; SXE_CDB_UID_NONE means key not found */
sxe_cdb_instance_get_uid(SXE_CDB_INSTANCE * cdb_instance)
{
    SXE_CDB_UID uid;

    uid.as_u64.u = 0;

    uid.as_part.sheets_index_index = sxe_cdb_hash.u16[0] % SXE_CDB_SHEETS_MAX;
    uint16_t sheet = cdb_instance->sheets_index[uid.as_part.sheets_index_index];
    SXEA6(sheet < cdb_instance->sheets_size, "ERROR: INTERNAL: %u=sheet < %u=cdb_instance->sheets_size", sheet, cdb_instance->sheets_size);

    uint32_t      hkv_pos    ;
    SXE_CDB_HKV * tmp_hkv    ;
    uint32_t      cell       ;
    uint32_t      row_1 = sxe_cdb_hash.u16[1] & (SXE_CDB_ROWS_PER_SHEET - 1);
    uint32_t      row_2 = sxe_cdb_hash.u16[2] & (SXE_CDB_ROWS_PER_SHEET - 1);
#if SXE_DEBUG
    sxe_cdb_tls_hkv_part.key_len = 0;
    sxe_cdb_tls_hkv_part.val_len = 0;
#endif
    for (cell = 0; cell < SXE_CDB_KEYS_PER_ROW; cell ++) {
        SXE_CDB_GET_UID_IN_CELL_IN(row_1);
        SXE_CDB_GET_UID_IN_CELL_IN(row_2);
    }

    uid.as_u64.u = SXE_CDB_UID_NONE;

SXE_EARLY_OUT:;
    SXEL6("%s(cdb_instance=?){} // return %010lx=ii[%04x]%03x-%01x=%s // sxe_cdb_tls_hkv_part.val_len=%u", __FUNCTION__, uid.as_u64.u, uid.as_part.sheets_index_index, uid.as_part.row, uid.as_part.cell, SXE_CDB_UID_NONE == uid.as_u64.u ? "key doesn't exist" : "key exists", sxe_cdb_tls_hkv_part.val_len);
    return uid.as_u64.u;
} /* sxe_cdb_instance_get_uid() */

SXE_CDB_HKV * /* NULL or tls SXE_CDB_HKV raw; not copy */
sxe_cdb_instance_get_uid_hkv_raw(SXE_CDB_INSTANCE * cdb_instance, SXE_CDB_UID uid)
{
    SXE_CDB_HKV * tls_hkv = NULL; /* result */

    uint32_t cell        = uid.as_part.cell                       ; SXEA1(cell        < SXE_CDB_KEYS_PER_ROW     , "ERROR: INTERNAL: %u=cell        < %lu=SXE_CDB_KEYS_PER_ROW"    , cell       , SXE_CDB_KEYS_PER_ROW     );
    uint32_t row         = uid.as_part.row                        ; SXEA1(row         < SXE_CDB_ROWS_PER_SHEET   , "ERROR: INTERNAL: %u=row         < %lu=SXE_CDB_ROWS_PER_SHEET"  , row        , SXE_CDB_ROWS_PER_SHEET   );
    uint16_t sheet_index = uid.as_part.sheets_index_index         ; SXEA1(sheet_index < SXE_CDB_SHEETS_MAX       , "ERROR: INTERNAL: %u=sheet_index < %lu=SXE_CDB_SHEETS_MAX"      , sheet_index, SXE_CDB_SHEETS_MAX       );
    uint16_t sheet       = cdb_instance->sheets_index[sheet_index]; SXEA1(sheet       < cdb_instance->sheets_size, "ERROR: INTERNAL: %u=sheet       < %u=cdb_instance->sheets_size", sheet      , cdb_instance->sheets_size);
    if (cdb_instance->sheets[sheet].row[row].hkv_pos.u32[cell]) {
        uint32_t     hkv_pos =                  cdb_instance->sheets[sheet].row[row].hkv_pos.u32[cell];
                 tls_hkv     = (SXE_CDB_HKV *) &cdb_instance->kvdata[hkv_pos];
        sxe_cdb_hkv_unpack(tls_hkv, &sxe_cdb_tls_hkv_part);
    }

    SXEL6("%s(cdb_instance=?, uid=%010lx=ii[%04x]%03x-%01x){} // return %p", __FUNCTION__, uid.as_u64.u, uid.as_part.sheets_index_index, uid.as_part.row, uid.as_part.cell, tls_hkv);
    return tls_hkv;
} /* sxe_cdb_instance_get_uid_hkv_raw() */

SXE_CDB_HKV * /* NULL or tls SXE_CDB_HKV copy */
sxe_cdb_instance_get_uid_hkv(SXE_CDB_INSTANCE * cdb_instance, SXE_CDB_UID uid)
{
    SXE_CDB_HKV * tls_hkv = NULL; /* result */

    uint32_t cell        = uid.as_part.cell                       ; SXEA1(cell        < SXE_CDB_KEYS_PER_ROW     , "ERROR: INTERNAL: %u=cell        < %lu=SXE_CDB_KEYS_PER_ROW"    , cell       , SXE_CDB_KEYS_PER_ROW     );
    uint32_t row         = uid.as_part.row                        ; SXEA1(row         < SXE_CDB_ROWS_PER_SHEET   , "ERROR: INTERNAL: %u=row         < %lu=SXE_CDB_ROWS_PER_SHEET"  , row        , SXE_CDB_ROWS_PER_SHEET   );
    uint16_t sheet_index = uid.as_part.sheets_index_index         ; SXEA1(sheet_index < SXE_CDB_SHEETS_MAX       , "ERROR: INTERNAL: %u=sheet_index < %lu=SXE_CDB_SHEETS_MAX"      , sheet_index, SXE_CDB_SHEETS_MAX       );
    uint16_t sheet       = cdb_instance->sheets_index[sheet_index]; SXEA1(sheet       < cdb_instance->sheets_size, "ERROR: INTERNAL: %u=sheet       < %u=cdb_instance->sheets_size", sheet      , cdb_instance->sheets_size);
    if (cdb_instance->sheets[sheet].row[row].hkv_pos.u32[cell]) {
        uint32_t hkv_pos = cdb_instance->sheets[sheet].row[row].hkv_pos.u32[cell];
                 tls_hkv = sxe_cdb_copy_hkv_to_tls(cdb_instance, hkv_pos);
    }

    SXEL6("%s(cdb_instance=?, uid=%010lx=ii[%04x]%03x-%01x){} // return %p", __FUNCTION__, uid.as_u64.u, uid.as_part.sheets_index_index, uid.as_part.row, uid.as_part.cell, tls_hkv);
    return tls_hkv;
} /* sxe_cdb_instance_get_uid_hkv() */

void /* only call this function directly *after* sxe_cdb_instance_get_uid_hkv() */
sxe_cdb_instance_set_uid_hkv(SXE_CDB_INSTANCE * cdb_instance, SXE_CDB_UID uid)
{
    uint32_t cell        = uid.as_part.cell                       ; SXEA1(cell        < SXE_CDB_KEYS_PER_ROW     , "ERROR: INTERNAL: %u=cell        < %lu=SXE_CDB_KEYS_PER_ROW"    , cell       , SXE_CDB_KEYS_PER_ROW     );
    uint32_t row         = uid.as_part.row                        ; SXEA1(row         < SXE_CDB_ROWS_PER_SHEET   , "ERROR: INTERNAL: %u=row         < %lu=SXE_CDB_ROWS_PER_SHEET"  , row        , SXE_CDB_ROWS_PER_SHEET   );
    uint16_t sheet_index = uid.as_part.sheets_index_index         ; SXEA1(sheet_index < SXE_CDB_SHEETS_MAX       , "ERROR: INTERNAL: %u=sheet_index < %lu=SXE_CDB_SHEETS_MAX"      , sheet_index, SXE_CDB_SHEETS_MAX       );
    uint16_t sheet       = cdb_instance->sheets_index[sheet_index]; SXEA1(sheet       < cdb_instance->sheets_size, "ERROR: INTERNAL: %u=sheet       < %u=cdb_instance->sheets_size", sheet      , cdb_instance->sheets_size);
    if (cdb_instance->sheets[sheet].row[row].hkv_pos.u32[cell]) {
        uint32_t hkv_pos = cdb_instance->sheets[sheet].row[row].hkv_pos.u32[cell];
                           sxe_cdb_copy_tls_to_hkv(cdb_instance, hkv_pos);
    }

    SXEL6("%s(cdb_instance=?, uid=%010lx=ii[%04x]%03x-%01x){}", __FUNCTION__, uid.as_u64.u, uid.as_part.sheets_index_index, uid.as_part.row, uid.as_part.cell);
    return;
} /* sxe_cdb_instance_set_uid_hkv() */

void
sxe_cdb_instance_free_count_push(SXE_CDB_INSTANCE * cdb_instance, uint32_t from_c, uint32_t to___c)
{
    uint32_t this_c = to___c;
    while (this_c >= from_c) {
        uint32_t next_c                    = cdb_instance->counts_next_free;
        cdb_instance->counts_next_free     = this_c;
        cdb_instance->counts[this_c].next  = next_c;
        cdb_instance->counts_free         ++;
        this_c                            --;
    }

    SXEL6("%s(cdb_instance=?, from_c=%u, to___c=%u){}", __FUNCTION__, from_c, to___c);
    return;
} /* sxe_cdb_instance_free_count_push() */

uint32_t
sxe_cdb_instance_free_count_pop(SXE_CDB_INSTANCE * cdb_instance)
{
    uint32_t next_c;

    if (0 == cdb_instance->counts_free) {
        cdb_instance->counts_pages ++; /* count extra page */
        if (cdb_instance->counts) { cdb_instance->counts = mremap(cdb_instance->counts          , SXE_CDB_KERNEL_PAGE_BYTES * (cdb_instance->counts_pages - 1), SXE_CDB_KERNEL_PAGE_BYTES * cdb_instance->counts_pages, MREMAP_MAYMOVE                    ); }
        else                      { cdb_instance->counts =   mmap(NULL /* kernel chooses addr */, SXE_CDB_KERNEL_PAGE_BYTES *  cdb_instance->counts_pages     , PROT_READ | PROT_WRITE                                , MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); }
        SXEA1(MAP_FAILED != cdb_instance->counts, "ERROR: FATAL: expected m(re)map() not to fail // %s(){}", __FUNCTION__);

        cdb_instance->counts_size += 1 == cdb_instance->counts_pages ? 1 : 0; /* skip over item zero because it's used for SXE_CDB_COUNT_NONE */

        uint32_t counts_size_new = ((SXE_CDB_KERNEL_PAGE_BYTES * cdb_instance->counts_pages) / SXE_CDB_COUNT_BYTES) - cdb_instance->counts_size;
        SXEL7("%s(cdb_instance=?){} // ... growing ->counts by %u from %u to %u, ->counts_used=%u, ->counts_pages=%u, %lu page bytes unused ...", __FUNCTION__, counts_size_new, cdb_instance->counts_size, cdb_instance->counts_size + counts_size_new - 1, cdb_instance->counts_used, cdb_instance->counts_pages, (SXE_CDB_KERNEL_PAGE_BYTES * cdb_instance->counts_pages) - (SXE_CDB_COUNT_BYTES * (cdb_instance->counts_size + counts_size_new)));
        sxe_cdb_instance_free_count_push(cdb_instance, cdb_instance->counts_size, cdb_instance->counts_size + counts_size_new - 1);
        cdb_instance->counts_size += counts_size_new;
    }

    next_c = cdb_instance->counts_next_free;
    SXEA6(SXE_CDB_COUNT_NONE != next_c, "ERROR: INTERNAL: unexpected SXE_CDB_COUNT_NONE == next_c");
    cdb_instance->counts_next_free = cdb_instance->counts[next_c].next;
    cdb_instance->counts_free --;
    cdb_instance->counts_used ++;

    SXEL6("%s(cdb_instance=?){} // return %u=next_c", __FUNCTION__, next_c);
    return next_c;
} /* sxe_cdb_instance_free_count_pop() */

/**
 * For a given key, do something similar to incrementing a 64bit
 * counter, but in reality additionally keep a list of all keys
 * incremented, sorted by the counter value.
 */

uint64_t /* count or zero means e.g. key couldn't be put */
sxe_cdb_instance_inc(SXE_CDB_INSTANCE * cdb_instance, uint32_t counts_list)
{
    uint64_t         count_new = 0;
    SXE_CDB_HKV_PART this;
    SXE_CDB_HKV_PART last;
    SXE_CDB_HKV_PART next;

    SXEE6("(cdb_instance=?)");

    SXEA6(counts_list < SXE_CDB_COUNTS_LISTS_MAX, "ERROR: INTERNAL: expected counts_list < %u but got %u", SXE_CDB_COUNTS_LISTS_MAX, counts_list);

    SXE_CDB_HKV * this_hkv; uint32_t this_hkv_pos; SXE_CDB_HKV_LIST * this_val_ptr; uint32_t this_c;
    SXE_CDB_HKV * last_hkv; uint32_t next_hkv_pos; SXE_CDB_HKV_LIST * next_val_ptr; uint32_t last_c;
    SXE_CDB_HKV * next_hkv; uint32_t last_hkv_pos; SXE_CDB_HKV_LIST * last_val_ptr; uint32_t next_c;
    this_hkv = sxe_cdb_instance_get_hkv_raw(cdb_instance);
    if (NULL == this_hkv) {
        SXEL7("key doesn't exist; so create it!");
        uint8_t val_dummy[SXE_CDB_HKV_LIST_BYTES];
        if (SXE_CDB_UID_NONE != sxe_cdb_instance_put_val(cdb_instance, &val_dummy[0], sizeof(val_dummy))) { /* if key appended */
            this_hkv = sxe_cdb_instance_get_hkv_raw(cdb_instance);
            SXEA6(this_hkv, "ERROR: INTERNAL: did sxe_cdb_instance_put_val() but can't sxe_cdb_instance_get_hkv()");
            sxe_cdb_hkv_unpack(this_hkv, &this);

            this_hkv_pos = (uint8_t *) this_hkv - cdb_instance->kvdata;
            this_val_ptr = (SXE_CDB_HKV_LIST *) this.val;
            this_c       = cdb_instance->counts_lo[counts_list];

            if (SXE_CDB_COUNT_NONE == this_c) {
                SXEL7("no counts exist; so create the very first one & add our key to it");
                count_new                            = 1;
                this_c                               = sxe_cdb_instance_free_count_pop(cdb_instance);
                this_val_ptr->count_to_use           = this_c;
                this_val_ptr->next_hkv_pos           = SXE_CDB_HKV_POS_NONE;
                this_val_ptr->last_hkv_pos           = SXE_CDB_HKV_POS_NONE;
                cdb_instance->counts[this_c].count   = count_new;
                cdb_instance->counts[this_c].next    = SXE_CDB_COUNT_NONE;
                cdb_instance->counts[this_c].last    = SXE_CDB_COUNT_NONE;
                cdb_instance->counts[this_c].hkv1    = this_hkv_pos;
                cdb_instance->counts_lo[counts_list] = this_c;
                cdb_instance->counts_hi[counts_list] = this_c;
            }
            else if (1 < cdb_instance->counts[this_c].count) {
                SXEL7("lowest count %lu is still higher than desired count of 1; insert new count with count of 1", (uint64_t)cdb_instance->counts[this_c].count);
                count_new                            = 1;
                next_c                               = this_c;
                this_c                               = sxe_cdb_instance_free_count_pop(cdb_instance);
                this_val_ptr->count_to_use           = this_c;
                this_val_ptr->next_hkv_pos           = SXE_CDB_HKV_POS_NONE;
                this_val_ptr->last_hkv_pos           = SXE_CDB_HKV_POS_NONE;
                cdb_instance->counts[this_c].count   = count_new;
                cdb_instance->counts[this_c].next    = cdb_instance->counts_lo[counts_list];
                cdb_instance->counts[this_c].last    = SXE_CDB_COUNT_NONE;
                cdb_instance->counts[this_c].hkv1    = this_hkv_pos;
                cdb_instance->counts_lo[counts_list] = this_c;
                cdb_instance->counts[next_c].last    = this_c;
            }
            else {
                SXEA6(1 == cdb_instance->counts[this_c].count, "ERROR: INTERNAL: unexpected count %lu", (uint64_t)cdb_instance->counts[this_c].count);
                SXEL7("add key to the count '1' list of key(s)");
                count_new                          = 1;
                last_hkv_pos                       =                  cdb_instance->counts[this_c].hkv1;
                SXEA6(SXE_CDB_HKV_POS_NONE        != last_hkv_pos              , "ERROR: INTERNAL: SXE_CDB_HKV_POS_NONE == last_hkv_pos");
                last_hkv                           = (SXE_CDB_HKV *) &cdb_instance->kvdata[last_hkv_pos];
                sxe_cdb_hkv_unpack(last_hkv, &last);
                last_val_ptr                       = (SXE_CDB_HKV_LIST *)  last.val;
                SXEA6(SXE_CDB_HKV_POS_NONE        == last_val_ptr->last_hkv_pos, "ERROR: INTERNAL: SXE_CDB_HKV_POS_NONE != last_val_ptr->last_hkv_pos");
                last_val_ptr->last_hkv_pos         = this_hkv_pos;
                this_val_ptr->count_to_use         = this_c;
                this_val_ptr->next_hkv_pos         = last_hkv_pos;
                this_val_ptr->last_hkv_pos         = SXE_CDB_HKV_POS_NONE;
                cdb_instance->counts[this_c].hkv1  = this_hkv_pos;
            }
        } /* if sxe_cdb_instance_put() */
    } /* if sxe_cdb_instance_get() */
    else {
        sxe_cdb_hkv_unpack(this_hkv, &this);

        if (sizeof(SXE_CDB_HKV_LIST) != sxe_cdb_tls_hkv_part.val_len) {
            SXEL3("WARN:  %s(){} tried to increment key which is not a counter; early out without incrementing // value length is %u but expected %lu", __FUNCTION__, sxe_cdb_tls_hkv_part.val_len, sizeof(SXE_CDB_HKV_LIST));
            goto SXE_EARLY_OUT;
        }

        this_hkv_pos = (uint8_t *) this_hkv - cdb_instance->kvdata;
        this_val_ptr = (SXE_CDB_HKV_LIST *) this.val              ;
        this_c       = this_val_ptr->count_to_use                 ; SXEA6(SXE_CDB_COUNT_NONE != this_c, "ERROR: INTERNAL: unexpected SXE_CDB_COUNT_NONE");

        if (this_c >= cdb_instance->counts_size) {
            SXEL3("WARN:  %s(){} tried to increment key which is not a counter; early out without incrementing // referenced counter %u but only have %u counters", __FUNCTION__, this_c, cdb_instance->counts_size);
            goto SXE_EARLY_OUT;
        }

        next_c       = cdb_instance->counts[this_c].next          ;
        count_new    = cdb_instance->counts[this_c].count + 1     ;

        SXEL7("key exists; so 'increment' it to %lu", count_new);

        if     ((cdb_instance->counts[this_c].hkv1  == this_hkv_pos        )    /* if this count points to our key */
        &&      (this_val_ptr->next_hkv_pos         == SXE_CDB_HKV_POS_NONE)) { /* and this is the only key in the list */
            if ((                     next_c        == SXE_CDB_COUNT_NONE  )    /*   if there is no next highest count */
            ||  (cdb_instance->counts[next_c].count >  count_new           )) { /*   or next highest count is not the desired count */
                 cdb_instance->counts[this_c].count ++;                         /*   then opportunistically increment this count! */
                 SXEL7("opportunistically incremented associated count");
                 goto SXE_EARLY_OUT;                                          /*   and early out for this lone climbing count :-) */
            }
        }

        next_hkv_pos = this_val_ptr->next_hkv_pos                 ;
        last_hkv_pos = this_val_ptr->last_hkv_pos                 ;
        last_c       = cdb_instance->counts[this_c].last          ;

        SXEL7("remove hkv from this count hkv chain");

        if (next_hkv_pos != SXE_CDB_HKV_POS_NONE) { /* remove this_hkv from next_hkv in chain */
            next_hkv                   = (SXE_CDB_HKV      *) &cdb_instance->kvdata[next_hkv_pos]; sxe_cdb_hkv_unpack(next_hkv, &next);
            next_val_ptr               = (SXE_CDB_HKV_LIST *) next.val                           ; SXEA6(next_val_ptr->last_hkv_pos == this_hkv_pos, "ERROR: INTERNAL: expected %u==next_val_ptr->last but got %u // ->counts_used=%u", this_hkv_pos, next_val_ptr->last_hkv_pos, cdb_instance->counts_used);
            next_val_ptr->last_hkv_pos = last_hkv_pos;
        }

        if (last_hkv_pos != SXE_CDB_HKV_POS_NONE) { /* remove this_hkv from last_hkv in chain */
            last_hkv                   = (SXE_CDB_HKV      *) &cdb_instance->kvdata[last_hkv_pos]; sxe_cdb_hkv_unpack(last_hkv, &last);
            last_val_ptr               = (SXE_CDB_HKV_LIST *) last.val                           ; SXEA6(last_val_ptr->next_hkv_pos == this_hkv_pos, "ERROR: INTERNAL: expected %u==last_val_ptr->next but got %u // ->counts_used=%u", this_hkv_pos, last_val_ptr->next_hkv_pos, cdb_instance->counts_used);
            last_val_ptr->next_hkv_pos = next_hkv_pos;
        }

        if (cdb_instance->counts[this_c].hkv1 == this_hkv_pos) {
            cdb_instance->counts[this_c].hkv1  = next_hkv_pos; SXEL7("hkv was 1st in count list; update 1st hkv to 2nd in list");
        }

        if (SXE_CDB_HKV_POS_NONE == cdb_instance->counts[this_c].hkv1) {
            SXEL7("removing count %lu because no keys with this count", (uint64_t)cdb_instance->counts[this_c].count);

            sxe_cdb_instance_free_count_push(cdb_instance, this_c, this_c);
            cdb_instance->counts_used --;

            if (next_c != SXE_CDB_COUNT_NONE) { cdb_instance->counts[next_c].last    =                                                  last_c                                      ; }
            else                              { cdb_instance->counts_hi[counts_list] = cdb_instance->counts_hi[counts_list] == this_c ? last_c : cdb_instance->counts_hi[counts_list]; SXEA1(0, "/* COVERAGE EXCLUSION: impossible code path due to opportunistic increment (see above) */"); }

            if (last_c != SXE_CDB_COUNT_NONE) { cdb_instance->counts[last_c].next    =                                                  next_c                                      ; }
            else                              { cdb_instance->counts_lo[counts_list] = cdb_instance->counts_lo[counts_list] == this_c ? next_c : cdb_instance->counts_lo[counts_list]; }
        }

        /* add hkv to next count hkv chain */

        if (SXE_CDB_COUNT_NONE == next_c) {
            SXEL7("higher count: doesn't exist; so create it & add our key to it");
            next_c                               = sxe_cdb_instance_free_count_pop(cdb_instance);
            this_val_ptr->count_to_use           = next_c;
            this_val_ptr->next_hkv_pos           = SXE_CDB_HKV_POS_NONE;
            this_val_ptr->last_hkv_pos           = SXE_CDB_HKV_POS_NONE;
            cdb_instance->counts_hi[counts_list] = next_c;
            cdb_instance->counts[next_c].count   = count_new; /* 'increment' count */
            cdb_instance->counts[next_c].next    = SXE_CDB_COUNT_NONE;
            cdb_instance->counts[next_c].last    = this_c;
            cdb_instance->counts[next_c].hkv1    = this_hkv_pos;
            cdb_instance->counts[this_c].next    = next_c;
        }
        else {
            if (cdb_instance->counts[next_c].count == count_new) {
                SXEL7("higher count: exists; it's the right count, so add our key to it");
                this_val_ptr->count_to_use         = next_c;
                this_val_ptr->next_hkv_pos         = cdb_instance->counts[next_c].hkv1;
                this_val_ptr->last_hkv_pos         = SXE_CDB_HKV_POS_NONE;

                next_hkv_pos                       = cdb_instance->counts[next_c].hkv1;
                next_hkv                           = (SXE_CDB_HKV      *) &cdb_instance->kvdata[next_hkv_pos]; sxe_cdb_hkv_unpack(next_hkv, &next);
                next_val_ptr                       = (SXE_CDB_HKV_LIST *) next.val                           ; SXEA6(next_val_ptr->last_hkv_pos == SXE_CDB_HKV_POS_NONE, "ERROR: INTERNAL: expected SXE_CDB_HKV_POS_NONE==next_val_ptr->last but got %u // ->counts_used=%u", next_val_ptr->last_hkv_pos, cdb_instance->counts_used);
                next_val_ptr->last_hkv_pos         = this_hkv_pos;

                cdb_instance->counts[next_c].hkv1  = this_hkv_pos;
            }
            else {
                SXEL7("higher count: exists; count %lu is higher than wanted, so insert a new count %lu & add our key to it", (uint64_t)cdb_instance->counts[next_c].count, count_new);
                uint32_t nex2_c                    = next_c;
                next_c                             = sxe_cdb_instance_free_count_pop(cdb_instance);
                this_val_ptr->count_to_use         = next_c;
                this_val_ptr->next_hkv_pos         = SXE_CDB_HKV_POS_NONE;
                this_val_ptr->last_hkv_pos         = SXE_CDB_HKV_POS_NONE;
                cdb_instance->counts[next_c].count = count_new; /* 'increment' count */
                cdb_instance->counts[next_c].next  = cdb_instance->counts[this_c].next;
                cdb_instance->counts[next_c].last  = this_c;
                cdb_instance->counts[next_c].hkv1  = this_hkv_pos;
                cdb_instance->counts[this_c].next  = next_c;
                cdb_instance->counts[nex2_c].last  = next_c;
            }
        }

        SXEL7("existing count is %lu", (uint64_t)cdb_instance->counts[this_c].count);
    }

SXE_EARLY_OUT:;
    SXER6("return %lu // key count", count_new);
    return count_new;
} /* sxe_cdb_instance_inc() */

int
sxe_cdb_instance_walk_pos_is_bad(
    SXE_CDB_INSTANCE * cdb_instance,
    uint32_t           cnt_pos     , /* SXE_CDB_COUNT_NONE   means start of list, or current index for count */
    uint32_t           hkv_pos     , /* SXE_CDB_HKV_POS_NONE means start of list, or current hkv   for count */
    const char       * warning_hint)
{
    int result = 1;

    if (SXE_CDB_COUNT_NONE != cnt_pos) { /* if valid looking cnt */
        if (SXE_CDB_HKV_POS_NONE != hkv_pos) { /* if valid looking hkv */
            /* scrutinize *_pos as they may have come from outside lib-sxe-cdb ! */
            if (cnt_pos > cdb_instance->counts_size) {
                SXEL3("%s(cdb_instance=?, cnt_pos=%u, hkv_pos=%u){} // WARNING: given cnt_pos is out of range; early out // %s; ->counts_size=%u", __FUNCTION__, cnt_pos, hkv_pos, warning_hint, cdb_instance->counts_size);
                goto SXE_EARLY_OUT;
            }

            if ((hkv_pos + 1) > cdb_instance->kvdata_used) {
                SXEL3("%s(cdb_instance=?, cnt_pos=%u, hkv_pos=%u){} // WARNING: given hkv_pos is out of range; early out // %s", __FUNCTION__, cnt_pos, hkv_pos, warning_hint);
                goto SXE_EARLY_OUT;
            }

            SXE_CDB_HKV * hkv   = (SXE_CDB_HKV *) &cdb_instance->kvdata[hkv_pos];
            uint32_t      h_len = sxe_cdb_h_len(hkv);
            if ((hkv_pos + h_len) > cdb_instance->kvdata_used) {
                SXEL3("%s(cdb_instance=?, cnt_pos=%u, hkv_pos=%u){} // WARNING: given hkv_pos + header len is out of range; early out // %s", __FUNCTION__, cnt_pos, hkv_pos, warning_hint);
                goto SXE_EARLY_OUT;
            }

            sxe_cdb_hkv_unpack(hkv, &sxe_cdb_tls_hkv_part);
            if ((hkv_pos + sxe_cdb_tls_hkv_part.hkv_len) > cdb_instance->kvdata_used) {
                SXEL3("%s(cdb_instance=?, cnt_pos=%u, hkv_pos=%u){} // WARNING: given hkv_pos + %u=hkv_len is out of range; early out // %s", __FUNCTION__, cnt_pos, hkv_pos, sxe_cdb_tls_hkv_part.hkv_len, warning_hint);
                goto SXE_EARLY_OUT;
            }

            SXE_CDB_HKV_LIST * val_ptr = (SXE_CDB_HKV_LIST *) sxe_cdb_tls_hkv_part.val;
            if (sxe_cdb_tls_hkv_part.val_len == sizeof(SXE_CDB_HKV_LIST) && val_ptr->count_to_use != cnt_pos) {
                SXEL3("%s(cdb_instance=?, cnt_pos=%u, hkv_pos=%u){} // WARNING: val_len incorrect, or, value as SXE_CDB_HKV_LIST does not reference cnt_pos; early out // %s", __FUNCTION__, cnt_pos, hkv_pos, warning_hint);
                goto SXE_EARLY_OUT;
            }
        }
    }

    result = 0; /* *_pos doesn't look bad :-) */
    SXEL6("%s(cdb_instance=?, cnt_pos=%u, hkv_pos=%u){} // %u=%s", __FUNCTION__, cnt_pos, hkv_pos, result, result ? "is true" : "is false");

SXE_EARLY_OUT:;
    return result;
} /* sxe_cdb_instance_walk_pos_is_bad() */

SXE_CDB_HKV * /* NULL or tls SXE_CDB_HKV copy */
sxe_cdb_instance_walk(
    SXE_CDB_INSTANCE * cdb_instance,
    uint32_t           direction   , /* n=hi2lo, 0=lo2hi */
    uint32_t           cnt_pos     , /* SXE_CDB_COUNT_NONE   means start of list, or current index for count */
    uint32_t           hkv_pos     , /* SXE_CDB_HKV_POS_NONE means start of list, or current hkv   for count */
    uint32_t           counts_list )
{
    SXE_CDB_HKV * tls_hkv = NULL; /* result */

    if (SXE_CDB_COUNT_NONE == cnt_pos) { /* if start of list */
        if (counts_list >= SXE_CDB_COUNTS_LISTS_MAX) {
            SXEL3("%s(cdb_instance=?, direction=%u, cnt_pos=%u, hkv_pos=%u, counts_list=%u){} // WARNING: given counts_list is out of range; early out", __FUNCTION__, direction, cnt_pos, hkv_pos, counts_list);
            cnt_pos = SXE_CDB_COUNT_NONE; /* fake end of list */
            hkv_pos = SXE_CDB_HKV_POS_NONE;
            goto SXE_EARLY_OUT;
        }
        cnt_pos = direction                     ? cdb_instance->counts_hi[counts_list] : cdb_instance->counts_lo[counts_list]; /* where to start? */
        hkv_pos = SXE_CDB_COUNT_NONE == cnt_pos ? SXE_CDB_HKV_POS_NONE    : cdb_instance->counts[cnt_pos].hkv1;
    }

    if (SXE_CDB_COUNT_NONE != cnt_pos) {
        if (SXE_CDB_HKV_POS_NONE != hkv_pos) { /* if valid looking hkv */
            if (sxe_cdb_instance_walk_pos_is_bad(cdb_instance, cnt_pos, hkv_pos, "validating hkv_pos given")) { /* todo: consider having a faster, 'unsafe' sxe_cdb_instance_walk() without this check */
                cnt_pos = SXE_CDB_COUNT_NONE  ; /* fake end of list */
                hkv_pos = SXE_CDB_HKV_POS_NONE;
                goto SXE_EARLY_OUT;
            }

            /* come here if given hkv_pos appears to reference a valid SXE_CDB_HKV_LIST which references a valid cnt_pos */
            SXE_CDB_HKV_LIST * val_ptr     = (SXE_CDB_HKV_LIST *) sxe_cdb_tls_hkv_part.val;
            uint32_t           hkv_pos_new = val_ptr->next_hkv_pos;

            if (sxe_cdb_instance_walk_pos_is_bad(cdb_instance, cnt_pos, hkv_pos_new, "validating hkv_pos discovered")) {/* todo: consider having a faster, 'unsafe' sxe_cdb_instance_walk() without this check */
                cnt_pos = SXE_CDB_COUNT_NONE  ; /* fake end of list */
                hkv_pos = SXE_CDB_HKV_POS_NONE; /* COVERAGE EXCLUSION: todo: create unit test for this paranoid check */
                goto SXE_EARLY_OUT;
            }

            /* come here if discovered hkv_pos appears to reference a valid SXE_CDB_HKV_LIST which references a valid cnt_pos */
            tls_hkv                = sxe_cdb_copy_hkv_to_tls(cdb_instance, hkv_pos);
            sxe_cdb_tls_walk_count = cdb_instance->counts[cnt_pos].count;
            hkv_pos                = hkv_pos_new;
            SXEL6("dumping key with count %lu: [%u]=%.*s // hkv_pos=%u", sxe_cdb_tls_walk_count, sxe_cdb_tls_hkv_part.key_len, sxe_cdb_tls_hkv_part.key_len, sxe_cdb_tls_hkv_part.key, hkv_pos);
        }
        if (SXE_CDB_HKV_POS_NONE == hkv_pos) { /* if no new hkv then advance to next count */
            cnt_pos = direction                     ? cdb_instance->counts[cnt_pos].last : cdb_instance->counts[cnt_pos].next; /* move in count direction */
            hkv_pos = SXE_CDB_COUNT_NONE == cnt_pos ? SXE_CDB_HKV_POS_NONE               : cdb_instance->counts[cnt_pos].hkv1;
        }
    }

SXE_EARLY_OUT:;
    sxe_cdb_tls_walk_cnt_pos = cnt_pos; /* SXE_CDB_COUNT_NONE means reached end of list */
    sxe_cdb_tls_walk_hkv_pos = hkv_pos;

    return tls_hkv;
} /* sxe_cdb_instance_walk() */

SXE_CDB_ENSEMBLE *
sxe_cdb_ensemble_new(
    uint32_t keys_at_start ,
    uint64_t kvdata_maximum,
    uint32_t cdb_count     ,
    uint32_t cdb_is_locked )
{
    SXE_CDB_ENSEMBLE * cdb_ensemble = NULL;

    SXEE6("(keys_at_start=%u, kvdata_maximum=%lu, cdb_count=%u, cdb_is_locked=%u)", keys_at_start, kvdata_maximum, cdb_count,
          cdb_is_locked);

    while (SXE_SPINLOCK_STATUS_TAKEN != sxe_spinlock_take(&sxe_cdb_ensemble_lock)) {
        SXEL5("%s() failed to acquire sxe_cdb_ensemble_lock; trying again", __FUNCTION__); /* COVERAGE EXCLUSION: todo: create multi-threaded test to show this informational lock message */
    }

    if (cdb_count <= 256) { /* limit uid size to 5 bytes; max. kvdata space is 256 * 4GB or 1,024GB */
        cdb_ensemble = sxe_malloc(sizeof(*cdb_ensemble));
        SXEA1(cdb_ensemble, ": sxe_malloc() failed for %zu bytes", sizeof(*cdb_ensemble));
        cdb_ensemble->cdb_instances      = sxe_malloc(cdb_count * sizeof(cdb_ensemble->cdb_instances));
        cdb_ensemble->cdb_instance_locks = sxe_malloc(cdb_count * sizeof(cdb_ensemble->cdb_instance_locks));
        SXEA1(cdb_ensemble->cdb_instances, ": sxe_malloc() failed for %zu bytes",
              cdb_count * sizeof(cdb_ensemble->cdb_instances));
        SXEA1(cdb_ensemble->cdb_instance_locks, ": sxe_malloc() failed for %zu bytes",
              cdb_count * sizeof(cdb_ensemble->cdb_instance_locks));
        SXEL6("cdb_ensemble                     = sxe_malloc(%zu)",                  sizeof(*cdb_ensemble));
        SXEL6("cdb_ensemble->cdb_instances      = sxe_malloc(%u * %zu)", cdb_count , sizeof(cdb_ensemble->cdb_instances));
        SXEL6("cdb_ensemble->cdb_instance_locks = sxe_malloc(%u * %zu)", cdb_count , sizeof(cdb_ensemble->cdb_instance_locks));

        SXEL6("creating array of cdb pointers, each with its own lock:");
        uint32_t i;
        for (i = 0; i < cdb_count; i++) {
            cdb_ensemble->cdb_instances[i] = sxe_cdb_instance_new(keys_at_start / cdb_count, kvdata_maximum / cdb_count);
            sxe_spinlock_construct(&cdb_ensemble->cdb_instance_locks[i]); /* in case we need locks */
        }
        cdb_ensemble->cdb_count     = cdb_count;
        cdb_ensemble->cdb_is_locked = cdb_is_locked ? 1 : 0;
    }

    sxe_spinlock_give(&sxe_cdb_ensemble_lock);

    SXER6("return %p=cdb_ensemble", cdb_ensemble);
    return cdb_ensemble;
} /* sxe_cdb_ensemble_new() */

void
sxe_cdb_ensemble_destroy(SXE_CDB_ENSEMBLE * cdb_ensemble)
{
    uint32_t i;
    SXEE6("(cdb_ensemble=?)");

    while (SXE_SPINLOCK_STATUS_TAKEN != sxe_spinlock_take(&sxe_cdb_ensemble_lock)) {
        SXEL5("%s() failed to acquire sxe_cdb_ensemble_lock; trying again", __FUNCTION__); /* COVERAGE EXCLUSION: todo: create multi-threaded test to show this informational lock message */
    }

    SXEL6("destroying array of cdb pointers:");
    for (i = 0; i < cdb_ensemble->cdb_count; i++) {
        sxe_cdb_instance_destroy(cdb_ensemble->cdb_instances[i]);
    }

    sxe_free(cdb_ensemble->cdb_instance_locks);
    sxe_free(cdb_ensemble->cdb_instances);
    sxe_free(cdb_ensemble);

    sxe_spinlock_give(&sxe_cdb_ensemble_lock);

    SXER6("return");
} /* sxe_cdb_ensemble_destroy() */

#define SXE_CDB_FUNCTION(NAME) #NAME

#define SXE_CDB_ENSEMBLE_INSTANCE_LOCK_BEFORE(CDB_ENSEMBLE,FUNCTION) \
    if (CDB_ENSEMBLE->cdb_is_locked) { \
        SXEL7("about to lock instance %u of %u for %s()", instance, CDB_ENSEMBLE->cdb_count, SXE_CDB_FUNCTION(FUNCTION)); \
        while (SXE_SPINLOCK_STATUS_TAKEN != sxe_spinlock_take(&CDB_ENSEMBLE->cdb_instance_locks[instance])) { \
            SXEL5("%s() failed to acquire lock instance %u of %u for %s; trying again", __FUNCTION__, instance, CDB_ENSEMBLE->cdb_count, SXE_CDB_FUNCTION(FUNCTION)); \
        } \
    }

#define SXE_CDB_ENSEMBLE_INSTANCE_UNLOCK(CDB_ENSEMBLE) \
    if (CDB_ENSEMBLE->cdb_is_locked) { \
        sxe_spinlock_give(&CDB_ENSEMBLE->cdb_instance_locks[instance]); \
    }

void
sxe_cdb_ensemble_reboot(SXE_CDB_ENSEMBLE * cdb_ensemble)
{
    uint32_t instance;
    SXEE6("(cdb_ensemble=?)");

    while (SXE_SPINLOCK_STATUS_TAKEN != sxe_spinlock_take(&sxe_cdb_ensemble_lock)) {
        SXEL5("%s() failed to acquire sxe_cdb_ensemble_lock; trying again", __FUNCTION__); /* COVERAGE EXCLUSION: todo: create multi-threaded test to show this informational lock message */
    }

    SXEL6("rebooting array of cdb pointers:");
    for (instance = 0; instance < cdb_ensemble->cdb_count; instance++) {
        SXE_CDB_ENSEMBLE_INSTANCE_LOCK_BEFORE(cdb_ensemble, sxe_cdb_ensemble_reboot);
    }

    for (instance = 0; instance < cdb_ensemble->cdb_count; instance++) {
        sxe_cdb_instance_reboot(cdb_ensemble->cdb_instances[instance]);
    }

    for (instance = 0; instance < cdb_ensemble->cdb_count; instance++) {
        SXE_CDB_ENSEMBLE_INSTANCE_UNLOCK(cdb_ensemble);
    }

    sxe_spinlock_give(&sxe_cdb_ensemble_lock);

    SXER6("return");
} /* sxe_cdb_ensemble_reboot() */

/**
 * USE WITH CAUTION: Caller responsible for unlock!
 *
 * Why would caller want to live dangerously like this?
 *
 * Whereas the other get/set functions copy to/from a tls buffer
 * so that the caller never touches the bytes which can possibly
 * get mremap()ed, sxe_cdb_ensemble_get_hkv_locked() returns a
 * pointer to a SXE_CDB_HKV structure which contains pointers to
 * those mremap()able key value bytes but never unlocks itself
 * therefore allowing the caller to operate on the bytes
 * directly and then unlock after.
 *
 * Example use case: The key and/or value is large enough that
 * we do not want to incur the cost of copying it into a tls
 * buffer for access.
 */

SXE_CDB_HKV * /* NULL or *dangerous* direct pointer to original SXE_CDB_HKV; header + key + value bytes; caller responsible for later calling sxe_cdb_ensemble_get_hkv_unlock() */
sxe_cdb_ensemble_get_hkv_raw_locked(SXE_CDB_ENSEMBLE * cdb_ensemble)
{
    SXE_CDB_HKV * tls_hkv = NULL; /* result */

    SXEE6("(cdb_ensemble=?)");

    uint32_t               instance = sxe_cdb_hash.u16[3] % cdb_ensemble->cdb_count;
    SXE_CDB_INSTANCE * cdb_instance =                       cdb_ensemble->cdb_instances[instance];

    SXE_CDB_ENSEMBLE_INSTANCE_LOCK_BEFORE(cdb_ensemble, sxe_cdb_instance_get_hkv_raw function);
    tls_hkv =                                           sxe_cdb_instance_get_hkv_raw(cdb_instance);
//  SXE_CDB_ENSEMBLE_INSTANCE_UNLOCK(     cdb_ensemble);

    /* todo: consider asserting in other functions if they are called before sxe_cdb_ensemble_get_hkv_unlock() */

    SXER6("return tls_hkv=%p (%s); sxe_cdb_tls_hkv_part: .key_len=%u  .val_len=%u", tls_hkv, NULL == tls_hkv ? "key doesn't exist" : "key exists", sxe_cdb_tls_hkv_part.key_len, sxe_cdb_tls_hkv_part.val_len);
    return tls_hkv;
} /* sxe_cdb_ensemble_get_hkv_raw_locked() */

void
sxe_cdb_ensemble_get_hkv_raw_unlock(SXE_CDB_ENSEMBLE * cdb_ensemble)
{
    SXEE6("(cdb_ensemble=?)");

    uint32_t               instance = sxe_cdb_hash.u16[3] % cdb_ensemble->cdb_count;
//  SXE_CDB_INSTANCE * cdb_instance =                       cdb_ensemble->cdb_instances[instance];

//  SXE_CDB_ENSEMBLE_INSTANCE_LOCK_BEFORE(cdb_ensemble, sxe_cdb_instance_get_hkv_raw function);
//  tls_hkv =                                           sxe_cdb_instance_get_hkv_raw(cdb_instance);
    SXE_CDB_ENSEMBLE_INSTANCE_UNLOCK(     cdb_ensemble);

    SXER6("return");
} /* sxe_cdb_ensemble_get_hkv_raw_unlock() */

uint64_t /* SXE_CDB_UID */
sxe_cdb_ensemble_get_uid(SXE_CDB_ENSEMBLE * cdb_ensemble)
{
    SXE_CDB_UID uid;

    SXEE6("(cdb_ensemble=?)");

    uint32_t               instance = sxe_cdb_hash.u16[3] % cdb_ensemble->cdb_count;
    SXE_CDB_INSTANCE * cdb_instance =                       cdb_ensemble->cdb_instances[instance];

    SXE_CDB_ENSEMBLE_INSTANCE_LOCK_BEFORE(cdb_ensemble, sxe_cdb_instance_get_uid function);
    uid.as_u64.u =                                      sxe_cdb_instance_get_uid(cdb_instance);
    SXE_CDB_ENSEMBLE_INSTANCE_UNLOCK(     cdb_ensemble);

    uid.as_part.instance = SXE_CDB_UID_NONE == uid.as_u64.u ? uid.as_part.instance : instance; /* set instance if uid exists */

    SXER6("return uid=%010lx=%02x[%04x]%03x-%01x=%s // sxe_cdb_tls_hkv_part.val_len=%u", uid.as_u64.u, uid.as_part.instance, uid.as_part.sheets_index_index, uid.as_part.row, uid.as_part.cell, SXE_CDB_UID_NONE == uid.as_u64.u ? "key doesn't exist" : "key exists", sxe_cdb_tls_hkv_part.val_len);
    return uid.as_u64.u;
} /* sxe_cdb_ensemble_get_uid() */

SXE_CDB_HKV * /* NULL or tls SXE_CDB_HKV raw; not copy */
sxe_cdb_ensemble_get_uid_hkv_raw_locked(SXE_CDB_ENSEMBLE * cdb_ensemble, SXE_CDB_UID uid)
{
    SXE_CDB_HKV * tls_hkv = NULL; /* result */

    SXEE6("(cdb_ensemble=?, uid=%010lx=%02x[%04x]%03x-%01x", uid.as_u64.u, uid.as_part.instance, uid.as_part.sheets_index_index, uid.as_part.row, uid.as_part.cell);

    uint32_t               instance = uid.as_part.instance;
    SXE_CDB_INSTANCE * cdb_instance = cdb_ensemble->cdb_instances[instance];

    SXE_CDB_ENSEMBLE_INSTANCE_LOCK_BEFORE(cdb_ensemble, sxe_cdb_instance_get_uid_hkv_raw function);
    tls_hkv =                                           sxe_cdb_instance_get_uid_hkv_raw(cdb_instance, uid);
//  SXE_CDB_ENSEMBLE_INSTANCE_UNLOCK(     cdb_ensemble);

    /* todo: consider asserting in other functions if they are called before sxe_cdb_ensemble_get_hkv_unlock() */

    SXER6("return tls_hkv=%p (%s); sxe_cdb_tls_hkv_part: .key_len=%u  .val_len=%u", tls_hkv, NULL == tls_hkv ? "key doesn't exist" : "key exists", sxe_cdb_tls_hkv_part.key_len, sxe_cdb_tls_hkv_part.val_len);
    return tls_hkv;
} /* sxe_cdb_ensemble_get_uid_hkv_raw_locked() */

void
sxe_cdb_ensemble_get_uid_hkv_raw_unlock(SXE_CDB_ENSEMBLE * cdb_ensemble, SXE_CDB_UID uid)
{
    SXEE6("(cdb_ensemble=?, uid=%010lx=%02x[%04x]%03x-%01x", uid.as_u64.u, uid.as_part.instance, uid.as_part.sheets_index_index, uid.as_part.row, uid.as_part.cell);

    uint32_t               instance = uid.as_part.instance;
//  SXE_CDB_INSTANCE * cdb_instance = cdb_ensemble->cdb_instances[instance];

//  SXE_CDB_ENSEMBLE_INSTANCE_LOCK_BEFORE(cdb_ensemble, sxe_cdb_instance_get_uid_hkv_raw function);
//  tls_hkv =                                           sxe_cdb_instance_get_uid_hkv_raw(cdb_instance, uid);
    SXE_CDB_ENSEMBLE_INSTANCE_UNLOCK(     cdb_ensemble);

    SXER6("return");
} /* sxe_cdb_ensemble_get_uid_hkv_raw_locked() */

SXE_CDB_HKV * /* NULL or tls SXE_CDB_HKV copy */
sxe_cdb_ensemble_get_uid_hkv(SXE_CDB_ENSEMBLE * cdb_ensemble, SXE_CDB_UID uid)
{
    SXE_CDB_HKV * tls_hkv = NULL; /* result */

    SXEE6("(cdb_ensemble=?, uid=%010lx=%02x[%04x]%03x-%01x", uid.as_u64.u, uid.as_part.instance, uid.as_part.sheets_index_index, uid.as_part.row, uid.as_part.cell);

    uint32_t               instance = uid.as_part.instance;
    SXE_CDB_INSTANCE * cdb_instance = cdb_ensemble->cdb_instances[instance];

    SXE_CDB_ENSEMBLE_INSTANCE_LOCK_BEFORE(cdb_ensemble, sxe_cdb_instance_get_uid_hkv function);
    tls_hkv =                                           sxe_cdb_instance_get_uid_hkv(cdb_instance, uid);
    SXE_CDB_ENSEMBLE_INSTANCE_UNLOCK(     cdb_ensemble);

    SXER6("return tls_hkv=%p // sxe_cdb_tls_hkv_part.val_len=%u", tls_hkv, sxe_cdb_tls_hkv_part.val_len);
    return tls_hkv;
} /* sxe_cdb_ensemble_get_uid_hkv() */

void /* only call this function directly *after* sxe_cdb_ensemble_set_uid_hkv() */
sxe_cdb_ensemble_set_uid_hkv(SXE_CDB_ENSEMBLE * cdb_ensemble, SXE_CDB_UID uid)
{
    SXEE6("(cdb_ensemble=?, uid=%010lx=%02x[%04x]%03x-%01x", uid.as_u64.u, uid.as_part.instance, uid.as_part.sheets_index_index, uid.as_part.row, uid.as_part.cell);

    uint32_t               instance = uid.as_part.instance;
    SXE_CDB_INSTANCE * cdb_instance = cdb_ensemble->cdb_instances[instance];

    SXE_CDB_ENSEMBLE_INSTANCE_LOCK_BEFORE(cdb_ensemble, sxe_cdb_instance_set_uid_hkv function);
                                                        sxe_cdb_instance_set_uid_hkv(cdb_instance, uid);
    SXE_CDB_ENSEMBLE_INSTANCE_UNLOCK(     cdb_ensemble);

    SXER6("return // sxe_cdb_tls_hkv_part.val_len=%u", sxe_cdb_tls_hkv_part.val_len);
    return;
} /* sxe_cdb_ensemble_set_uid_hkv() */

uint64_t /* SXE_CDB_UID; SXE_CDB_UID_NONE means something went wrong and key not appended */
sxe_cdb_ensemble_put_val(SXE_CDB_ENSEMBLE * cdb_ensemble, const uint8_t * val, uint32_t val_len)
{
    SXE_CDB_UID uid;

    uint32_t               instance = sxe_cdb_hash.u16[3] % cdb_ensemble->cdb_count;
    SXE_CDB_INSTANCE * cdb_instance =                       cdb_ensemble->cdb_instances[instance];

    SXEE6("(cdb_ensemble=?, val=?, val_len=%u) // instance=%u", val_len, instance);

    SXE_CDB_ENSEMBLE_INSTANCE_LOCK_BEFORE(cdb_ensemble, sxe_cdb_instance_put_val function);
    uid.as_u64.u =                                      sxe_cdb_instance_put_val(cdb_instance, val, val_len);
    SXE_CDB_ENSEMBLE_INSTANCE_UNLOCK(     cdb_ensemble);

    uid.as_part.instance = SXE_CDB_UID_NONE == uid.as_u64.u ? uid.as_part.instance : instance; /* set instance if uid exists */

    SXER6("return uid=%010lx=%02x[%04x]%03x-%01x=%s", uid.as_u64.u, uid.as_part.instance, uid.as_part.sheets_index_index, uid.as_part.row, uid.as_part.cell, SXE_CDB_UID_NONE == uid.as_u64.u ? "failure" : "success");
    return uid.as_u64.u;
} /* sxe_cdb_ensemble_put_val() */

uint64_t /* count or zero means e.g. key couldn't be put */
sxe_cdb_ensemble_inc(SXE_CDB_ENSEMBLE * cdb_ensemble, uint32_t counts_list)
{
    uint64_t count_new;

    uint32_t               instance = sxe_cdb_hash.u16[3] % cdb_ensemble->cdb_count;
    SXE_CDB_INSTANCE * cdb_instance =                       cdb_ensemble->cdb_instances[instance];

    SXEE6("(cdb_ensemble=?) // instance=%u", instance);

    SXE_CDB_ENSEMBLE_INSTANCE_LOCK_BEFORE(cdb_ensemble, sxe_cdb_instance_inc function);
    count_new =                                         sxe_cdb_instance_inc(cdb_instance, counts_list);
    SXE_CDB_ENSEMBLE_INSTANCE_UNLOCK(     cdb_ensemble);

    SXER6("return %lu // key count", count_new);
    return count_new;
} /* sxe_cdb_ensemble_inc() */

/*
 * Note: sxe_cdb_ensemble_walk() contains no locks.
 * Why? Because walking while updating will not guarantee walking iterates over all counter keys.
 * Therefore, the caller should ensure that no updating happens during walking.
 */

SXE_CDB_HKV * /* NULL or tls SXE_CDB_HKV copy */
sxe_cdb_ensemble_walk(
    SXE_CDB_ENSEMBLE * cdb_ensemble,
    uint32_t           direction   , /* n=hi2lo, 0=lo2hi */
    uint32_t           cnt_pos     , /* SXE_CDB_COUNT_NONE   means start of list, or current index for count */
    uint32_t           hkv_pos     , /* SXE_CDB_HKV_POS_NONE means start of list, or current hkv   for count */
    uint32_t           count_list  ,
    uint32_t           instance    )
{
    SXE_CDB_HKV * tls_hkv = NULL; /* result */

    if (instance < cdb_ensemble->cdb_count) {
        SXE_CDB_INSTANCE * cdb_instance = cdb_ensemble->cdb_instances[instance];
        tls_hkv = sxe_cdb_instance_walk(cdb_instance, direction, cnt_pos, hkv_pos, count_list);
    }

    return tls_hkv;
} /* sxe_cdb_ensemble_walk() */

uint32_t /* 0 if invalid instance or ->kvdata_used */
sxe_cdb_ensemble_kvdata_used(
    SXE_CDB_ENSEMBLE * cdb_ensemble,
    uint32_t           instance    )
{
    uint32_t kvdata_used = 0;

    if (instance < cdb_ensemble->cdb_count) {
        SXE_CDB_INSTANCE * cdb_instance = cdb_ensemble->cdb_instances[instance];
        kvdata_used = cdb_instance->kvdata_used;
    }

    return kvdata_used;
} /* sxe_cdb_ensemble_kvdata_used() */

/**
 * Given two sxe cdb ensembles then swap the underlying
 * instances (using locking if the ensembles were initially
 * created using locks).
 *
 * Example use case: A bunch of threads use a locked sxe cdb
 * ensemble for reading (and maybe a bit of writing). While
 * another thread creates a brand new unlocked (read: faster
 * creation) sxe cdb ensemble that it wants the threads to start
 * using. This function can be used to swap in the underlying
 * instances in a locked way.
 */

void
sxe_cdb_ensemble_swap_instances(
    SXE_CDB_ENSEMBLE * this_cdb_ensemble, /* swap SXE_CDB_INSTANCEs in this SXE_CDB_ENSEMBLE */
    SXE_CDB_ENSEMBLE * that_cdb_ensemble) /* with SXE_CDB_INSTANCEs in that SXE_CDB_ENSEMBLE */
{
    SXEA1(this_cdb_ensemble->cdb_count == that_cdb_ensemble->cdb_count, "ERROR: cannot swap SXE_CDB_INSTANCEs of two SXE_CDB_ENSEMBLEs with differing numbers of instances!");

    uint32_t instance;
    for (instance = 0; instance < this_cdb_ensemble->cdb_count; instance++) {
        SXE_CDB_ENSEMBLE_INSTANCE_LOCK_BEFORE(this_cdb_ensemble, this_cdb_ensemble);
        SXE_CDB_ENSEMBLE_INSTANCE_LOCK_BEFORE(that_cdb_ensemble, that_cdb_ensemble);
        struct SXE_CDB_INSTANCE * temp             = this_cdb_ensemble->cdb_instances[instance];
        this_cdb_ensemble->cdb_instances[instance] = that_cdb_ensemble->cdb_instances[instance];
        that_cdb_ensemble->cdb_instances[instance] = temp;
        SXE_CDB_ENSEMBLE_INSTANCE_UNLOCK(     that_cdb_ensemble);
        SXE_CDB_ENSEMBLE_INSTANCE_UNLOCK(     this_cdb_ensemble);

    }
} /* sxe_cdb_ensemble_swap_instances() */
