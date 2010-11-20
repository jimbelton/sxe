/*
-------------------------------------------------------------------------------
lookup3.c, by Bob Jenkins, May 2006, Public Domain.

These are functions for producing 32-bit hashes for hash table lookup.
hashword(), hashlittle(), hashlittle2(), hashbig(), mix(), and final()
are externally useful functions.  Routines to test the hash are included
if SELF_TEST is defined.  You can use this free for any purpose.  It's in
the public domain.  It has no warranty.

You probably want to use hashlittle().  hashlittle() and hashbig()
hash byte arrays.  hashlittle() is faster than hashbig() on
little-endian machines.  Intel and AMD are little-endian machines.
On second thought, you probably want hashlittle2(), which is identical to
hashlittle() except it returns two 32-bit hashes for the price of one.
You could implement hashbig2() if you wanted but I haven't bothered here.

If you want to find a hash of, say, exactly 7 integers, do
  a = i1;  b = i2;  c = i3;
  mix(a,b,c);
  a += i4; b += i5; c += i6;
  mix(a,b,c);
  a += i7;
  final(a,b,c);
then use c as the hash value.  If you have a variable length array of
4-byte integers to hash, use hashword().  If you have a byte array (like
a character string), use hashlittle().  If you have several byte arrays, or
a mix of things, see the comments above hashlittle().

Why is this so big?  I read 12 bytes at a time into 3 4-byte integers,
then mix those integers.  This is fast (you can do a lot more thorough
mixing with 12*3 instructions on 3 integers than you can with 3 instructions
on 1 byte), but shoehorning those bytes into integers efficiently is messy.
-------------------------------------------------------------------------------
*/

/* Sophos change: Tests moved from lookup3.c */

#include <stdio.h>      /* defines printf for tests */
#include <time.h>       /* defines time_t for timings in the test */
#include <stdint.h>     /* defines uint32_t etc */

#ifdef _WIN32    /* Sophos extension: define endianness for X86 and X64 versions of Windows */
    #if defined(_M_IX86) || defined(_M_X64)
        #define __LITTLE_ENDIAN 1
        #define __BYTE_ORDER    __LITTLE_ENDIAN
    #endif
#else
    #include <sys/param.h>  /* attempt to define endianness */
    #ifdef linux
        #include <endian.h>    /* attempt to define endianness */
    #endif
#endif

#include "lookup3.h"

/* TODO: Turn into a tap based test.
 */
#ifdef FUTURE

/*
 * My best guess at if you are big-endian or little-endian.  This may
 * need adjustment.
 */
#if (defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN) && \
     __BYTE_ORDER == __LITTLE_ENDIAN) || \
    (defined(i386) || defined(__i386__) || defined(__i486__) || \
     defined(__i586__) || defined(__i686__) || defined(vax) || defined(MIPSEL))
# define HASH_LITTLE_ENDIAN 1
# define HASH_BIG_ENDIAN 0
#elif (defined(__BYTE_ORDER) && defined(__BIG_ENDIAN) && \
       __BYTE_ORDER == __BIG_ENDIAN) || \
      (defined(sparc) || defined(POWERPC) || defined(mc68000) || defined(sel))
# define HASH_LITTLE_ENDIAN 0
# define HASH_BIG_ENDIAN 1
#else
# define HASH_LITTLE_ENDIAN 0
# define HASH_BIG_ENDIAN 0
#endif

#define hashsize(n) ((uint32_t)1<<(n))
#define hashmask(n) (hashsize(n)-1)
#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))

/*
-------------------------------------------------------------------------------
mix -- mix 3 32-bit values reversibly.

This is reversible, so any information in (a,b,c) before mix() is
still in (a,b,c) after mix().

If four pairs of (a,b,c) inputs are run through mix(), or through
mix() in reverse, there are at least 32 bits of the output that
are sometimes the same for one pair and different for another pair.
This was tested for:
* pairs that differed by one bit, by two bits, in any combination
  of top bits of (a,b,c), or in any combination of bottom bits of
  (a,b,c).
* "differ" is defined as +, -, ^, or ~^.  For + and -, I transformed
  the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
  is commonly produced by subtraction) look like a single 1-bit
  difference.
* the base values were pseudorandom, all zero but one bit set, or
  all zero plus a counter that starts at zero.

Some k values for my "a-=c; a^=rot(c,k); c+=b;" arrangement that
satisfy this are
    4  6  8 16 19  4
    9 15  3 18 27 15
   14  9  3  7 17  3
Well, "9 15 3 18 27 15" didn't quite get 32 bits diffing
for "differ" defined as + with a one-bit base and a two-bit delta.  I
used http://burtleburtle.net/bob/hash/avalanche.html to choose
the operations, constants, and arrangements of the variables.

This does not achieve avalanche.  There are input bits of (a,b,c)
that fail to affect some output bits of (a,b,c), especially of a.  The
most thoroughly mixed value is c, but it doesn't really even achieve
avalanche in c.

This allows some parallelism.  Read-after-writes are good at doubling
the number of bits affected, so the goal of mixing pulls in the opposite
direction as the goal of parallelism.  I did what I could.  Rotates
seem to cost as much as shifts on every machine I could lay my hands
on, and rotates are much kinder to the top and bottom bits, so I used
rotates.
-------------------------------------------------------------------------------
*/
#define mix(a,b,c) \
{ \
  a -= c;  a ^= rot(c, 4);  c += b; \
  b -= a;  b ^= rot(a, 6);  a += c; \
  c -= b;  c ^= rot(b, 8);  b += a; \
  a -= c;  a ^= rot(c,16);  c += b; \
  b -= a;  b ^= rot(a,19);  a += c; \
  c -= b;  c ^= rot(b, 4);  b += a; \
}

