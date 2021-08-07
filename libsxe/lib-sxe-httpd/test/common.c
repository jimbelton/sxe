#include "tap.h"
#include "sxe-httpd.h"
#include "sxe-test.h"
#include "sxe-util.h"

#include "common.h"

tap_ev_queue q_client;
tap_ev_queue q_httpd;

void
client_connect(SXE * this)
{
    SXEE6I("()");
    SXEA1I(q_client != NULL, "asdf");
    tap_ev_queue_push(q_client, __func__, 1, "this", this);
    SXER6I("return");
}

void
client_read(SXE * this, int length)
{
    SXE_UNUSED_PARAMETER(length);
    SXEE6I("%s(length=%u)", __func__, length);
    tap_ev_queue_push(q_client, __func__, 3, "this", this, "buf", tap_dup(SXE_BUF(this), SXE_BUF_USED(this)),
                      "used", SXE_BUF_USED(this));
    sxe_buf_clear(this);
    SXER6I("return");
}

void
client_sent(SXE * this, SXE_RETURN result)
{
    SXEE6I("%s(result=%s)", __func__, sxe_return_to_string(result));
    tap_ev_queue_push(q_client, __func__, 2, "this", this, "result", result);
    SXER6I("return");
}

void
client_close(SXE * this)
{
    SXEE6I("%s(this=%p)", __func__, this);
    tap_ev_queue_push(q_client, __func__, 1, "this", this);
    SXER6("return");
}

void
h_connect(struct SXE_HTTPD_REQUEST *request)
{
    SXE * this = sxe_httpd_request_get_sxe(request);
    SXE_UNUSED_PARAMETER(this);
    SXEE6I("()");
    tap_ev_queue_push(q_httpd, __func__, 2, "request", request, "this", this);
    SXER6I("return");
}

void
h_request(struct SXE_HTTPD_REQUEST *request, const char *method, unsigned mlen, const char *url, unsigned ulen, const char *version, unsigned vlen)
{
    SXE * this = sxe_httpd_request_get_sxe(request);
    SXE_UNUSED_PARAMETER(this);
    SXEE6I("%s(method=[%.*s],url=[%.*s],version=[%.*s])", __func__, mlen, method, ulen, url, vlen, version);
    tap_ev_queue_push(q_httpd, __func__, 5, "request", request, "url", tap_dup(url, ulen), "method", tap_dup(method, mlen),
                      "version", tap_dup(version, vlen), "this", this);
    SXER6I("return");
}

void
h_header(struct SXE_HTTPD_REQUEST *request, const char *key, unsigned klen, const char *val, unsigned vlen)
{
    SXE * this = sxe_httpd_request_get_sxe(request);
    SXE_UNUSED_PARAMETER(this);
    SXEE6I("%s(key=[%.*s],value=[%.*s])", __func__, klen, key, vlen, val);
    tap_ev_queue_push(q_httpd, __func__, 3, "request", request, "key", tap_dup(key, klen), "value", tap_dup(val, vlen));
    SXER6I("return");
}

void
h_eoh(struct SXE_HTTPD_REQUEST *request)
{
    SXE * this = sxe_httpd_request_get_sxe(request);
    SXE_UNUSED_PARAMETER(this);
    SXEE6I("()");
    tap_ev_queue_push(q_httpd, __func__, 1, "request", request);
    SXER6I("return");
}

void
h_body(struct SXE_HTTPD_REQUEST *request, const char *buf, unsigned used)
{
    SXE * this = sxe_httpd_request_get_sxe(request);
    SXE_UNUSED_PARAMETER(this);
    SXEE6I("%s(buf=%p,used=%u) // '%.*s'", __func__, buf, used, used, buf);
    tap_ev_queue_push(q_httpd, __func__, 3, "request", request, "buf", tap_dup(buf, used), "used", used);
    SXER6I("return");
}

void
h_respond(struct SXE_HTTPD_REQUEST *request)
{
    SXE * this = sxe_httpd_request_get_sxe(request);
    SXE_UNUSED_PARAMETER(this);
    SXEE6I("()");
    tap_ev_queue_push(q_httpd, __func__, 1, "request", request);
    SXER6I("return");
}

void
h_close(struct SXE_HTTPD_REQUEST *request)
{
    SXE * this = sxe_httpd_request_get_sxe(request);
    SXEE6I("()");
    tap_ev_queue_push(q_httpd, __func__, 2, "request", request, "this", this);
    SXER6I("return");
}

void
h_sent(SXE_HTTPD_REQUEST *request, SXE_RETURN result, void *user_data)
{
    SXE * this = sxe_httpd_request_get_sxe(request);
    SXE_UNUSED_PARAMETER(this);
    SXE_UNUSED_PARAMETER(user_data);
    SXEE6I("()");
    tap_ev_queue_push(q_httpd, __func__, 2, "request", request, "result", result);
    SXER6I("return");
}
