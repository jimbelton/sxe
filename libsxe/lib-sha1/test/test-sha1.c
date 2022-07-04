/*
 *  shatest.c
 *
 *  Copyright (C) 1998, 2009
 *  Paul E. Jones <paulej@packetizer.com>
 *  All Rights Reserved
 *
 *****************************************************************************
 *  $Id: shatest.c 12 2009-06-22 19:34:25Z paulej $
 *****************************************************************************
 *
 *  Description:
 *      This file will exercise the SHA1 class and perform the three
 *      tests documented in FIPS PUB 180-1.
 *
 *  Portability Issues:
 *      None.
 *
 */

#include <stdio.h>
#include <string.h>
#include "sha1.h"
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

unsigned char testa_sha1[] = {0xA9, 0x99, 0x3E, 0x36, 0x47, 0x06, 0x81, 0x6A, 0xBA, 0x3E,
                              0x25, 0x71, 0x78, 0x50, 0xC2, 0x6C, 0x9C, 0xD0, 0xD8, 0x9D};
unsigned char testb_sha1[] = {0x84, 0x98, 0x3E, 0x44, 0x1C, 0x3B, 0xD2, 0x6E, 0xBA, 0xAE,
                              0x4A, 0xA1, 0xF9, 0x51, 0x29, 0xE5, 0xE5, 0x46, 0x70, 0xF1};
unsigned char testc_sha1[] = {0x34, 0xAA, 0x97, 0x3C, 0xD4, 0xC4, 0xDA, 0xA4, 0xF6, 0x1E,
                              0xEB, 0x2B, 0xDB, 0xAD, 0x27, 0x31, 0x65, 0x34, 0x01, 0x6F};
unsigned char testq_sha1[] = {0x2f, 0xd4, 0xe1, 0xc6, 0x7a, 0x2d, 0x28, 0xfc, 0xed, 0x84,
                              0x9e, 0xe1, 0xbb, 0x76, 0xe7, 0x39, 0x1b, 0x93, 0xeb, 0x12};

static void
is_sha1(void * expected, void * got, const char * str)
{
    if (memcmp(expected, got, sizeof(SOPHOS_SHA1)) == 0) {
        pass("%s", str);
        return;
    }

    diag("Expected SHA1:");
    SXED1(expected, sizeof(SOPHOS_SHA1));
    diag("Got      SHA1:");
    SXED1(got,      sizeof(SOPHOS_SHA1));
    fail("%s", str);
}

int
main(void)
{
    SHA1Context sha;
    uint32_t    Message_Digest[5];
    int         i;

    plan_tests(8);

    /*
     *  Perform test A
     */
    SHA1Reset(&sha);
    SHA1Input(&sha, (const unsigned char *)TESTA, strlen(TESTA));
    ok(SHA1Result(&sha, Message_Digest), "Computed message digest for 'abc'");
    is_sha1(testa_sha1, Message_Digest,  "Test A: SHA1 as expected");

    /*
     *  Perform test B
     */
    SHA1Reset(&sha);
    SHA1Input(&sha, (const unsigned char *) TESTB, strlen(TESTB));
    ok(SHA1Result(&sha, Message_Digest), "Computed message digest for '%s'", TESTB);
    is_sha1(testb_sha1, Message_Digest,  "Test B: SHA1 as expected");

    /*
     *  Perform test C
     */
    SHA1Reset(&sha);

    for(i = 1; i <= 1000000; i++) {
        SHA1Input(&sha, (const unsigned char *) TESTC, 1);
    }

    ok(SHA1Result(&sha, Message_Digest), "Computed message digest for one million 'a's");
    is_sha1(testc_sha1, Message_Digest,  "Test C: SHA1 as expected");

    /*
     *  Perform quick brown fox test
     */
    SHA1Reset(&sha);
    SHA1Input(&sha, (const unsigned char *) TESTQ, strlen(TESTQ));
    ok(SHA1Result(&sha, Message_Digest), "Computed message digest for '%s'", TESTQ);
    is_sha1(testq_sha1, Message_Digest,  "Quick brown fox test: SHA1 as expected");

    return exit_status();
}