/*
-------------------------------------------------------------------------------
final -- final mixing of 3 32-bit values (a,b,c) into c

Pairs of (a,b,c) values differing in only a few bits will usually
produce values of c that look totally different.  This was tested for
* pairs that differed by one bit, by two bits, in any combination
  of top bits of (a,b,c), or in any combination of bottom bits of
  (a,b,c).
* "differ" is defined as +, -, ^, or ~^.  For + and -, I transformed
  the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
  is commonly produced by subtraction) look like a single 1-bit
  difference.
* the base values were pseudorandom, all zero but one bit set, or
  all zero plus a counter that starts at zero.

These constants passed:
 14 11 25 16 4 14 24
 12 14 25 16 4 14 24
and these came close:
  4  8 15 26 3 22 24
 10  8 15 26 3 22 24
 11  8 15 26 3 22 24
-------------------------------------------------------------------------------
*/
#define final(a,b,c) \
{ \
  c ^= b; c -= rot(b,14); \
  a ^= c; a -= rot(c,11); \
  b ^= a; b -= rot(a,25); \
  c ^= b; c -= rot(b,16); \
  a ^= c; a -= rot(c,4);  \
  b ^= a; b -= rot(a,14); \
  c ^= b; c -= rot(b,24); \
}

/* used for timings */
static void driver1(void)
{
  uint8_t buf[256];
  uint32_t i;
  uint32_t h=0;
  time_t a,z;

  time(&a);
  for (i=0; i<256; ++i) buf[i] = 'x';
  for (i=0; i<1; ++i)
  {
    h = hashlittle(&buf[0],1,h);
  }
  time(&z);
  if (z-a > 0) printf("time %d %.8x\n", z-a, h);
}

/* check that every input bit changes every output bit half the time */
#define HASHSTATE 1
#define HASHLEN   1
#define MAXPAIR 60
#define MAXLEN  70
static void driver2(void)
{
  uint8_t qa[MAXLEN+1], qb[MAXLEN+2], *a = &qa[0], *b = &qb[1];
  uint32_t c[HASHSTATE], d[HASHSTATE], i=0, j=0, k, l, m=0, z;
  uint32_t e[HASHSTATE],f[HASHSTATE],g[HASHSTATE],h[HASHSTATE];
  uint32_t x[HASHSTATE],y[HASHSTATE];
  uint32_t hlen;

  printf("No more than %d trials should ever be needed \n",MAXPAIR/2);
  for (hlen=0; hlen < MAXLEN; ++hlen)
  {
    z=0;
    for (i=0; i<hlen; ++i)  /*----------------------- for each input byte, */
    {
      for (j=0; j<8; ++j)   /*------------------------ for each input bit, */
      {
	for (m=1; m<8; ++m) /*------------ for serveral possible initvals, */
	{
	  for (l=0; l<HASHSTATE; ++l)
	    e[l]=f[l]=g[l]=h[l]=x[l]=y[l]=~((uint32_t)0);

      	  /*---- check that every output bit is affected by that input bit */
	  for (k=0; k<MAXPAIR; k+=2)
	  {
	    uint32_t finished=1;
	    /* keys have one bit different */
	    for (l=0; l<hlen+1; ++l) {a[l] = b[l] = (uint8_t)0;}
	    /* have a and b be two keys differing in only one bit */
	    a[i] ^= (k<<j);
	    a[i] ^= (k>>(8-j));
	     c[0] = hashlittle(a, hlen, m);
	    b[i] ^= ((k+1)<<j);
	    b[i] ^= ((k+1)>>(8-j));
	     d[0] = hashlittle(b, hlen, m);
	    /* check every bit is 1, 0, set, and not set at least once */
	    for (l=0; l<HASHSTATE; ++l)
	    {
	      e[l] &= (c[l]^d[l]);
	      f[l] &= ~(c[l]^d[l]);
	      g[l] &= c[l];
	      h[l] &= ~c[l];
	      x[l] &= d[l];
	      y[l] &= ~d[l];
	      if (e[l]|f[l]|g[l]|h[l]|x[l]|y[l]) finished=0;
	    }
	    if (finished) break;
	  }
	  if (k>z) z=k;
	  if (k==MAXPAIR)
	  {
	     printf("Some bit didn't change: ");
	     printf("%.8x %.8x %.8x %.8x %.8x %.8x  ",
	            e[0],f[0],g[0],h[0],x[0],y[0]);
	     printf("i %d j %d m %d len %d\n", i, j, m, hlen);
	  }
	  if (z==MAXPAIR) goto done;
	}
      }
    }
   done:
    if (z < MAXPAIR)
    {
      printf("Mix success  %2d bytes  %2d initvals  ",i,m);
      printf("required  %d  trials\n", z/2);
    }
  }
  printf("\n");
}

