/* Copyright (c) 2010 Sophos Group.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>             /* misc. UNIX functions         */

#ifdef __APPLE__
#include <spawn.h>
extern char **environ;
#endif

#include "sxe.h"
#include "sxe-log.h"
#include "sxe-spawn.h"
#include "sxe-util.h"

SXE_RETURN
sxe_spawn_init(void)
{
    SXEE60("sxe_spawn_init()");
    SXER60("return SXE_RETURN_OK");
    return SXE_RETURN_OK;
}

/**
 * Spawn a command on the end of a bidirectional TCP connection (SXE)
 *
 * @param this               = SXE whose event we are currently handling or NULL
 * @param that               = SXE_SPAWN to be constructed by this function
 * @param command            = Path to command to run
 * @param arg1               = First  argument to command or NULL if no  arguments
 * @param arg2               = Second argument to command or NULL if < 2 arguments
 * @param in_event_connected = Connection callback: Command is up
 * @param in_event_read      = Read       callback: Data ready to read
 * @param in_event_close     = Close      callback: Command has closed the connection (e.g. exited)
 *
 * @note On connection callback, SXE_USER_DATA(SXE_SPAWN_SXE(that)) contains the pid of the child
 *
 * Make sure that your event loop is running and, after calling sxe_spawn(), expect your connection callback to be called.
 * After this, you'll be able to call sxe_write().
 *
 * How to kill your children:
 * Option #1: If your child (e.g. listen on stdin) is in a blocking read then consider in parent: sxe_close(that_child)
 * Option #2: Ctrl-c the parent and all children (e.g. top) should end
 * Option #3: sxe_spawn_term() your child
 *
 * @example result = sxe_spawn(NULL, &child, "sh", "-c", "top -b -d 1 | grep --line-buffered http", event_read_top, event_close);
 */

#ifdef WIN32
/* TODO: Implement sxe_spawn() on Windows */
#else
SXE_RETURN
sxe_spawn(SXE                    * this,
          SXE_SPAWN              * that,
          const char             * command,
          const char             * arg1,
          const char             * arg2,
          SXE_IN_EVENT_CONNECTED   in_event_connected,
          SXE_IN_EVENT_READ        in_event_read,
          SXE_IN_EVENT_CLOSE       in_event_close)
{
    SXE_RETURN         result      = SXE_RETURN_ERROR_INTERNAL;
    char               path[PATH_MAX];
    int                sock;

    SXE_UNUSED_PARAMETER(this);

    SXEE67I("sxe_spawn(that=%p, command=%s, arg1=%s, arg2=%s, in_event_connected=%p, in_event_read=%p, in_event_close=%p)",
                       that,    command,    arg1,    arg2,    in_event_connected,    in_event_read,    in_event_close);

    SXEL60I("creating socket pair");

    if ((that->sxe = sxe_new_socketpair(NULL, &sock, in_event_connected, in_event_read, in_event_close)) == NULL)
    {                                                                                   /* COVERAGE EXCLUSION */
        SXEL21("Couldn't allocate SXE to spawn the '%s' command", command);             /* COVERAGE EXCLUSION */
        goto SXE_ERROR_OUT;                                                             /* COVERAGE EXCLUSION */
    }                                                                                   /* COVERAGE EXCLUSION */

    /* Close SXE's end of the socket on exec, so that child process will exit
     * when we sxe_close(that->sxe) */
    fcntl(that->sxe->socket_as_fd, F_SETFD, 1);

#ifdef __APPLE__
    {
        int err;
        posix_spawn_file_actions_t fa;
        const char *argv[] = { command, arg1, arg2, NULL };

        posix_spawn_file_actions_init(&fa);
        posix_spawn_file_actions_addclose(&fa, 0);
        posix_spawn_file_actions_addclose(&fa, 1);
        posix_spawn_file_actions_adddup2(&fa, sock, 0);
        posix_spawn_file_actions_adddup2(&fa, sock, 1);
        posix_spawn_file_actions_addclose(&fa, sock);

        err = posix_spawnp(&that->pid, command, &fa, NULL, SXE_CAST_NOCONST(char * const *, argv), environ);
        if (err != 0) {
            SXEL13I("posix_spawnp failed to run '%s': %d:%s\n", command, err, strerror(err));
            result = SXE_RETURN_ERROR_COMMAND_NOT_RUN;
            SXE_EVENT_CLOSE(that->sxe) = NULL;
            sxe_close(that->sxe);
        }
        else {
            result = SXE_RETURN_OK;
        }

        posix_spawn_file_actions_destroy(&fa);

        err = close(sock);
        SXEA61I(err != -1, "Can't close socket to self: %s", strerror(errno));
    }
#else
    switch (that->pid = fork()) {
    case -1:
        SXEA10I(0, "Aborting: fork failed");    /* Intentionally aborting at level 1; don't change to level 6         */
        break;                                  /* Coverage exclusion: fork failure after an abort (can't reach this) */

    case 0:
        SXEL60I("Spawned child process");
        sxe_close(that->sxe);
        close(0);    /* Should not need this (see dup2 manual page) */
        close(1);    /* Should not need this (see dup2 manual page) */
        SXEV61(dup2(sock, 0), == 0, "Couldn't duplicate child socket to STDIN: %s",  strerror(errno));
        SXEV61(dup2(sock, 1), == 1, "Couldn't duplicate child socket to STDOUT: %s", strerror(errno));
        close(sock);
        execlp(command, command, arg1, arg2, NULL);
        SXEL22I("Child process: failed to execute '%s' in directory '%s'", command, getcwd(path, sizeof(path)));
        exit(-1);

    default:
        close(sock);
        result = SXE_RETURN_OK;
        break;
    }
#endif

    if (result == SXE_RETURN_OK) {
        strncpy(        path, command, sizeof(path) - 1);

        if (arg1) {
            strncat(    path, " ",     sizeof(path) - 1);
            strncat(    path, arg1,    sizeof(path) - 1);

            if (arg2) {
                strncat(path, " ",     sizeof(path) - 1);    /* Coverage exclusion - logging spawn of command with two arguments */
                strncat(path, arg2,    sizeof(path) - 1);    /* Coverage exclusion - logging spawn of command with two arguments */
            }
        }

        SXEL63I("Spawned '%s': pid %d (0x%x)", path, that->pid, that->pid);
        SXE_USER_DATA_AS_INT(that->sxe) = that->pid;    /* Give SXE user access to the pid */
    }

SXE_EARLY_OR_ERROR_OUT:
    SXER61I("return %s", sxe_return_to_string(result));
    return result;
} // sxe_spawn()
#endif

