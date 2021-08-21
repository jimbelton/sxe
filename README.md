Project SXE
===========

The SXE project implements two components, a powerful build environment (mak) and an event driven C programming library, libsxe.
The original codebase was developed at Sophos in Vancouver in 2010. In August 2021, a new version of the codebase, developed at Cisco, was released as open source and has been merged into this repository.

For more information, refer to the [Project SXE Wiki](https://github.com/jimbelton/sxe/wiki)

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