/* Check for reading beyond the end of the buffer and alignment problems */
static void driver3(void)
{
  uint8_t buf[MAXLEN+20], *b;
  uint32_t len;
  uint8_t q[] = "This is the time for all good men to come to the aid of their country...";
  uint32_t h;
  uint8_t qq[] = "xThis is the time for all good men to come to the aid of their country...";
  uint32_t i;
  uint8_t qqq[] = "xxThis is the time for all good men to come to the aid of their country...";
  uint32_t j;
  uint8_t qqqq[] = "xxxThis is the time for all good men to come to the aid of their country...";
  uint32_t ref,x,y;
  uint8_t *p;

  printf("Endianness.  These lines should all be the same (for values filled in):\n");
  printf("%.8x                            %.8x                            %.8x\n",
         hashword((const uint32_t *)q, (sizeof(q)-1)/4, 13),
         hashword((const uint32_t *)q, (sizeof(q)-5)/4, 13),
         hashword((const uint32_t *)q, (sizeof(q)-9)/4, 13));
  p = q;
  printf("%.8x %.8x %.8x %.8x %.8x %.8x %.8x %.8x %.8x %.8x %.8x %.8x\n",
         hashlittle(p, sizeof(q)-1, 13), hashlittle(p, sizeof(q)-2, 13),
         hashlittle(p, sizeof(q)-3, 13), hashlittle(p, sizeof(q)-4, 13),
         hashlittle(p, sizeof(q)-5, 13), hashlittle(p, sizeof(q)-6, 13),
         hashlittle(p, sizeof(q)-7, 13), hashlittle(p, sizeof(q)-8, 13),
         hashlittle(p, sizeof(q)-9, 13), hashlittle(p, sizeof(q)-10, 13),
         hashlittle(p, sizeof(q)-11, 13), hashlittle(p, sizeof(q)-12, 13));
  p = &qq[1];
  printf("%.8x %.8x %.8x %.8x %.8x %.8x %.8x %.8x %.8x %.8x %.8x %.8x\n",
         hashlittle(p, sizeof(q)-1, 13), hashlittle(p, sizeof(q)-2, 13),
         hashlittle(p, sizeof(q)-3, 13), hashlittle(p, sizeof(q)-4, 13),
         hashlittle(p, sizeof(q)-5, 13), hashlittle(p, sizeof(q)-6, 13),
         hashlittle(p, sizeof(q)-7, 13), hashlittle(p, sizeof(q)-8, 13),
         hashlittle(p, sizeof(q)-9, 13), hashlittle(p, sizeof(q)-10, 13),
         hashlittle(p, sizeof(q)-11, 13), hashlittle(p, sizeof(q)-12, 13));
  p = &qqq[2];
  printf("%.8x %.8x %.8x %.8x %.8x %.8x %.8x %.8x %.8x %.8x %.8x %.8x\n",
         hashlittle(p, sizeof(q)-1, 13), hashlittle(p, sizeof(q)-2, 13),
         hashlittle(p, sizeof(q)-3, 13), hashlittle(p, sizeof(q)-4, 13),
         hashlittle(p, sizeof(q)-5, 13), hashlittle(p, sizeof(q)-6, 13),
         hashlittle(p, sizeof(q)-7, 13), hashlittle(p, sizeof(q)-8, 13),
         hashlittle(p, sizeof(q)-9, 13), hashlittle(p, sizeof(q)-10, 13),
         hashlittle(p, sizeof(q)-11, 13), hashlittle(p, sizeof(q)-12, 13));
  p = &qqqq[3];
  printf("%.8x %.8x %.8x %.8x %.8x %.8x %.8x %.8x %.8x %.8x %.8x %.8x\n",
         hashlittle(p, sizeof(q)-1, 13), hashlittle(p, sizeof(q)-2, 13),
         hashlittle(p, sizeof(q)-3, 13), hashlittle(p, sizeof(q)-4, 13),
         hashlittle(p, sizeof(q)-5, 13), hashlittle(p, sizeof(q)-6, 13),
         hashlittle(p, sizeof(q)-7, 13), hashlittle(p, sizeof(q)-8, 13),
         hashlittle(p, sizeof(q)-9, 13), hashlittle(p, sizeof(q)-10, 13),
         hashlittle(p, sizeof(q)-11, 13), hashlittle(p, sizeof(q)-12, 13));
  printf("\n");

  /* check that hashlittle2 and hashlittle produce the same results */
  i=47; j=0;
  hashlittle2(q, sizeof(q), &i, &j);
  if (hashlittle(q, sizeof(q), 47) != i)
    printf("hashlittle2 and hashlittle mismatch\n");

  /* check that hashword2 and hashword produce the same results */
  len = 0xdeadbeef;
  i=47, j=0;
  hashword2(&len, 1, &i, &j);
  if (hashword(&len, 1, 47) != i)
    printf("hashword2 and hashword mismatch %x %x\n",
	   i, hashword(&len, 1, 47));

  /* check hashlittle doesn't read before or after the ends of the string */
  for (h=0, b=buf+1; h<8; ++h, ++b)
  {
    for (i=0; i<MAXLEN; ++i)
    {
      len = i;
      for (j=0; j<i; ++j) *(b+j)=0;

      /* these should all be equal */
      ref = hashlittle(b, len, (uint32_t)1);
      *(b+i)=(uint8_t)~0;
      *(b-1)=(uint8_t)~0;
      x = hashlittle(b, len, (uint32_t)1);
      y = hashlittle(b, len, (uint32_t)1);
      if ((ref != x) || (ref != y))
      {
	printf("alignment error: %.8x %.8x %.8x %d %d\n",ref,x,y,
               h, i);
      }
    }
  }
}

