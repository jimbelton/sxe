libsxe
======

This repo contains a branch of the libsxe [1] used by opendnscache plus
the pair reviewed but not production tested sxe-cdb.

This libsxe gets linked against zeus & open resty (aka nginx).

[1] https://github.office.opendns.com/OpenDNS/opendnscache

submodules
==========

Submodules packaged in __libsxe__ include:

 * lib-ev
 * lib-lookup3
 * lib-md5
 * lib-mock
 * lib-murmurhash3
 * lib-port
 * lib-sha1
 * lib-sxe
 * lib-sxe-buffer
 * lib-sxe-cdb
 * lib-sxe-dirwatch
 * lib-sxe-hash
 * lib-sxe-http
 * lib-sxe-httpd
 * lib-sxe-list
 * lib-sxe-log
 * lib-sxe-mmap
 * lib-sxe-pool
 * lib-sxe-pool-tcp
 * lib-sxe-socket
 * lib-sxe-spawn
 * lib-sxe-sync-ev
 * lib-sxe-test
 * lib-sxe-thread
 * lib-sxe-util
 * lib-tap

build
=====

__libsxe__ can be exported as a debian package and __include__ d as a
third party library. To build the package follow the instructions in
__sxe__ builder repository.

usage
=====

Given a debian package of __libsxe__: `opendns-sxe_3.1.0_amd64.deb`,
install the package:

```
dpkg -i opendns-sxe_3.1.0_amd64.deb
```

This should add the shared library and symlink:

```
$ ls -la /usr/lib
lrwxrwxrwx  1 root root     11 Jan 22 15:04 libsxe.so -> libsxe.so.3
lrwxrwxrwx  1 root root     15 Jan 22 15:04 libsxe.so.3 -> libsxe.so.3.1.0
-rw-r--r--  1 root root 239776 Jan 22 15:04 libsxe.so.3.1.0
```

and add header files:

```
$ ls -la /usr/include/sxe
total 340
drwxr-xr-x  2 root root  4096 Jan 18 12:05 .
drwxr-xr-x 44 root root  4096 Jan 18 12:05 ..
-rw-r--r--  1 root root 23022 Jan 18 11:02 ev.h
-rw-r--r--  1 root root  2805 Jan 18 11:02 lib-sha1-proto.h
-rw-r--r--  1 root root  1332 Jan 18 11:02 lib-sxe-buffer-proto.h
-rw-r--r--  1 root root  7130 Jan 18 11:02 lib-sxe-cdb-proto.h
-rw-r--r--  1 root root  2726 Jan 18 11:02 lib-sxe-hash-proto.h
-rw-r--r--  1 root root  6920 Jan 18 11:02 lib-sxe-http-proto.h
-rw-r--r--  1 root root  2081 Jan 18 11:02 lib-sxe-list-proto.h
-rw-r--r--  1 root root  6432 Jan 18 11:02 lib-sxe-pool-proto.h
-rw-r--r--  1 root root  6721 Jan 18 11:02 lib-sxe-proto.h
...
```


header files within the __sxe__ directory create a namespace. The
following is an example using murmurhash. Create a file named
__main.c__ with the following contents:

```
#include <stdio.h>
#include <stdint.h>

#include <sxe/murmurhash3.h>

int
main() {
	uint32_t obj[5] = {1, 2, 3, 4};
	uint32_t out;

	out = murmur3_32(obj, 4, 1234);
	printf("murmurhash: %d\n", out);
}
```

To compile and link:

```
gcc -Wall -Wextra -o main.o -c main.c
gcc -Wall -Wextra -o prog main.o -lsxe -lssl -ltap
```

And run:

```
$ ./prog
output: -1366845423
```
