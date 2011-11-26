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

#ifndef SXE_DISABLE_OPENSSL

#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "sxe.h"
#include "sxe-pool.h"

typedef enum SXE_SSL_STATE {
    SXE_SSL_STATE_FREE=0,
    SXE_SSL_STATE_CONNECTED,
    SXE_SSL_STATE_ESTABLISHED,
    SXE_SSL_STATE_READING,
    SXE_SSL_STATE_WRITING,
    SXE_SSL_STATE_CLOSING,
    SXE_SSL_STATE_NUMBER_OF_STATES
} SXE_SSL_STATE;

typedef enum SXE_SSL_MODE {
    SXE_SSL_MODE_INVALID=0,
    SXE_SSL_MODE_CLIENT,
    SXE_SSL_MODE_SERVER
} SXE_SSL_MODE;

typedef struct SXE_SSL {
    SSL          * conn;                /* freed with SSL_free()              */
    X509         * peer_certificate;    /* freed with X509_free()             */
    bool           stopped;             /* are read events blocked right now? */
    SXE_SSL_MODE   mode;
    int            verified;            /* was the peer certificate verified? */
    char           cipher[42];
    int            bits;
    char           version[42];
    size_t         mem_total;
    size_t         mem_high;
} SXE_SSL;

extern struct ev_loop * sxe_private_main_loop;

static unsigned   sxe_ssl_total = 0;
static SXE_SSL  * sxe_ssl_array = NULL;
static SSL_CTX  * sxe_ssl_ctx   = NULL;

/* SSL memory allocation functions.
 *
 * OpenSSL allocates memory in the following cases:
 *
 * 1) when we create a new SSL_CTX structure during sxe_ssl_init(). Since we only do this once,
 *    we don't really care how much memory this uses.
 *
 * 2) each time we create a new SSL structure during setup_ssl_socket(). We do this once per SSL
 *    socket, so we care more, but in practice this only consumes ~4KB of memory. Unfortunately,
 *    OpenSSL also lazily allocates a few other bits of memory that it stores in the SSL_CTX
 *    structure, so the first SSL structure ends up being "dinged" a bit more than the others.
 *
 * 3) each time we call SSL_read() or SSL_write(), the SSL object allocates a read or write
 *    buffer. The buffer persists until there have been enough calls to SSL_read() to drain it;
 *    for SSL_write(), it persists until the buffer has been entirely written to the socket.
 */

static struct {
    SXE     * this;
    size_t    total;
    size_t    high;

    enum {
        MEM_SSL_CTX_NEW,
        MEM_SSL_CTX_FREE,
        MEM_SSL_NEW,
        MEM_SSL_FREE,
        MEM_SSL_HANDSHAKE,
        MEM_SSL_SHUTDOWN,
        MEM_SSL_READ,
        MEM_SSL_WRITE,
        MEM_SSL_WTF
    }         state;
}             mem_state;

#define MEM_SET_STATE(t, s) do { mem_state.this = (t); mem_state.state = (s); } while (0)
#define MEM_INC(h, t, i) do { (t) += (i); (h) = (h) > (t) ? (h) : (t); } while (0)
#define MEM_DEC(   t, i) do { (t) -= (i); } while (0)

#if SXE_DEBUG
#define MEM_GET_STATE() mem_state_as_string()
static const char *
mem_state_as_string(void)
{
    switch (mem_state.state) {
    case MEM_SSL_CTX_NEW  : return "SSL_CTX_new"  ;
    case MEM_SSL_CTX_FREE : return "SSL_CTX_free" ;
    case MEM_SSL_NEW      : return "SSL_new"      ;
    case MEM_SSL_FREE     : return "SSL_free"     ;
    case MEM_SSL_HANDSHAKE: return "SSL_handshake";
    case MEM_SSL_SHUTDOWN : return "SSL_shutdown" ;
    case MEM_SSL_READ     : return "SSL_read"     ;
    case MEM_SSL_WRITE    : return "SSL_write"    ;
    default:
    case MEM_SSL_WTF      : return "W?T?F?"       ;
    }
}
#endif

static void *
mem_alloc(size_t size)
{
    size_t   alloc;
    size_t * mem;
    SXE    * this = mem_state.this;

    SXE_UNUSED_PARAMETER(this);

    alloc = size + sizeof(size_t);
    mem = malloc(alloc);
    mem[0] = size;
    MEM_INC(mem_state.high, mem_state.total, size);

    if (this) {
        SXE_SSL * ssl = &sxe_ssl_array[this->ssl_id];
        SXEA1I(this->ssl_id != SXE_POOL_NO_INDEX, "SSL-MEM: invalid ssl_id pointer");
        MEM_INC(ssl->mem_high, ssl->mem_total, size);
        SXEL6I("%13s: %10lu=hitot %10lu=total %10lu=myhi %10lu=mytot %6lu+++", MEM_GET_STATE(), (unsigned long)mem_state.high,
               (unsigned long)mem_state.total, (unsigned long)ssl->mem_high, (unsigned long)ssl->mem_total, (unsigned long)size);
    }
    else {
        SXEL6I("%13s: %10lu=hitot %10lu=total ----------=myhi ----------=mytot %6lu+++", MEM_GET_STATE(),
               (unsigned long)mem_state.high, (unsigned long)mem_state.total, (unsigned long)size);
    }

    return &mem[1];
}

