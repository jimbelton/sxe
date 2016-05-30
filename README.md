Project SXE
===========

The SXE project implements two components, and powerful build environment (mak) and an event driven C programming library, libsxe.
The original codebase was developed at Sophos in Vancouver in 2010. Since then, there have been a few minor updates.

For more information, refer to the [Project SXE Wiki](https://github.com/jimbelton/sxe/wiki)

May 2016 Update
===============
 * Fixed a bug in lib-sxe where the user data was clipped to 4 bytes when accessed as an integer, which had broken the web server
 * Significant improvements to the lib-sxe-httpd webserver; these break backward compatibility, so if you use the web server, you may have to make minor changes to your code
 * Added the option to parse URLs that don't include a scheme (e.g. http://)
 * New less hacky implementation of spawnl() portability function and added a unit test
 * Lot's of changes to allow 64 bit compilation of the unit tests with the latest version of gcc and libc
 * Disabled a few tests that I couldn't get to work; these were all self spawning multiprocess tests
