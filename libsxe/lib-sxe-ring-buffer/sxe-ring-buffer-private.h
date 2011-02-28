#ifndef __SXE_RING_BUFFER_PRIVATE_H__
#define __SXE_RING_BUFFER_PRIVATE_H__

// ----------------
// |    Size      | - unsigned
// ----------------
// | Itterations  | - unsigned
// ----------------
// |  Current     | - char *
// ----------------
// |  Writen End  | - char *
// ----------------
// |  Array Base  |
// .              .
// .              .

#define SXE_RING_BUFFER_SIZE_OF_INTERNALS ((sizeof(unsigned) * 2) + (sizeof(char *) * 2))

#define SXE_RING_BUFFER_SIZE       (  * (unsigned *)(base)                                                                )
#define SXE_RING_BUFFER_ITERATION  (  * (unsigned *)((char *)base + sizeof(unsigned))                                     )
#define SXE_RING_BUFFER_CURRENT    (  *    (char **)((char *)base + sizeof(unsigned) + sizeof(unsigned))                  )
#define SXE_RING_BUFFER_WRITEN_END (  *    (char **)((char *)base + sizeof(unsigned) + sizeof(unsigned) + sizeof(char *)) )
#define SXE_RING_BUFFER_ARRAY_BASE ( (char *)base + sizeof(unsigned) + sizeof(unsigned) + sizeof(char *) + sizeof(char *) )

#define SXE_RING_BUFFER_END        (SXE_RING_BUFFER_ARRAY_BASE + SXE_RING_BUFFER_SIZE - 1)


// CURRENT always points to the next available place to write in the array
// WRITEN_END points to the last writen character in the array
// BASE is the first writable spot in the array
// END is the last writable spot in the array


#endif