static void *
mem_realloc(void * old, size_t newsize)
{
    size_t * newmem;
    size_t * oldmem;
    size_t   alloc;
    size_t   oldsize;
    SXE    * this = mem_state.this;

    SXE_UNUSED_PARAMETER(this);

    oldmem = old;
    oldmem--; /* get back to the size */
    oldsize = oldmem[0];

    alloc  = newsize + sizeof(size_t);
    newmem = realloc(oldmem, alloc);
    newmem[0] = newsize;
    MEM_INC(mem_state.high, mem_state.total, newsize - oldsize);

    if (this) {
        SXE_SSL * ssl = &sxe_ssl_array[this->ssl_id];

        SXEA1I(this->ssl_id != SXE_POOL_NO_INDEX, "SSL-MEM: invalid ssl_id pointer");
        MEM_INC(ssl->mem_high, ssl->mem_total, newsize - oldsize);
        SXEL6I("%13s: %10lu=hitot %10lu=total %10lu=myhi %10lu=mytot %6lu+++ %6lu---", MEM_GET_STATE(),
               (unsigned long)mem_state.high, (unsigned long)mem_state.total, (unsigned long)ssl->mem_high,
               (unsigned long)ssl->mem_total, (unsigned long)newsize, (unsigned long)oldsize);
    }
    else {
        SXEL6I("%13s: %10lu=hitot %10lu=total ----------=myhi ----------=mytot %6lu+++ %6lu---", MEM_GET_STATE(),
               (unsigned long)mem_state.high, (unsigned long)mem_state.total, (unsigned long)newsize, (unsigned long)oldsize);
    }

    return &newmem[1];
}

static void
mem_free(void * ptr)
{
    size_t * mem = ptr;
    size_t   oldsize;
    SXE    * this = mem_state.this;

    SXE_UNUSED_PARAMETER(this);

    if (ptr == NULL) {
        return; /* coverage exclusion: OpenSSL doesn't always free(null) */
    }

    mem--;
    oldsize = mem[0];
    MEM_DEC(mem_state.total, oldsize);

    if (this) {
        SXE_SSL * ssl = &sxe_ssl_array[this->ssl_id];
        SXEA1I(this->ssl_id != SXE_POOL_NO_INDEX, "SSL-MEM: invalid ssl_id pointer");
        MEM_DEC(ssl->mem_total, oldsize);
        SXEL6I("%13s: %10lu=hitot %10lu=total %10lu=myhi %10lu=mytot           %6lu---", MEM_GET_STATE(),
               (unsigned long)mem_state.high, (unsigned long)mem_state.total, (unsigned long)ssl->mem_high,
               (unsigned long)ssl->mem_total, (unsigned long)oldsize);
    }
    else {
        SXEL6I("%13s: %10lu=hitot %10lu=total ----------=myhi ----------=mytot           %6lu---", MEM_GET_STATE(),
               (unsigned long)mem_state.high, (unsigned long)mem_state.total, (unsigned long)oldsize);
    }

    free(mem);
}

void
sxe_ssl_instrument_memory_functions(void)
{
    SXEE7("()");
    SXEA1(sxe_ssl_array == NULL, "%s() called after sxe_ssl_init() -- too late!", __func__);
    CRYPTO_set_mem_functions(mem_alloc, mem_realloc, mem_free);
    SXER7("return");
}

static const char *
sxe_ssl_state_to_string(SXE_SSL_STATE state)                  /* Coverage exclusion: state-to-string */
{                                                             /* Coverage exclusion: state-to-string */
    switch (state) {                                          /* Coverage exclusion: state-to-string */
    case SXE_SSL_STATE_FREE        : return "FREE";           /* Coverage exclusion: state-to-string */
    case SXE_SSL_STATE_CONNECTED   : return "CONNECTED";      /* Coverage exclusion: state-to-string */
    case SXE_SSL_STATE_ESTABLISHED : return "ESTABLISHED";    /* Coverage exclusion: state-to-string */
    case SXE_SSL_STATE_READING     : return "READING";        /* Coverage exclusion: state-to-string */
    case SXE_SSL_STATE_WRITING     : return "WRITING";        /* Coverage exclusion: state-to-string */
    default:                         return NULL;             /* Coverage exclusion: state-to-string */
    }                                                         /* Coverage exclusion: state-to-string */
}                                                             /* Coverage exclusion: state-to-string */

static void
sxe_ssl_free(SXE * this, SXE_SSL * ssl, SXE_SSL_STATE state)
{
    SXEE7I("(state=%s)", sxe_ssl_state_to_string(state));

    if (ssl->conn) {
        SSL_free(ssl->conn);
        ssl->conn = NULL;
    }
    if (ssl->peer_certificate) {
        X509_free(ssl->peer_certificate);
        ssl->peer_certificate = NULL;
    }

    sxe_pool_set_indexed_element_state(sxe_ssl_array, this->ssl_id, state, SXE_SSL_STATE_FREE);
    this->ssl_id = SXE_POOL_NO_INDEX;

    SXER7I("return");
}

void
sxe_ssl_register(unsigned number_of_connections)
{
    SXEE6("(number_of_connections=%u)", number_of_connections);
    sxe_ssl_total += number_of_connections;
    SXER6("return");
}

/* The cipherlist we're using is quite restricted: we prefer RC4 because it's
 * a stream cipher, so requires no padding; otherwise we'll use AES because
 * it's fast in modern Intel processors: but only AES128, which is faster than
 * other AES variants.
 */
