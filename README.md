Project SXE
===========

The SXE project implements two components, a powerful build environment (mak) and an event driven C programming library, libsxe.
The original codebase was developed at Sophos in Vancouver in 2010. In August 2021, a new version of the codebase, developed at
Cisco, was released as open source and has been merged into this repository. In September of 2022, the sxe-jitson JSON parser,
which I developed independently, but is now in use at Cisco. In Feb 2023, I added sxe-dict, based on the open source hashdict.c
github project.

For more information, refer to the [Project SXE Wiki](https://github.com/jimbelton/sxe/wiki)

Feb 2022: Release 2.3
=====================
 * sxe-dict: Dynamic dictionary based on hashdict.c
 * sxe-thread: Track and free per thread allocations.

Nov 2022: Release 2.2
=====================
 * sxe-jitson: Extension to parse and return identifiers
 * sxe-jitson: Added sxe_jitson_create_uint helper
 * sxe-jitson: Can have more than one \u#### in a string
 * sxe-jitson: Support registering operations on jitsons
 * Fix a minor bug in the code to allow compilation on libssl3
 * sxe-jitson: sxe_jitson_dup generates broken duplicates
 * sxe-jitson: construct object from multiple objects
 * sxe-jitson: support Strict JSON parsing
 * sxe-jitson: Allow non-NUL terminated source buffers
 * sxe-jitson: numbers > 999999 are incorrectly formatted
 * Compile on Ubuntu 20.04LTS
 * sxe-jitson: Can't create a reference to an allocated reference

Jan 2022: Release 2.1
=====================
 * Added lib-sxe-jitson (JSON, unicode, and generic factory)
 * Support hexadecimal unsigned integers in sxe-jitson
 * Update sxe-jitson to use type objects
 * Support references in sxe-jitson
 * Add sxe_jitson_dup
 * sxe-jitson const values
 * fix sxe_jitson_object_get_member core dump
 * Size of an empty object is 0, not 1
 * Fixed more SXE jitson bugs
 * It should be possible to construct an object with an arr
 * sxe_jitson_object_get_member should allow non-NUL termin
 * sxe_jitson_stack_push_string should be a global to allow
 * MOCKFAIL should parenthesize (expr) for the release build
 * Need to be able to duplicate into a collection
 * sxe_dup should duplicate referred to jitson values
 * sxe-jitson ease of use
 * lib-sxe-alloc: allow use of jemalloc
 * Add helper functions for creating jitson
 * No longer build in libtap by default
 * sxe-jitson: Optional extension to support named constants

Aug 2021: Release 2.0
=====================
New in this release:
 * Sophos MD5 interface wrapper for OpenSSL
 * New mock macros allowing mocking function returns in debug builds
 * Murmerhash3 non-cryptographic hash function
 * A buffer abstraction, which is used in the network interface
 * A CDB (constant database) implementation
 * Support for auth in the HTTP client
 * Simplified logging interface
 * BSD like sxe_strlcpy and sxe_strlcat functions
 * Tempfile generation.

Removed in this release:
 * Embedded LUA interpreter and interface module
 * sxe-expose module

May 2016: Release 1.1
=====================
 * Fixed a bug in lib-sxe where the user data was clipped to 4 bytes when accessed as an integer, which had broken the web server
 * Significant improvements to the lib-sxe-httpd webserver; these break backward compatibility, so if you use the web server, you may have to make minor changes to your code
 * Added the option to parse URLs that don't include a scheme (e.g. http://)
 * New less hacky implementation of spawnl() portability function and added a unit test
 * Lot's of changes to allow 64 bit compilation of the unit tests with the latest version of gcc and libc
 * Disabled a few tests that I couldn't get to work; these were all self spawning multiprocess tests
