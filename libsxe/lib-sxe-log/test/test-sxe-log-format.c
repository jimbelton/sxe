#include <pthread.h>
#include <string.h>

#include "sxe-log.h"
#include "tap.h"

#if defined(__APPLE__) || defined(__FreeBSD__)
#   define PREFIX_FORMAT "YYYYMMDD HHmmSS.mmm P01234567789 T0123456789"
#else    // Linux and Windows
#   define PREFIX_FORMAT "YYYYMMDD HHmmSS.mmm T0123456789"
#endif

static SXE_LOG_LEVEL last_level;
static char *        last_line;

static void
test_log_line_out(SXE_LOG_LEVEL level, const char * line)
{
    static char buf[SXE_LOG_BUFFER_SIZE];

    last_level = level;
    line      += sizeof(PREFIX_FORMAT);
    last_line  = buf;
    sxe_strlcpy(buf, line, sizeof(buf));
    SXEA1(strlen(buf) == 0 || buf[strlen(buf) - 1] == '\n', "Line is not newline terminated");
    buf[strlen(buf) - 1] = '\0';
}

static void
test_token(char **line, const char *token, const char *message)
{
    char * end;

    if (**line == ' ')    // Skip leading blanks
        *line += strspn(*line, " ");

    if ((end = strchr(*line, ' '))) {
        *end = '\0';
        is_eq(*line, token, message);
        *line = end + 1;
    }
    else
        fail("No blanks left in line '%s'", *line);
}

static void *
other_thread_log_warning(void *dummy)
{
    SXE_UNUSED_PARAMETER(dummy);
    SXEL3("No id expected");
    return NULL;
}

int
main(void)
{
    pthread_t thread;
    void *    retval;

    plan_tests(18);
    sxe_log_hook_line_out(test_log_line_out);

    SXEL1("booga, booga!");
    is(last_level, SXE_LOG_LEVEL_FATAL, "SXEL1 level is FATAL");
    test_token(&last_line, "------",    "No transaction id set");
    test_token(&last_line, "1",         "Level indicator is 1 (FATAL)");
    is_eq(last_line, "- booga, booga!", "Line is as expected");

    sxe_log_set_options(SXE_LOG_OPTION_LEVEL_TEXT);
    sxe_log_set_thread_id(666);
    SXEL1("booga, booga!");
    is(last_level, SXE_LOG_LEVEL_FATAL, "SXEL1 level is FATAL");
    test_token(&last_line, "666",       "Transaction id is 666");
    test_token(&last_line, "FAT",       "Level indicator is FAT (FATAL)");
    is_eq(last_line, "- booga, booga!", "Line is as expected");

    sxe_log_set_options(SXE_LOG_OPTION_LEVEL_TEXT | SXE_LOG_OPTION_ID_HEX);
    sxe_log_set_thread_id(0x666);
    SXEL2(": booga, booga!");
    is(last_level, SXE_LOG_LEVEL_ERROR, "SXEL2 level is ERROR");
    test_token(&last_line, "00000666",  "No transaction id is 00000666");
    test_token(&last_line, "ERR",       "Level indicator is ERR (ERROR)");
    is_eq(last_line, "- main: booga, booga!", "Line is as expected");

    is(pthread_create(&thread, NULL, other_thread_log_warning, NULL), 0, "Ceated a thread");
    is(pthread_join(thread, &retval), 0,                                 "Joined the thread");
    is(last_level, SXE_LOG_LEVEL_WARNING, "SXEL3 level is WARNING");
    test_token(&last_line, "--------",    "No transaction id");
    test_token(&last_line, "WAR",         "Level indicator is WAR (wARNING)");
    is_eq(last_line, "- No id expected",  "Line is as expected");

    return exit_status();
}