static const char cipherlist[] =
#if 1
                                 "RC4-SHA:RC4-MD5:AES128-SHA:DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA"
#else
                                 "HIGH;MEDIUM;!LOW;!EXPORT;!eNULL;!aNULL;!SSLv2"
#endif
                                 ;

extern const char * SSL_version_str;

SXE_RETURN
sxe_ssl_init(const char *cert_path, const char *key_path, const char *CAfile, const char *CAdir)
{
    SXE_RETURN          result = SXE_RETURN_OK;
    char                errstr[1024];
    long                mode;
    STACK_OF(SSL_COMP)* comp_methods;

    SXEE6("(cert_path=%s,key_path=%s,CAfile=%s,CAdir=%s)",
            cert_path, key_path, CAfile ? CAfile : "(null)", CAdir ? CAdir : "(null)");
    MEM_SET_STATE(NULL, MEM_SSL_CTX_NEW);

    /* Something like this is probably a good idea, but I don't want to
     * prevent people from linking against a DLL version of OpenSSL and *not*
     * having to recompile the whole application.
    SXEA1(strcmp(SSL_version_str, OPENSSL_VERSION_TEXT) == 0,
           "Compiled with '%s'; runtime version is '%s'", OPENSSL_VERSION_TEXT, SSL_version_str);
     */

    SSL_load_error_strings();
    SSL_library_init();
    comp_methods = SSL_COMP_get_compression_methods();
    sk_SSL_COMP_zero(comp_methods);

    /* SSL_write() can't stop SIGPIPE, so we have to block it globally in
     * applications that use sxe_ssl */
    signal(SIGPIPE, SIG_IGN);

    SXEL6("Allocating %u SXE_SSLs of %zu bytes each", sxe_ssl_total, sizeof(SXE_SSL));
    sxe_ssl_array = sxe_pool_new("sxe_ssl_pool", sxe_ssl_total, sizeof(SXE_SSL), SXE_SSL_STATE_NUMBER_OF_STATES, 0);
    sxe_pool_set_state_to_string(sxe_ssl_array, sxe_ssl_state_to_string);

    ERR_clear_error();

    /* Use SSLv2 handshake, but only allow TLSv1 or SSLv3. */
    SXEA1((sxe_ssl_ctx = SSL_CTX_new(SSLv23_method())) != NULL,  "Failed to create SSL context: %lu",            ERR_get_error());
    SSL_CTX_set_options(sxe_ssl_ctx, SSL_OP_NO_SSLv2);
    SXEA1(SSL_CTX_set_cipher_list(sxe_ssl_ctx, cipherlist) == 1, "Failed to set SSL context's cipher list: %lu", ERR_get_error());

    mode = SSL_CTX_get_mode(sxe_ssl_ctx);

    /* Allow SSL_write() to return non-negative numbers after writing a single
     * SSL record. */
    mode |= SSL_MODE_ENABLE_PARTIAL_WRITE;

#ifdef SSL_MODE_RELEASE_BUFFERS
    /* SSL_MODE_RELEASE_BUFFERS is a new feature in OpenSSL 1.0.0 that
     * allocates and frees internal read and write buffers for each TLS
     * fragment, so idle connections consume less memory. */
    mode |= SSL_MODE_RELEASE_BUFFERS;
#   ifndef OPENSSL_NO_BUF_FREELISTS
    sxe_ssl_ctx->freelist_max_len = 0;  /* call free()! */
#   endif
#endif

    SSL_CTX_set_mode(sxe_ssl_ctx, mode);

#ifdef SSL_CTX_set_max_send_fragment
    /* Don't need to shrink the maximum send fragment size, because OpenSSL
     * always allocates enough memory for the biggest frame size anyway! So it
     * doesn't save any memory, and can only increase SSL overhead and network
     * fragmentation. */
    //SSL_CTX_set_max_send_fragment(sxe_ssl_ctx, SXE_BUF_SIZE);
#endif

    SXEA1(SSL_CTX_use_certificate_file(sxe_ssl_ctx, cert_path, SSL_FILETYPE_PEM) == 1,
            "Failed to set SSL context's certificate file '%s': %s", cert_path, ERR_error_string(ERR_get_error(), errstr));
    SXEA1(SSL_CTX_use_PrivateKey_file(sxe_ssl_ctx, key_path, SSL_FILETYPE_PEM) == 1,
            "Failed to set SSL context's PrivateKey file '%s': %s", key_path, ERR_error_string(ERR_get_error(), errstr));
    SXEA1(SSL_CTX_load_verify_locations(sxe_ssl_ctx, CAfile, CAdir) == 1,
            "Failed to set SSL context's verify locations dir='%s' file='%s': %s", CAdir, CAfile, ERR_error_string(ERR_get_error(), errstr));

    /* Server mode: the server will not send a client certificate request to
     * the client, so the client will not send a certificate.
     *
     * Client mode: the server will send a certificate which will be checked.
     * The result of the certificate verification process can be checked after
     * the TLS/SSL handshake using the SSL_get_verify_result(3) function.  The
     * handshake will be continued regardless of the verification result.
     */
    SSL_CTX_set_verify(sxe_ssl_ctx, SSL_VERIFY_NONE, NULL);

    SXEL5("OpenSSL version %s", SSL_version_str);

    SXER6("return %d // %s", result, sxe_return_to_string(result));
    return result;
}

void
sxe_ssl_fini(void)
{
    MEM_SET_STATE(NULL, MEM_SSL_CTX_FREE);
    SSL_CTX_free(sxe_ssl_ctx);
    sxe_ssl_ctx = NULL;
}