/* check for problems with nulls */
static void driver4(void)
{
  uint8_t buf[1];
  uint32_t h,i,state[HASHSTATE];


  buf[0] = ~0;
  for (i=0; i<HASHSTATE; ++i) state[i] = 1;
  printf("These should all be different\n");
  for (i=0, h=0; i<8; ++i)
  {
    h = hashlittle(buf, 0, h);
    printf("%2ld  0-byte strings, hash is  %.8x\n", i, h);
  }
}

static void driver5(void)
{
  uint32_t b,c;
  b=0, c=0, hashlittle2("", 0, &c, &b);
  printf("hash is %.8lx %.8lx\n", c, b);   /* deadbeef deadbeef */
  b=0xdeadbeef, c=0, hashlittle2("", 0, &c, &b);
  printf("hash is %.8lx %.8lx\n", c, b);   /* bd5b7dde deadbeef */
  b=0xdeadbeef, c=0xdeadbeef, hashlittle2("", 0, &c, &b);
  printf("hash is %.8lx %.8lx\n", c, b);   /* 9c093ccd bd5b7dde */
  b=0, c=0, hashlittle2("Four score and seven years ago", 30, &c, &b);
  printf("hash is %.8lx %.8lx\n", c, b);   /* 17770551 ce7226e6 */
  b=1, c=0, hashlittle2("Four score and seven years ago", 30, &c, &b);
  printf("hash is %.8lx %.8lx\n", c, b);   /* e3607cae bd371de4 */
  b=0, c=1, hashlittle2("Four score and seven years ago", 30, &c, &b);
  printf("hash is %.8lx %.8lx\n", c, b);   /* cd628161 6cbea4b3 */
  c = hashlittle("Four score and seven years ago", 30, 0);
  printf("hash is %.8lx\n", c);   /* 17770551 */
  c = hashlittle("Four score and seven years ago", 30, 1);
  printf("hash is %.8lx\n", c);   /* cd628161 */
}

#endif

int main(void)
{
#ifdef FUTURE
  driver1();   /* test that the key is hashed: used for timings */
  driver2();   /* test that whole key is hashed thoroughly */
  driver3();   /* test that nothing but the key is hashed */
  driver4();   /* test hashing multiple buffers (all buffers are null) */
  driver5();   /* test the hash against known vectors */
#endif
  return 0;
}

