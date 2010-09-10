/*
 *  sha1.c
 *
 *  Copyright (C) 1998
 *  Paul E. Jones <paulej@arid.us>
 *  All Rights Reserved
 *
 *****************************************************************************
 *  $Id: sha1.c,v 1.2 2004/03/27 18:00:33 paulej Exp $
 *****************************************************************************
 *
 *  Description:
 *      This file implements the Secure Hashing Standard as defined
 *      in FIPS PUB 180-1 published April 17, 1995.
 *
 *      The Secure Hashing Standard, which uses the Secure Hashing
 *      Algorithm (SHA), produces a 160-bit message digest for a
 *      given data stream.  In theory, it is highly improbable that
 *      two messages will produce the same message digest.  Therefore,
 *      this algorithm can serve as a means of providing a "fingerprint"
 *      for a message.
 *
 *  Portability Issues:
 *      SHA-1 is defined in terms of 32-bit "words".  This code was
 *      written with the expectation that the processor has at least
 *      a 32-bit machine word size.  If the machine word size is larger,
 *      the code should still function properly.  One caveat to that
 *      is that the input functions taking characters and character
 *      arrays assume that only 8 bits of information are stored in each
 *      character.
 *
 *  Caveats:
 *      SHA-1 is designed to work with messages less than 2^64 bits
 *      long. Although SHA-1 allows a message digest to be generated for
 *      messages of any number of bits less than 2^64, this
 *      implementation only works with messages with a length that is a
 *      multiple of the size of an 8-bit character.
 *
 */
#include <string.h>
#include "sha1.h"

/*
 *  Define the circular shift macro
 */
#define SHA1CircularShift(bits,word) \
                ((((word) << (bits)) & 0xFFFFFFFF) | \
                ((word) >> (32-(bits))))

/* Function prototypes */
static void SHA1ProcessMessageBlock(sophos_sha1_ctx_t *);
static void SHA1PadMessage(sophos_sha1_ctx_t *);

/* function test configuration */
static int attempts_sha1_init = 0;
static int attempts_sha1_update = 0;
static int attempts_sha1_final = 0;
static int attempts_sha1 = 0;
static int fail_next_call = 0;

/*
 *  SHA1Init
 *
 *  Description:
 *      This function will initialize the sophos_sha1_ctx_t in preparation
 *      for computing a new message digest.
 *
 *  Parameters:
 *      context: [in/out]
 *          The context to reset.
 *
 *  Returns:
 *      0 = success.
 *     <0 = fail.
 *
 *  Comments:
 *
 */
int sophos_sha1_init(sophos_sha1_ctx_t * context)
{
  static int attempts = 0;

  /* return -1 if configured to fail on this attempt */
  if(attempts_sha1_init>0)
  {
    if(++attempts >= attempts_sha1_init)
    {
      attempts=0;
      return -1;
    }
  }
  else
    attempts=0;

  context->Length_Low             = 0;
  context->Length_High            = 0;
  context->Message_Block_Index    = 0;

  context->Message_Digest[0]      = 0x67452301;
  context->Message_Digest[1]      = 0xEFCDAB89;
  context->Message_Digest[2]      = 0x98BADCFE;
  context->Message_Digest[3]      = 0x10325476;
  context->Message_Digest[4]      = 0xC3D2E1F0;

  context->Computed   = 0;
  context->Corrupted  = 0;

  return 0;
}

/**
 * Finish the SHA1 hash and return the digest.
 *
 * @param data Pointer to SHA1 data structure.
 * @param digest Buffer to recieve the digest.
 *  Returns:
 *      0 = success.
 *     <0 = fail.
 */
int sophos_sha1_final(sophos_sha1_ctx_t * context, char * digest)
{
  static int attempts = 0;
  int i,j,x;

  /* return -1 if configured to fail on this attempt */
  if(attempts_sha1_final>0)
  {
    if(++attempts >= attempts_sha1_final)
    {
      attempts=0;
      return -1;
    }
  }
  else
    attempts=0;

  if (context->Corrupted)
  {
      return -1;
  }

  if (!context->Computed)
  {
      SHA1PadMessage(context);
      context->Computed = 1;
  }

  /* need to test endianness of machine to get uint[5] to uchar[20] copy correctly */
  x = 1;
  if(*(char *)&x == 1)
  { /* little endian */
    for(i=0;i<5;i++)
      for(j=0;j<4;j++)
        digest[i*4+j] = (context->Message_Digest[i] >> (3-j)*8) & 0xff;
  }
  else /* big endian */
    memcpy(digest, context->Message_Digest, 20);

    return 0;
}

/*
 *  SHA1Update
 *
 *  Description:
 *      This function accepts an array of octets as the next portion of
 *      the message.
 *
 *  Parameters:
 *      context: [in/out]
 *          The SHA-1 context to update
 *      message_array: [in]
 *          An array of characters representing the next portion of the
 *          message.
 *      length: [in]
 *          The length of the message in message_array
 *
 *  Returns:
 *      0 = success.
 *     <0 = fail.
 *
 *  Comments:
 *
 */