SXE_RETURN
sxe_ssl_enable(SXE * this)
{
    SXE_RETURN result = SXE_RETURN_OK;

    SXEE6I("()");

    SXEA1I(sxe_ssl_array != NULL, "SSL: cannot call %s() before sxe_ssl_init()", __func__);
    SXEA1I(this->flags & SXE_FLAG_IS_STREAM, "SXE is not a stream: cannot enable SSL");

    this->flags |= SXE_FLAG_IS_SSL;

    SXER6("return %d // %s", result, sxe_return_to_string(result));
    return result;
}

static SXE_RETURN
setup_ssl_socket(SXE * this)
{
    SXE_RETURN   result  = SXE_RETURN_ERROR_INTERNAL;
    SXE_SSL    * ssl = NULL;
    unsigned     id;

    SXEE6I("()");
    MEM_SET_STATE(this, MEM_SSL_NEW);

    if ((id = sxe_pool_set_oldest_element_state(sxe_ssl_array, SXE_SSL_STATE_FREE, SXE_SSL_STATE_CONNECTED)) == SXE_POOL_NO_INDEX) {
        SXEL3I("setup_ssl_socket: Warning: ran out of SSL connections; SSL concurrency too high");
        result = SXE_RETURN_NO_UNUSED_ELEMENTS;
        goto SXE_EARLY_OUT;
    }

    ssl = &sxe_ssl_array[id];
    memset(ssl, 0, sizeof *ssl);

    this->ssl_id          = id;
    ssl->conn             = SSL_new(sxe_ssl_ctx);
    ssl->peer_certificate = NULL;
    result                = SXE_RETURN_OK;
    SXEA1I(ssl->conn != NULL,                              "Failed to create new SSL connection: %lu", ERR_get_error());
    SXEA1I(SSL_set_fd(ssl->conn, this->socket_as_fd) == 1, "Failed to set SSL file descriptor: %lu",   ERR_get_error());

SXE_EARLY_OUT:
    SXER6I("return %d // %s", result, sxe_return_to_string(result));
    return result;
}

static void
sxe_ssl_close_socket(SXE * this, SXE_SSL_STATE state)
{
    SXE_SSL    * ssl = NULL;

    SXEE6I("(state=%s)", sxe_ssl_state_to_string(state));
    MEM_SET_STATE(this, MEM_SSL_FREE);

    ssl = &sxe_ssl_array[this->ssl_id];
    SXEA1I(ssl->conn != NULL, "Attempt to free an already-free SSL socket");
    sxe_ssl_free(this, ssl, state);
    sxe_close(this);

    if (this->in_event_close) {
        (*this->in_event_close)(this);
    }

    SXER6("return");
}

static void sxe_ssl_io_cb_write(EV_P_ ev_io *io, int revents);
static void sxe_ssl_io_cb_read(EV_P_ ev_io *io, int revents);

static SXE_RETURN
sxe_ssl_handle_io_error(SXE * this, SXE_SSL *ssl, int ret, SXE_SSL_STATE state, SXE_SSL_STATE read_state,
                        SXE_SSL_STATE write_state, const char *func, const char *op)
{
    SXE_RETURN result = SXE_RETURN_ERROR_INTERNAL;
    char       errstr[1024];

    SXEE6I("(ret=%d,state=%s,read_state=%s,write_state=%s,func=%s,op=%s", ret, sxe_ssl_state_to_string(state),
            sxe_ssl_state_to_string(read_state), sxe_ssl_state_to_string(write_state), func, op);
    SXEA1I(this->ssl_id != SXE_POOL_NO_INDEX, "%s(): called on non-SSL or closed SSL socket", __func__);
    SXEA1I(ssl != NULL, "%s() called with NULL ssl pointer", __func__);
    MEM_SET_STATE(this, MEM_SSL_WTF);

    switch (SSL_get_error(ssl->conn, ret)) {
    case SSL_ERROR_WANT_READ:
        SXEL6I("%s(): %s() wants to read!", func, op);

        if (state != read_state) {
            sxe_pool_set_indexed_element_state(sxe_ssl_array, this->ssl_id, state, read_state);     /* Coverage exclusion: todo - get SSL_write() to return SSL_ERROR_WANT_READ */
        }

        sxe_private_set_watch_events(this, sxe_ssl_io_cb_read, EV_READ, 1);
        result = SXE_RETURN_IN_PROGRESS;
        break;

    case SSL_ERROR_WANT_WRITE:
        SXEL6I("%s(): %s() wants to write!", func, op);

        if (state != write_state) {
            sxe_pool_set_indexed_element_state(sxe_ssl_array, this->ssl_id, state, write_state);
        }

        sxe_private_set_watch_events(this, sxe_ssl_io_cb_write, EV_WRITE, 1);
        result = SXE_RETURN_IN_PROGRESS;
        break;

    case SSL_ERROR_ZERO_RETURN:
        /* The SSL transport has been closed cleanly. This does not
         * necessarily mean the underlying transport has been closed. Need to
         * do an sxe_close() here. */
        SXEL6I("%s(): SSL zero return (clean shutdown): closing connection", func);
        sxe_ssl_close_socket(this, state);
        result = SXE_RETURN_END_OF_FILE;
        break;

    case SSL_ERROR_SYSCALL:
        {
            long err = ERR_get_error();
            if (err == 0 && ret == 0) {
                SXEL6I("%s(): SSL received illegal EOF (underlying transport closed)", func);
            }
            else if (err == 0) {
                SXEL6I("%s(): SSL system call error: %s (%d)", func, strerror(errno), errno);
            }
            else {
                SXEL3I("%s(): %s(): SSL error %s", func, op, ERR_error_string(err, errstr));       /* Coverage exclusion: todo - get SSL functions to fail in system calls */
            }

            sxe_ssl_close_socket(this, state);
            result = SXE_RETURN_ERROR_WRITE_FAILED;
        }
        break;

    case SSL_ERROR_SSL:
        SXEL3I("%s(): %s() error: %s", func, op, ERR_error_string(ERR_get_error(), errstr));       /* Coverage exclusion: todo - get SSL function to return SSL_ERROR_SSL */
        sxe_ssl_close_socket(this, state);                                                         /* Coverage exclusion: todo - get SSL function to return SSL_ERROR_SSL */
        break;                                                                                     /* Coverage exclusion: todo - get SSL function to return SSL_ERROR_SSL */

    default:
        SXEL3I("%s(): %s(): returned unknown SSL error", func, op);                                /* Coverage exclusion: todo - get SSL functions to fail in other ways */
        sxe_ssl_close_socket(this, state);                                                         /* Coverage exclusion: todo - get SSL functions to fail in other ways */
        break;                                                                                     /* Coverage exclusion: todo - get SSL functions to fail in other ways */
    }

    SXER6I("return %u // %s", result, sxe_return_to_string(result))
    return result;
}

