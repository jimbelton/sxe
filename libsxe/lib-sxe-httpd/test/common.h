#ifndef __COMMON_H__
#define __COMMON_H__

#include "sxe.h"
#include "sxe-httpd.h"
#include "sxe-test.h"
#include "tap.h"

extern tap_ev_queue q_client;
extern tap_ev_queue q_httpd;

void client_connect(SXE * this);
void client_read(SXE * this, int length);
void client_sent(SXE * this, SXE_RETURN result);
void client_close(SXE * this);

void h_connect(SXE_HTTPD_REQUEST *request);
void h_request(SXE_HTTPD_REQUEST *request, const char *method, unsigned mlen, const char *url, unsigned ulen, const char *version, unsigned vlen);
void h_header(SXE_HTTPD_REQUEST *request, const char *key, unsigned klen, const char *val, unsigned vlen);
void h_eoh(SXE_HTTPD_REQUEST *request);
void h_body(SXE_HTTPD_REQUEST *request, const char *buf, unsigned used);
void h_respond(SXE_HTTPD_REQUEST *request);
void h_close(SXE_HTTPD_REQUEST *request);
void h_sent(SXE_HTTPD_REQUEST *request, SXE_RETURN result, void *user_data);

static inline void
test_sxe_register_and_init(int clients)
{
    sxe_register(clients, 0);
    sxe_init();

    q_client = tap_ev_queue_new();
    q_httpd  = tap_ev_queue_new();

#if defined(ENABLE_SSL) && !defined(SXE_DISABLE_OPENSSL)
    {
        const char *cert = "../../lib-sxe/test/test-sxe-ssl.cert";
        const char *pkey = "../../lib-sxe/test/test-sxe-ssl.pkey";
        sxe_ssl_register(clients);
        sxe_ssl_init(cert, pkey, cert, ".");
    }
#endif
}

static inline SXE *
test_httpd_listen(SXE_HTTPD *server, const char * iface, unsigned short port)
{
    SXE * sxe = sxe_httpd_listen(server, iface, port);
#if defined(ENABLE_SSL) && !defined(SXE_DISABLE_OPENSSL)
    sxe_ssl_enable(sxe);
#endif
    return sxe;
}

static inline SXE *
test_new_tcp(SXE * ignored, const char * iface, unsigned short port, SXE_IN_EVENT_CONNECTED on_conn, SXE_IN_EVENT_READ on_read, SXE_IN_EVENT_CLOSE on_close)
{
    SXE * sxe = sxe_new_tcp(ignored, iface, port, on_conn, on_read, on_close);
#if defined(ENABLE_SSL) && !defined(SXE_DISABLE_OPENSSL)
    sxe_ssl_enable(sxe);
#endif
    return sxe;
}

static inline void
test_sxe_send(SXE * this, const char * buffer, unsigned length, SXE_OUT_EVENT_WRITTEN on_sent, const char * event, tap_ev_queue queue, ev_tstamp timeout, tap_ev * ev)
{
    SXE_RETURN res = sxe_send(this, buffer, length, on_sent);

    if (res == SXE_RETURN_OK) {
        pass("sxe_send() returned immediately -- no need to wait for %s event", event);
        return;
    }

    is_eq(test_tap_ev_queue_identifier_wait(queue, timeout, ev), event, "Got correct event: %s", event);
}

#define TEST_SXE_SEND_LITERAL(this, string, callback, queue, timeout, ev) \
    test_sxe_send(this, string, SXE_LITERAL_LENGTH(string), callback, #callback, queue, timeout, ev);

#endif
