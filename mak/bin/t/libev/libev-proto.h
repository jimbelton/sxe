/*
 * Generated by genxface.pl - DO NOT EDIT OR COMMIT TO SOURCE CODE CONTROL!
 */
#ifdef __cplusplus
extern "C" {
#endif

EV_INLINE struct ev_loop * ev_default_loop_uc (void) ;
EV_INLINE struct ev_loop * ev_default_loop (unsigned int flags) ;
EV_INLINE ev_tstamp ev_now (void) ;
EV_INLINE int ev_is_default_loop (EV_P) ;
void ev_set_syserr_cb (void (*cb)(const char *msg)) ;
void ev_set_allocator (void *(*cb)(void *ptr, long size)) ;
inline_speed void * ev_realloc (void *ptr, long size) ;
ev_tstamp ev_time (void) ;
ev_tstamp inline_size get_clock (void) ;
void ev_sleep (ev_tstamp delay) ;
int inline_size array_nextsize (int elem, int cur, int cnt) ;
void noinline ev_feed_event (EV_P_ void *w, int revents) ;
void inline_speed queue_events (EV_P_ W *events, int eventcnt, int type) ;
void inline_speed fd_event (EV_P_ int fd, int revents) ;
void ev_feed_fd_event (EV_P_ int fd, int revents) ;
void inline_size fd_reify (EV_P) ;
void inline_size fd_change (EV_P_ int fd, int flags) ;
void inline_speed fd_kill (EV_P_ int fd) ;
int inline_size fd_valid (int fd) ;
void inline_speed downheap (ANHE *heap, int N, int k) ;
void inline_speed upheap (ANHE *heap, int k) ;
void inline_size adjustheap (ANHE *heap, int N, int k) ;
void inline_size reheap (ANHE *heap, int N) ;
void inline_speed fd_intern (int fd) ;
void inline_size evpipe_write (EV_P_ EV_ATOMIC_T *flag) ;
void noinline ev_feed_signal_event (EV_P_ int signum) ;
void inline_speed child_reap (EV_P_ int chain, int pid, int status) ;
int ev_version_major (void) ;
int ev_version_minor (void) ;
int inline_size enable_secure (void) ;
unsigned int ev_supported_backends (void) ;
unsigned int ev_recommended_backends (void) ;
unsigned int ev_embeddable_backends (void) ;
unsigned int ev_backend (EV_P) ;
unsigned int ev_loop_count (EV_P) ;
void ev_set_io_collect_interval (EV_P_ ev_tstamp interval) ;
void ev_set_timeout_collect_interval (EV_P_ ev_tstamp interval) ;
void inline_size loop_fork (EV_P) ;
struct ev_loop * ev_loop_new (unsigned int flags) ;
void ev_loop_destroy (EV_P) ;
void ev_loop_fork (EV_P) ;
void ev_loop_verify (EV_P) ;
void ev_default_destroy (void) ;
void ev_default_fork (void) ;
void ev_invoke (EV_P_ void *w, int revents) ;
void inline_speed call_pending (EV_P) ;
void inline_size idle_reify (EV_P) ;
void inline_size timers_reify (EV_P) ;
void inline_size periodics_reify (EV_P) ;
void inline_speed time_update (EV_P_ ev_tstamp max_block) ;
void ev_ref (EV_P) ;
void ev_unref (EV_P) ;
void ev_now_update (EV_P) ;
void ev_loop (EV_P_ int flags) ;
void ev_unloop (EV_P_ int how) ;
void inline_size wlist_add (WL *head, WL elem) ;
void inline_size wlist_del (WL *head, WL elem) ;
void inline_speed clear_pending (EV_P_ W w) ;
int ev_clear_pending (EV_P_ void *w) ;
void inline_size pri_adjust (EV_P_ W w) ;
void inline_speed ev_start (EV_P_ W w, int active) ;
void inline_size ev_stop (EV_P_ W w) ;
void noinline ev_io_start (EV_P_ ev_io *w) ;
void noinline ev_io_stop (EV_P_ ev_io *w) ;
void noinline ev_timer_start (EV_P_ ev_timer *w) ;
void noinline ev_timer_stop (EV_P_ ev_timer *w) ;
void noinline ev_timer_again (EV_P_ ev_timer *w) ;
void noinline ev_periodic_start (EV_P_ ev_periodic *w) ;
void noinline ev_periodic_stop (EV_P_ ev_periodic *w) ;
void noinline ev_periodic_again (EV_P_ ev_periodic *w) ;
void noinline ev_signal_start (EV_P_ ev_signal *w) ;
void noinline ev_signal_stop (EV_P_ ev_signal *w) ;
void ev_child_start (EV_P_ ev_child *w) ;
void ev_child_stop (EV_P_ ev_child *w) ;
void inline_size check_2625 (EV_P) ;
void inline_size infy_init (EV_P) ;
void inline_size infy_fork (EV_P) ;
void ev_stat_stat (EV_P_ ev_stat *w) ;
void ev_stat_start (EV_P_ ev_stat *w) ;
void ev_stat_stop (EV_P_ ev_stat *w) ;
void ev_idle_start (EV_P_ ev_idle *w) ;
void ev_idle_stop (EV_P_ ev_idle *w) ;
void ev_prepare_start (EV_P_ ev_prepare *w) ;
void ev_prepare_stop (EV_P_ ev_prepare *w) ;
void ev_check_start (EV_P_ ev_check *w) ;
void ev_check_stop (EV_P_ ev_check *w) ;
void noinline ev_embed_sweep (EV_P_ ev_embed *w) ;
void ev_embed_start (EV_P_ ev_embed *w) ;
void ev_embed_stop (EV_P_ ev_embed *w) ;
void ev_fork_start (EV_P_ ev_fork *w) ;
void ev_fork_stop (EV_P_ ev_fork *w) ;
void ev_async_start (EV_P_ ev_async *w) ;
void ev_async_stop (EV_P_ ev_async *w) ;
void ev_async_send (EV_P_ ev_async *w) ;
void ev_once (EV_P_ int fd, int events, ev_tstamp timeout, void (*cb)(int revents, void *arg), void *arg) ;
int inline_size epoll_init (EV_P_ int flags) ;
void inline_size epoll_destroy (EV_P) ;
void inline_size epoll_fork (EV_P) ;
void inline_speed kqueue_change (EV_P_ int fd, int filter, int flags, int fflags) ;
int inline_size kqueue_init (EV_P_ int flags) ;
void inline_size kqueue_destroy (EV_P) ;
void inline_size kqueue_fork (EV_P) ;
void inline_size pollidx_init (int *base, int count) ;
int inline_size poll_init (EV_P_ int flags) ;
void inline_size poll_destroy (EV_P) ;
void inline_speed port_associate_and_check (EV_P_ int fd, int ev) ;
int inline_size port_init (EV_P_ int flags) ;
void inline_size port_destroy (EV_P) ;
void inline_size port_fork (EV_P) ;
int inline_size select_init (EV_P_ int flags) ;
void inline_size select_destroy (EV_P) ;
const char *event_get_version (void) ;
const char *event_get_method (void) ;
void *event_init (void) ;
void event_base_free (struct event_base *base) ;
int event_dispatch (void) ;
void event_set_log_callback (event_log_cb cb) ;
int event_loop (int flags) ;
int event_loopexit (struct timeval *tv) ;
void event_set (struct event *ev, int fd, short events, void (*cb)(int, short, void *), void *arg) ;
int event_once (int fd, short events, void (*cb)(int, short, void *), void *arg, struct timeval *tv) ;
int event_add (struct event *ev, struct timeval *tv) ;
int event_del (struct event *ev) ;
void event_active (struct event *ev, int res, short ncalls) ;
int event_pending (struct event *ev, short events, struct timeval *tv) ;
int event_priority_init (int npri) ;
int event_priority_set (struct event *ev, int pri) ;
int event_base_set (struct event_base *base, struct event *ev) ;
int event_base_loop (struct event_base *base, int flags) ;
int event_base_dispatch (struct event_base *base) ;
int event_base_loopexit (struct event_base *base, struct timeval *tv) ;
int event_base_once (struct event_base *base, int fd, short events, void (*cb)(int, short, void *), void *arg, struct timeval *tv) ;
int event_base_priority_init (struct event_base *base, int npri) ;

#ifdef __cplusplus
}
#endif