static SXE_RETURN
sxe_ssl_handshake(SXE * this)
{
    SXE_RETURN   result = SXE_RETURN_ERROR_INTERNAL;
    SXE_SSL    * ssl;
    int          ret;
    unsigned     state;

    SXEE6I("()");
    MEM_SET_STATE(this, MEM_SSL_HANDSHAKE);

    SXEA1I(this->ssl_id != SXE_POOL_NO_INDEX, "%s(): socket is not SSL", __func__);
    state   = sxe_pool_index_to_state(sxe_ssl_array, this->ssl_id);
    SXEA1I(state == SXE_SSL_STATE_CONNECTED, "%s: SSL is in unexpected state '%s'", __func__, sxe_ssl_state_to_string(state));
    ssl = &sxe_ssl_array[this->ssl_id];

    ERR_clear_error();
    ret = SSL_do_handshake(ssl->conn);

    if (ret == 1) {
        const char *cipher, *version;

        /* The TLS/SSL handshake was successfully completed, a TLS/SSL
         * connection has been established. */
        SXEL6I("SSL connection established!");
        sxe_pool_set_indexed_element_state(sxe_ssl_array, this->ssl_id, SXE_SSL_STATE_CONNECTED, SXE_SSL_STATE_ESTABLISHED);
        result = SXE_RETURN_OK;

        if (ssl->mode == SXE_SSL_MODE_CLIENT) {
            ssl->verified         = SSL_get_verify_result(ssl->conn) == X509_V_OK;
            ssl->peer_certificate = SSL_get_peer_certificate(ssl->conn);
        }

        if ((cipher = SSL_get_cipher(ssl->conn))) {
            snprintf(ssl->cipher, sizeof ssl->cipher, "%s", cipher);
        }

        ssl->bits = SSL_get_cipher_bits(ssl->conn, NULL);

        if ((version = SSL_get_version(ssl->conn))) {
            snprintf(ssl->version, sizeof ssl->version, "%s", version);
        }

        if (this->in_event_connected) {
            (*this->in_event_connected)(this);
        }

        goto SXE_EARLY_OUT;
    }

    result = sxe_ssl_handle_io_error(this, ssl, ret, state, state, state, __func__, "SSL_do_handshake");

SXE_EARLY_OUT:
    SXER6I("return %d // %s", result, sxe_return_to_string(result));
    return result;
}

void
sxe_ssl_get_info(SXE * this, int * verified, const char **cipher, int * bits, const char **version)
{
    SXE_SSL    * ssl;

    SXEE6I("()");
    SXEA1I(sxe_ssl_array != NULL, "SSL: cannot call %s() before sxe_ssl_init()", __func__);
    SXEA1I(this->ssl_id != SXE_POOL_NO_INDEX, "%s() called on non-SSL SXE", __func__);

    ssl = &sxe_ssl_array[this->ssl_id];

    if (verified) {
        *verified = ssl->verified;
    }

    if (cipher) {
        *cipher = ssl->cipher;
    }

    if (bits) {
        *bits = ssl->bits;
    }

    if (version) {
        *version = ssl->version;
    }

    SXER6I("return");
}

unsigned
sxe_ssl_get_peer_CN(SXE * this, char * buffer, unsigned buflen)
{
    SXE_SSL    * ssl;
    X509_NAME  * name;
    unsigned     length = 0;

    SXEE6I("()");
    SXEA1I(sxe_ssl_array != NULL, "SSL: cannot call %s() before sxe_ssl_init()", __func__);
    SXEA1I(this->ssl_id != SXE_POOL_NO_INDEX, "%s() called on non-SSL SXE", __func__);

    ssl = &sxe_ssl_array[this->ssl_id];

    if (!ssl->peer_certificate) {
        goto SXE_EARLY_OUT;
    }

    name = X509_get_subject_name(ssl->peer_certificate);
    length = X509_NAME_get_text_by_NID(name, NID_commonName, buffer, buflen);

SXE_EARLY_OUT:
    SXER6I("return %u // CN='%.*s'", length, length, buffer);
    return length;
}