#ifdef WIN32
/* TODO: Implement sxe_spawn_kill() on Windows */
#else
SXE_RETURN
sxe_spawn_kill(SXE_SPAWN * spawn, int sig)
{
    SXE_RETURN result = SXE_RETURN_ERROR_INTERNAL;
    SXE *      this   = SXE_SPAWN_GET_SXE(spawn);

    SXE_UNUSED_PARAMETER(this); /* Used only for diagnostics */
    SXE_UNUSED_PARAMETER(sig); /* Used only for diagnostics */

    SXEE62I("sxe_spawn_kill(spawn->pid=%d,sig=%d)", SXE_SPAWN_GET_PID(spawn), sig);

    if (kill(SXE_SPAWN_GET_PID(spawn), sig) < 0) {
        SXEL63I("Couldn't kill process %d with signal %d: %s", SXE_SPAWN_GET_PID(spawn), sig, strerror(errno));
        goto SXE_ERROR_OUT;
    }

    result = SXE_RETURN_OK;

SXE_EARLY_OR_ERROR_OUT:
    SXER61I("return %s", sxe_return_to_string(result));
    return result;
}
#endif

/**
 * Spawn a command on the end of a unidirectional pipe and collect the output
 *
 * @param cmd       = Command to run
 * @param buf       = A buffer for resulting output
 * @param buf_max   = The max length of the buf parameter
 * @return          = The number of bytes writen
 *
 * @note the command specified will be run in a 'sh -c'
 *
 * @example buf_used = sxe_spawn_backticks("ls -la | grep \.log", buf, sizeof(buf));
 */

unsigned int
sxe_spawn_backticks(const char * cmd, char * buf, unsigned int buf_max)
{
    FILE         * fp;
    unsigned int   bytes_read    = 0;
    unsigned int   bytes_written = 0;

    SXEE83("sxe_spawn_backticks(cmd=%s, buf=%p, buf_max=%u)", cmd, buf, buf_max);
    SXEA80(buf_max > 1, "sxe_spawn_backticks: Parameter buf_max is zero!");

#ifdef _WIN32
    fp = _popen(cmd, "r");
#else
    fp = popen(cmd, "r");
#endif

    SXEA81(fp != NULL, "sxe_spawn_backticks: popen failed to exec command '%s'", cmd);

    while ((bytes_read = fread(buf + bytes_written, sizeof(char), (buf_max - bytes_written - 1), fp)) != 0) {
        SXEL81("read %u bytes from the pipe", bytes_read);
        bytes_written += bytes_read;
    }

    buf[bytes_written] = '\0';
#ifdef _WIN32
    SXEV80(_pclose(fp), == 0, "sxe_spawn_backticks: failed to close pipe");
#else
    SXEV80(pclose(fp), == 0, "sxe_spawn_backticks: failed to close pipe");
#endif

    SXER80("return");
    return bytes_written;
} // sxe_spawn_backticks()