int sophos_sha1_update(sophos_sha1_ctx_t * context, const char * message_array, size_t length)
{
  static int attempts = 0;

  /* return -1 if configured to fail on this attempt */
  if(attempts_sha1_update>0)
  {
    if(++attempts >= attempts_sha1_update)
    {
      attempts=0;
      return -1;
    }
  }
  else
    attempts=0;

  if (!length)
    {
        return -1;
    }

    if (context->Computed || context->Corrupted)
    {
        context->Corrupted = 1;
        return -2;
    }

    while(length-- && !context->Corrupted)
    {
        context->Message_Block[context->Message_Block_Index++] =
                                                (*message_array & 0xFF);

        context->Length_Low += 8;
        /* Force it to 32 bits */
        context->Length_Low &= 0xFFFFFFFF;
        if (context->Length_Low == 0)
        {
            context->Length_High++;
            /* Force it to 32 bits */
            context->Length_High &= 0xFFFFFFFF;
            if (context->Length_High == 0)
            {
                /* Message is too long */
                context->Corrupted = 1;
            }
        }

        if (context->Message_Block_Index == 64)
        {
            SHA1ProcessMessageBlock(context);
        }

        message_array++;
    }
    return 0;
}


/*
 *  sophos_sha1 function.
 *
 *  Description:
 *      This function generates the SHA1 hash for a given message in one shot,
 *      and stores it in the given buffer.
 *
 *  Parameters:
 *      msg: [in]
 *          An array of characters representing the message.
 *      length: [in]
 *          The length of the message in message_array
 *      md: [out]
 *          The calculated message digest (checksum).
 *
 *  Returns:
 *      The message digest pointer, or NULL on failure.
 *
 */
char *sophos_sha1(const char *msg, size_t length, char *md)
{
  static int attempts = 0;
  sophos_sha1_ctx_t  context;

  /* return NULL if configured to fail on this attempt */
  if(fail_next_call > 0)
  {
      fail_next_call = 0;
      attempts=0;
      *md='\0';
      return NULL;
  }
  if(attempts_sha1>0)
  {
    if(++attempts >= attempts_sha1)
    {
      attempts=0;
      *md='\0';
      return NULL;
    }
  }
  else
    attempts=0;

  if (!sophos_sha1_init(&context))
    if (!sophos_sha1_update(&context,msg,(unsigned)length))
      if (!sophos_sha1_final(&context,md))
        return md;
  *md='\0';
  return NULL;
}


/*
 *  SHA1ProcessMessageBlock
 *
 *  Description:
 *      This function will process the next 512 bits of the message
 *      stored in the Message_Block array.
 *
 *  Parameters:
 *      context: [in/out]
 *          The context to process
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *      Many of the variable names in the SHAContext, especially the
 *      single character names, were used because those were the names
 *      used in the publication.
 *
 *
 */
static void SHA1ProcessMessageBlock(sophos_sha1_ctx_t *context)
{
    const unsigned K[] =            /* Constants defined in SHA-1   */
    {
        0x5A827999,
        0x6ED9EBA1,
        0x8F1BBCDC,
        0xCA62C1D6
    };
    int         t;                  /* Loop counter                 */
    unsigned    temp;               /* Temporary word value         */
    unsigned    W[80];              /* Word sequence                */
    unsigned    A, B, C, D, E;      /* Word buffers                 */

    /*
     *  Initialize the first 16 words in the array W
     */
    for(t = 0; t < 16; t++)
    {
        W[t] = ((unsigned) context->Message_Block[t * 4]) << 24;
        W[t] |= ((unsigned) context->Message_Block[t * 4 + 1]) << 16;
        W[t] |= ((unsigned) context->Message_Block[t * 4 + 2]) << 8;
        W[t] |= ((unsigned) context->Message_Block[t * 4 + 3]);
    }

    for(t = 16; t < 80; t++)
    {
       W[t] = SHA1CircularShift(1,W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16]);
    }

    A = context->Message_Digest[0];
    B = context->Message_Digest[1];
    C = context->Message_Digest[2];
    D = context->Message_Digest[3];
    E = context->Message_Digest[4];

    for(t = 0; t < 20; t++)
    {
        temp =  SHA1CircularShift(5,A) +
                ((B & C) | ((~B) & D)) + E + W[t] + K[0];
        temp &= 0xFFFFFFFF;
        E = D;
        D = C;
        C = SHA1CircularShift(30,B);
        B = A;
        A = temp;
    }

    for(t = 20; t < 40; t++)
    {
        temp = SHA1CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[1];
        temp &= 0xFFFFFFFF;
        E = D;
        D = C;
        C = SHA1CircularShift(30,B);
        B = A;
        A = temp;
    }

    for(t = 40; t < 60; t++)
    {
        temp = SHA1CircularShift(5,A) +
               ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2];
        temp &= 0xFFFFFFFF;
        E = D;
        D = C;
        C = SHA1CircularShift(30,B);
        B = A;
        A = temp;
    }

    for(t = 60; t < 80; t++)
    {
        temp = SHA1CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[3];
        temp &= 0xFFFFFFFF;
        E = D;
        D = C;
        C = SHA1CircularShift(30,B);
        B = A;
        A = temp;
    }

    context->Message_Digest[0] =
                        (context->Message_Digest[0] + A) & 0xFFFFFFFF;
    context->Message_Digest[1] =
                        (context->Message_Digest[1] + B) & 0xFFFFFFFF;
    context->Message_Digest[2] =
                        (context->Message_Digest[2] + C) & 0xFFFFFFFF;
    context->Message_Digest[3] =
                        (context->Message_Digest[3] + D) & 0xFFFFFFFF;
    context->Message_Digest[4] =
                        (context->Message_Digest[4] + E) & 0xFFFFFFFF;

    context->Message_Block_Index = 0;
}