unsigned
sxe_ssl_get_peer_issuer(SXE * this, char * buffer, unsigned buflen)
{
    SXE_SSL    * ssl;
    X509_NAME  * issuer;
    unsigned     length = 0;

    SXEE6I("()");
    SXEA1I(sxe_ssl_array != NULL, "SSL: cannot call %s() before sxe_ssl_init()", __func__);
    SXEA1I(this->ssl_id != SXE_POOL_NO_INDEX, "%s() called on non-SSL SXE", __func__);

    ssl = &sxe_ssl_array[this->ssl_id];

    if (!ssl->peer_certificate) {
        goto SXE_EARLY_OUT;
    }

    issuer = X509_get_issuer_name(ssl->peer_certificate);

    if (issuer != NULL) {
        if ((length = X509_NAME_get_text_by_NID(issuer, NID_commonName, buffer, buflen)) != 0) {
            goto SXE_EARLY_OUT;
        }
        if ((length = X509_NAME_get_text_by_NID(issuer, NID_organizationalUnitName, buffer, buflen)) != 0) { /* Coverage exclusion: no test certs without CN= issuers */
            goto SXE_EARLY_OUT;                                                                              /* Coverage exclusion: no test certs without CN= issuers */
        }
        if ((length = X509_NAME_get_text_by_NID(issuer, NID_organizationName, buffer, buflen)) != 0) {       /* Coverage exclusion: no test certs without CN= issuers */
            goto SXE_EARLY_OUT;                                                                              /* Coverage exclusion: no test certs without CN= issuers */
        }
    }

SXE_EARLY_OUT:
    SXER6I("return %u // issuer='%.*s'", length, length, buffer);
    return length;
}

SXE_RETURN
sxe_ssl_accept(SXE * this)
{
    SXE_RETURN   result;

    SXEE6I("()");
    MEM_SET_STATE(this, MEM_SSL_WTF);
    SXEA1I(sxe_ssl_array != NULL, "SSL: cannot call %s() before sxe_ssl_init()", __func__);

    /* Allow calling sxe_ssl_accept() by an application on a non-SSL socket */
    sxe_ssl_enable(this);

    if ((result = setup_ssl_socket(this)) != SXE_RETURN_OK) {
        goto SXE_EARLY_OUT;
    }

    sxe_ssl_array[this->ssl_id].mode = SXE_SSL_MODE_SERVER;
    SSL_set_accept_state(sxe_ssl_array[this->ssl_id].conn);
    result = sxe_ssl_handshake(this);

SXE_EARLY_OUT:
    SXER6I("return %d // %s", result, sxe_return_to_string(result));
    return result;
}

SXE_RETURN
sxe_ssl_connect(SXE * this)
{
    SXE_RETURN   result;

    SXEE6I("()");
    MEM_SET_STATE(this, MEM_SSL_WTF);
    SXEA1I(sxe_ssl_array != NULL, "SSL: cannot call %s() before sxe_ssl_init()", __func__);

    /* Allow calling sxe_ssl_accept() by an application on a non-SSL socket */
    sxe_ssl_enable(this);

    if ((result = setup_ssl_socket(this)) != SXE_RETURN_OK) {
        goto SXE_EARLY_OUT;
    }

    sxe_ssl_array[this->ssl_id].mode = SXE_SSL_MODE_CLIENT;
    SSL_set_connect_state(sxe_ssl_array[this->ssl_id].conn);
    result = sxe_ssl_handshake(this);

SXE_EARLY_OUT:
    SXER6I("return %d // %s", result, sxe_return_to_string(result));
    return result;
}

SXE_RETURN
sxe_ssl_close(SXE * this)
{
    SXE_RETURN    result = SXE_RETURN_OK;
    SXE_SSL     * ssl;
    SXE_SSL_STATE state;
    int           ret = -1;

    SXEE6I("()");
    MEM_SET_STATE(this, MEM_SSL_SHUTDOWN);

    SXEA1I(this->ssl_id != SXE_POOL_NO_INDEX, "sxe_ssl_close() called on non-SSL SXE");
    ssl = &sxe_ssl_array[this->ssl_id];
    state = sxe_pool_index_to_state(sxe_ssl_array, this->ssl_id);
    switch (state) {
    case SXE_SSL_STATE_ESTABLISHED:
    case SXE_SSL_STATE_CLOSING:
        ERR_clear_error();
        ret = SSL_shutdown(ssl->conn);
        break;
    default:
        SXEL3I("SSL shutdown: cannot shutdown in state %s; doing an unclean shutdown", sxe_ssl_state_to_string(state));
        ret = 0;
        break;
    }

    if (ret >= 0) {
        SXEL6I("SSL shut down");
        sxe_ssl_free(this, ssl, state);
        sxe_close(this);
        goto SXE_EARLY_OUT;
    }

    result = sxe_ssl_handle_io_error(this, ssl, ret, state, SXE_SSL_STATE_CLOSING, SXE_SSL_STATE_CLOSING, "sxe_ssl_close", "SSL_shutdown"); /* Coverage exclusion: todo - get SSL_shutdown() to fail */

SXE_EARLY_OUT:
    SXER6I("return %d // %s", result, sxe_return_to_string(result));
    return result;
}

