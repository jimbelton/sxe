#include <stdio.h>
#include <string.h>
#include "md5.h"
#include "tap.h"

/*
 *  Define patterns for testing
 */
#define TESTA   "abc"
#define TESTB_1 "abcdbcdecdefdefgefghfghighij"
#define TESTB_2 "hijkijkljklmklmnlmnomnopnopq"
#define TESTB   TESTB_1 TESTB_2
#define TESTC   "a"
#define TESTQ   "The quick brown fox jumps over the lazy dog"
#define TESTL   TESTQ TESTQ TESTQ TESTQ TESTQ TESTQ

unsigned char testa_md5[] = {0x90, 0x01, 0x50, 0x98, 0x3c, 0xd2, 0x4f, 0xb0,
                             0xd6, 0x96, 0x3f, 0x7d, 0x28, 0xe1, 0x7f, 0x72};
unsigned char testb_md5[] = {0x82, 0x15, 0xef, 0x07, 0x96, 0xa2, 0x0b, 0xca,
                             0xaa, 0xe1, 0x16, 0xd3, 0x87, 0x6c, 0x66, 0x4a};
unsigned char testc_md5[] = {0x77, 0x07, 0xd6, 0xae, 0x4e, 0x02, 0x7c, 0x70,
                             0xee, 0xa2, 0xa9, 0x35, 0xc2, 0x29, 0x6f, 0x21};
unsigned char testq_md5[] = {0x9e, 0x10, 0x7d, 0x9d, 0x37, 0x2b, 0xb6, 0x82,
                             0x6b, 0xd8, 0x1d, 0x35, 0x42, 0xa4, 0x19, 0xd6};
unsigned char testl_md5[] = {0x5b, 0x61, 0x37, 0x72, 0xd2, 0xe9, 0x07, 0x7d,
                             0x2d, 0x40, 0x9b, 0xb6, 0x62, 0x9f, 0xdb, 0x09};

static void
is_md5(void * expected, void * got, const char * str)
{
    if (memcmp(expected, got, sizeof(SOPHOS_MD5)) == 0) {
        pass("%s", str);
        return;
    }

    diag("Expected MD5:");
    SXED10(expected, sizeof(SOPHOS_MD5));
    diag("Got      MD5:");
    SXED10(got,      sizeof(SOPHOS_MD5));
    fail("%s", str);
}

int
main(void)
{
    MD5_CTX     md5;
    uint32_t    Message_Digest[4];
    char        Message_Digest_Hex[MD5_IN_HEX_LENGTH + 1];
    int         i;

    plan_tests(11);

    /*
     *  Perform test A
     */
    MD5Reset(&md5);
    MD5Input(&md5, (const unsigned char *)TESTA, strlen(TESTA));
    ok(MD5Result(&md5, Message_Digest), "Computed message digest for 'abc'");
    is_md5(testa_md5, Message_Digest,  "Test A: MD5 as expected");
    md5_to_hex((SOPHOS_MD5 *)&Message_Digest, Message_Digest_Hex, sizeof Message_Digest_Hex);
    is_eq(Message_Digest_Hex, "900150983cd24fb0d6963f7d28e17f72", "Hex version of digest is correct");

    /*
     *  Perform test B
     */
    MD5Reset(&md5);
    MD5Input(&md5, (const unsigned char *) TESTB, strlen(TESTB));
    ok(MD5Result(&md5, Message_Digest), "Computed message digest for '%s'", TESTB);
    is_md5(testb_md5, Message_Digest,  "Test B: MD5 as expected");

    /*
     *  Perform test C
     */
    MD5Reset(&md5);

    for(i = 1; i <= 1000000; i++) {
        MD5Input(&md5, (const unsigned char *) TESTC, 1);
    }

    ok(MD5Result(&md5, Message_Digest), "Computed message digest for one million 'a's");
    is_md5(testc_md5, Message_Digest,  "Test C: MD5 as expected");

    /*
     *  Perform quick brown fox test
     */
    MD5Reset(&md5);
    MD5Input(&md5, (const unsigned char *) TESTQ, strlen(TESTQ));
    ok(MD5Result(&md5, Message_Digest), "Computed message digest for '%s'", TESTQ);
    is_md5(testq_md5, Message_Digest,  "Quick brown fox test: MD5 as expected");

    /*
     * Perform long string test (>64)
     */
    MD5Reset(&md5);
    MD5Input(&md5, (const unsigned char *) TESTL, strlen(TESTL));
    ok(MD5Result(&md5, Message_Digest), "Computed message digest for '%s'", TESTL);
    is_md5(testl_md5, Message_Digest,  "Long test: MD5 as expected");

    return exit_status();
}