/*
 *  SHA1PadMessage
 *
 *  Description:
 *      According to the standard, the message must be padded to an even
 *      512 bits.  The first padding bit must be a '1'.  The last 64
 *      bits represent the length of the original message.  All bits in
 *      between should be 0.  This function will pad the message
 *      according to those rules by filling the Message_Block array
 *      accordingly.  It will also call SHA1ProcessMessageBlock()
 *      appropriately.  When it returns, it can be assumed that the
 *      message digest has been computed.
 *
 *  Parameters:
 *      context: [in/out]
 *          The context to pad
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *
 */
static void SHA1PadMessage(sophos_sha1_ctx_t *context)
{
    /*
     *  Check to see if the current message block is too small to hold
     *  the initial padding bits and length.  If so, we will pad the
     *  block, process it, and then continue padding into a second
     *  block.
     */
    if (context->Message_Block_Index > 55)
    {
        context->Message_Block[context->Message_Block_Index++] = 0x80;
        while(context->Message_Block_Index < 64)
        {
            context->Message_Block[context->Message_Block_Index++] = 0;
        }

        SHA1ProcessMessageBlock(context);

        while(context->Message_Block_Index < 56)
        {
            context->Message_Block[context->Message_Block_Index++] = 0;
        }
    }
    else
    {
        context->Message_Block[context->Message_Block_Index++] = 0x80;
        while(context->Message_Block_Index < 56)
        {
            context->Message_Block[context->Message_Block_Index++] = 0;
        }
    }

    /*
     *  Store the message length as the last 8 octets
     */
    context->Message_Block[56] = (context->Length_High >> 24) & 0xFF;
    context->Message_Block[57] = (context->Length_High >> 16) & 0xFF;
    context->Message_Block[58] = (context->Length_High >> 8) & 0xFF;
    context->Message_Block[59] = (context->Length_High) & 0xFF;
    context->Message_Block[60] = (context->Length_Low >> 24) & 0xFF;
    context->Message_Block[61] = (context->Length_Low >> 16) & 0xFF;
    context->Message_Block[62] = (context->Length_Low >> 8) & 0xFF;
    context->Message_Block[63] = (context->Length_Low) & 0xFF;

    SHA1ProcessMessageBlock(context);
}


/**  TEST FUNCTIONS
 *   The following fucntions are used to artificaially cause the public sha1
 *   functions to fail in order to assist with unit testing. You
 *   can configure each function to fail every x number of calls (attempts)
 *   (set attempts to 0 to never fail, 1 to always fail, or x to fail every xth call).
 */

/**  sophos_sha1_init_config
 *   Configures sophos_sha1_init to fail (return NULL) every xth call, where x=attempts.
 */
int sophos_sha1_init_config(int attempts)
{
  attempts_sha1_init = attempts;
  return 0;
}

/**  sophos_sha1_update_config
 *   Configures sophos_sha1_update to fail (return NULL) every xth call, where x=attempts.
 */
int sophos_sha1_update_config(int attempts)
{
  attempts_sha1_update = attempts;
  return 0;
}

/**  sophos_sha1_final_config
 *   Configures sophos_sha1_final to fail (return NULL) every xth call, where x=attempts.
 */
int sophos_sha1_final_config(int attempts)
{
  attempts_sha1_final = attempts;
  return 0;
}

/**  sophos_sha1_config
 *   Configures sophos_sha1 to fail (return NULL) every xth call, where x=attempts.
 */
int sophos_sha1_config(int attempts)
{
  attempts_sha1 = attempts;
  return 0;
}

void sophos_sha1_fail_next_call(void)
{
    fail_next_call = 1;
}