SXE_RETURN
sxe_ssl_send_buffers(SXE * this)
{
    SXE_RETURN    result  = SXE_RETURN_ERROR_INTERNAL;
    SXE_SSL     * ssl;
    SXE_BUFFER  * buffer;
    SXE_SSL_STATE state;

    SXEE6I("() // SSL socket=%d", this->socket);

    SXEA1I(this->ssl_id != SXE_POOL_NO_INDEX, "sxe_ssl_send_buffers() called on non-SSL SXE");
    ssl   = &sxe_ssl_array[this->ssl_id];
    state = sxe_pool_index_to_state(sxe_ssl_array, this->ssl_id);
    SXEA1I(state == SXE_SSL_STATE_ESTABLISHED || state == SXE_SSL_STATE_WRITING,
            "sxe_ssl_send_buffers() in unexpected state %s", sxe_ssl_state_to_string(state));
    buffer = sxe_list_peek_head(&this->send_list);

    while (buffer != NULL) {
        const char * send_data   = sxe_buffer_get_data(buffer);
        unsigned     send_length = sxe_buffer_length(  buffer);
        int          bytes_written = 0;

        MEM_SET_STATE(this, MEM_SSL_WRITE);
        ERR_clear_error();

        if (send_length != 0) {
            bytes_written = SSL_write(ssl->conn, send_data, send_length);

            if (bytes_written <= 0) {
                result = sxe_ssl_handle_io_error(this, ssl, bytes_written, state, SXE_SSL_STATE_WRITING, SXE_SSL_STATE_WRITING,    /* Coverage exclusion: TODO - get SSL_write() to fail */
                                             "sxe_ssl_send_buffers", "SSL_write");
                goto SXE_EARLY_OUT;
            }
        }

        SXED7I(send_data, bytes_written);
        sxe_buffer_consume(buffer, bytes_written);

        if (sxe_buffer_length(buffer) == 0) {
            SXEL6I("All %u bytes written to SSL socket=%d; on to the next buffer", send_length, this->socket);
            sxe_list_shift(&this->send_list);
            buffer = sxe_list_peek_head(&this->send_list);
        }
        else {
            SXEL6I("Only %u of %u bytes written to SSL socket=%d", bytes_written, send_length, this->socket);
        }
    }

    if (state != SXE_SSL_STATE_ESTABLISHED) {
        sxe_pool_set_indexed_element_state(sxe_ssl_array, this->ssl_id, state, SXE_SSL_STATE_ESTABLISHED);
        sxe_private_set_watch_events(this, sxe_ssl_io_cb_read, EV_READ, 1);
    }

    /* We've written the last buffer: SXE_RETURN_OK */
    result = SXE_RETURN_OK;

SXE_EARLY_OUT:
    SXER6I("return %d // %s", result, sxe_return_to_string(result));
    return result;
}

static void
sxe_ssl_read(SXE * this)
{
    SXE_SSL     * ssl;
    int           nbytes;
    SXE_SSL_STATE state;

    SXEE6I("() // SSL socket=%d", this->socket);

    SXEA1I(this->ssl_id != SXE_POOL_NO_INDEX, "%s() called on non-SSL SXE", __func__);
    ssl = &sxe_ssl_array[this->ssl_id];
    state = sxe_pool_index_to_state(sxe_ssl_array, this->ssl_id);
    SXEA1I(state == SXE_SSL_STATE_ESTABLISHED || state == SXE_SSL_STATE_READING,
           "%s() in unexpected state %s", __func__, sxe_ssl_state_to_string(state));

    /* At some point while processing a read event, we might do something
     * which causes sxe_close() to be called, which will cause the ssl_id to
     * be reset. This will invalid the SSL->conn object, so we check it each
     * time we call. */
    while (this->ssl_id != SXE_POOL_NO_INDEX && !ssl->stopped) {
        MEM_SET_STATE(this, MEM_SSL_READ);
        ERR_clear_error();
        nbytes = SSL_read(ssl->conn, this->in_buf + this->in_total, sizeof(this->in_buf) - this->in_total);

        if (nbytes > 0) {
            sxe_private_handle_read_data(this, nbytes, NULL);
            continue;
        }

        sxe_ssl_handle_io_error(this, ssl, nbytes, state, SXE_SSL_STATE_ESTABLISHED, SXE_SSL_STATE_READING, __func__, "SSL_read");
        break;
    }

    SXER6I("return");
    return;
}

void
sxe_ssl_private_stop_reading_buffer_full(SXE * this)
{
    SXE_SSL * ssl;

    SXEE6I("()");
    SXEA1I(this->ssl_id != SXE_POOL_NO_INDEX, "%s() called on non-SSL SXE", __func__);
    ssl = &sxe_ssl_array[this->ssl_id];
    SXEA1I(ssl->stopped == 0, "%s() called when read events are already stopped", __func__);

    ssl->stopped = 1;

    SXER6I("return");
}

void
sxe_ssl_private_restart_reading_buffer_drained(SXE * this)
{
    SXE_SSL * ssl;

    SXEE6I("()");
    SXEA1I(this->ssl_id != SXE_POOL_NO_INDEX, "%s() called on non-SSL SXE", __func__);
    ssl = &sxe_ssl_array[this->ssl_id];
    SXEA1I(ssl->stopped != 0, "%s() called when read events are already stopped", __func__);

    ssl->stopped = 0;

    /* There may already be buffered data: try to drain it immediately */
    sxe_ssl_read(this);

    SXER6I("return");
}

static void
sxe_ssl_io_cb_read(EV_P_ ev_io *io, int revents)
{
    SXE_RETURN      result;
    SXE           * this  = (SXE *)io->data;
    SXE_SSL_STATE   state;

    SXE_UNUSED_PARAMETER(loop);
    SXE_UNUSED_PARAMETER(revents);

    SXEE6I("(revents=%u) // socket=%d", revents, this->socket);
    MEM_SET_STATE(this, MEM_SSL_WTF);

    SXEA1I(this->ssl_id != SXE_POOL_NO_INDEX, "sxe_ssl_io_cb_read() called on non-SSL SXE");
    state = sxe_pool_index_to_state(sxe_ssl_array, this->ssl_id);

    switch (state) {
    case SXE_SSL_STATE_CONNECTED:
        sxe_ssl_handshake(this);
        break;

    case SXE_SSL_STATE_ESTABLISHED:
    case SXE_SSL_STATE_READING:
        sxe_ssl_read(this);
        break;

    case SXE_SSL_STATE_WRITING:                                                                     /* Coverage exclusion: todo - make SSL_write() return SSL_ERROR_WANT_READ */
        result = sxe_ssl_send_buffers(this);                                                        /* Coverage exclusion: todo - make SSL_write() return SSL_ERROR_WANT_READ */

        if (result != SXE_RETURN_IN_PROGRESS) {                                                     /* Coverage exclusion: todo - make SSL_write() return SSL_ERROR_WANT_READ */
            if (this->out_event_written) {                                                          /* Coverage exclusion: todo - make SSL_write() return SSL_ERROR_WANT_READ */
                (*this->out_event_written)(this, result);                                           /* Coverage exclusion: todo - make SSL_write() return SSL_ERROR_WANT_READ */
            }
        }

        break;                                                                                      /* Coverage exclusion: todo - make SSL_write() return SSL_ERROR_WANT_READ */

    case SXE_SSL_STATE_CLOSING:                                                                     /* Coverage exclusion: todo - make SSL_shutdown() return SSL_ERROR_WANT_READ */
        sxe_ssl_close(this);                                                                        /* Coverage exclusion: todo - make SSL_shutdown() return SSL_ERROR_WANT_READ */
        break;                                                                                      /* Coverage exclusion: todo - make SSL_shutdown() return SSL_ERROR_WANT_READ */

    default:
        SXEA1I(0, "Unhandled sxe_ssl_io_cb_read() in state %s", sxe_ssl_state_to_string(state));    /* Coverage exclusion: can't happen without adding states. */
        break;
    }

    SXER6I("return");
}

static void
sxe_ssl_io_cb_write(EV_P_ ev_io *io, int revents)
{
    SXE_RETURN      result;
    SXE           * this  = (SXE *)io->data;
    SXE_SSL_STATE   state;

    SXE_UNUSED_PARAMETER(loop);
    SXE_UNUSED_PARAMETER(revents);

    SXEE6I("(revents=%u) // socket=%d", revents, this->socket);
    MEM_SET_STATE(this, MEM_SSL_WTF);

    SXEA1I(this->ssl_id != SXE_POOL_NO_INDEX, "sxe_ssl_io_cb_write() called on non-SSL SXE");
    state = sxe_pool_index_to_state(sxe_ssl_array, this->ssl_id);

    switch (state) {
    case SXE_SSL_STATE_CONNECTED:                                                                   /* Coverage exclusion: todo - make SSL_do_handshake() return SSL_ERROR_WANT_WRITE */
        sxe_ssl_handshake(this);                                                                    /* Coverage exclusion: todo - make SSL_do_handshake() return SSL_ERROR_WANT_WRITE */
        break;                                                                                      /* Coverage exclusion: todo - make SSL_do_handshake() return SSL_ERROR_WANT_WRITE */

    case SXE_SSL_STATE_READING:                                                                     /* Coverage exclusion: todo - make SSL_read() return SSL_ERROR_WANT_WRITE */
        sxe_ssl_read(this);                                                                         /* Coverage exclusion: todo - make SSL_read() return SSL_ERROR_WANT_WRITE */
        break;                                                                                      /* Coverage exclusion: todo - make SSL_read() return SSL_ERROR_WANT_WRITE */

    case SXE_SSL_STATE_WRITING:
        result = sxe_ssl_send_buffers(this);

        if (result != SXE_RETURN_IN_PROGRESS) {
            if (this->out_event_written) {
                (*this->out_event_written)(this, result);                                           /* Coverage exclusion: TODO - SSL coverage sucks */
            }
        }

        break;

    case SXE_SSL_STATE_CLOSING:                                                                     /* Coverage exclusion: todo - make SSL_shutdown() return SSL_ERROR_WANT_WRITE */
        sxe_ssl_close(this);                                                                        /* Coverage exclusion: todo - make SSL_shutdown() return SSL_ERROR_WANT_WRITE */
        break;                                                                                      /* Coverage exclusion: todo - make SSL_shutdown() return SSL_ERROR_WANT_WRITE */

    default:
        SXEA1I(0, "Unhandled sxe_ssl_io_cb_write() in state %s", sxe_ssl_state_to_string(state));   /* Coverage exclusion: can't happen without adding states. */
        break;
    }

    SXER6("return");
}

#endif /* ! SXE_DISABLE_OPENSSL */

/* vim: set expandtab list sw=4 sts=4 listchars=tab\:^.,trail\:@: */
