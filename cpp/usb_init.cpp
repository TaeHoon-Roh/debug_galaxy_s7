//
// Created by root on 17. 11. 20.
//

#include "native-lib.h"
#include "usbi_backend.h"
#include "thread.h"
#include <unistd.h>

static usbi_mutex_static_t default_context_lock = USBI_MUTEX_INITIALIZER;
static struct timeval timestamp_origin = {0, 0};
struct libusb_context *usbi_default_context = NULL;
static int default_context_refcnt = 0;

#define usbi_pipe pipe

struct libusb_pollfd {
    int fd;
    short events;
};

struct usbi_pollfd {
    struct libusb_pollfd pollfd;
    struct list_head list;
};

static inline void list_add_tail(struct list_head *entry,
                                 struct list_head *head)
{
    entry->next = head;
    entry->prev = head->prev;

    head->prev->next = entry;
    head->prev = entry;
}

int usbi_add_pollfd(struct libusb_context *ctx, int fd, short events)
{
    struct usbi_pollfd *ipollfd = (usbi_pollfd *) malloc(sizeof(*ipollfd));
    if (!ipollfd)
        return LIBUSB_ERROR_NO_MEM;

    ipollfd->pollfd.fd = fd;
    ipollfd->pollfd.events = events;

    //first Error
    //usbi_mutex_lock(&ctx->pollfds_lock);
    list_add_tail(&ipollfd->list, (list_head *) &ctx->pollfds);
    //usbi_mutex_unlock(&ctx->pollfds_lock);

    ctx->fd_added_cb.fd = fd;
    ctx->fd_added_cb.events = events;

    return 0;
}



int usbi_mutex_init_recursive(pthread_mutex_t *mutex, pthread_mutexattr_t *attr)
{
    int err;
    pthread_mutexattr_t stack_attr;
    if (!attr) {
        attr = &stack_attr;
        err = pthread_mutexattr_init(&stack_attr);
        if (err != 0)
            return err;
    }

    err = pthread_mutexattr_settype(attr, PTHREAD_MUTEX_RECURSIVE);
    if (err != 0)
        goto finish;

    err = pthread_mutex_init(mutex, attr);

    finish:
    if (attr == &stack_attr)
        pthread_mutexattr_destroy(&stack_attr);

    return err;
}

static inline void list_init(struct list_head *entry)
{
    entry->prev = entry->next = entry;
}

int usbi_io_init(struct libusb_context *ctx)
{
    int r;

    usbi_mutex_init(&ctx->flying_transfers_lock, NULL);
    usbi_mutex_init(&ctx->pollfds_lock, NULL);
    usbi_mutex_init(&ctx->pollfd_modify_lock, NULL);
    usbi_mutex_init_recursive(&ctx->events_lock, NULL);
    usbi_mutex_init(&ctx->event_waiters_lock, NULL);
    usbi_cond_init(&ctx->event_waiters_cond, NULL);
    list_init(&ctx->flying_transfers);
    list_init((list_head *) &ctx->pollfds);

    /* FIXME should use an eventfd on kernels that support it */
    r = usbi_pipe(ctx->ctrl_pipe);
    if (r < 0) {
        r = LIBUSB_ERROR_OTHER;
        goto err;
    }

    r = usbi_add_pollfd(ctx, ctx->ctrl_pipe[0], POLLIN);
    if (r < 0)
        goto err_close_pipe;

#ifdef USBI_TIMERFD_AVAILABLE
    ctx->timerfd = timerfd_create(usbi_backend->get_timerfd_clockid(),
		TFD_NONBLOCK);
	if (ctx->timerfd >= 0) {
		usbi_dbg("using timerfd for timeouts");
		r = usbi_add_pollfd(ctx, ctx->timerfd, POLLIN);
		if (r < 0) {
			usbi_remove_pollfd(ctx, ctx->ctrl_pipe[0]);
			close(ctx->timerfd);
			goto err_close_pipe;
		}
	} else {
		usbi_dbg("timerfd not available (code %d error %d)", ctx->timerfd, errno);
		ctx->timerfd = -1;
	}
#endif

    return 0;

    err_close_pipe:
    //Error
    err:
    usbi_mutex_destroy(&ctx->flying_transfers_lock);
    usbi_mutex_destroy(&ctx->pollfds_lock);
    usbi_mutex_destroy(&ctx->pollfd_modify_lock);
    usbi_mutex_destroy(&ctx->events_lock);
    usbi_mutex_destroy(&ctx->event_waiters_lock);
    usbi_cond_destroy(&ctx->event_waiters_cond);
    return r;
}

int libusb_init(struct libusb_context **context) {
    struct libusb_context *ctx;
    int r = 0;

    usbi_mutex_static_lock(&default_context_lock);
    if (!context && usbi_default_context) {
        default_context_refcnt++;
        usbi_mutex_static_unlock(&default_context_lock);
        return 0;
    }


    ctx = (libusb_context *) malloc(sizeof(*ctx));
    if (!ctx) {
        r = -11;
        goto err_unlock;
    }
    memset(ctx, 0, sizeof(*ctx));

    r = op_init(ctx);
    if (r)
        goto err_free_ctx;

    usbi_mutex_init(&ctx->usb_devs_lock, NULL);
    usbi_mutex_init(&ctx->open_devs_lock, NULL);
    list_init(&ctx->usb_devs);
    list_init(&ctx->open_devs);

    r = usbi_io_init(ctx);
    if (r < 0) {
        op_exit();
        goto err_destroy_mutex;
    }

    if (context) {
        *context = ctx;
    } else if (!usbi_default_context) {
        usbi_default_context = ctx;
        default_context_refcnt++;
    }
    usbi_mutex_static_unlock(&default_context_lock);

    return 0;

    err_destroy_mutex:
    usbi_mutex_destroy(&ctx->open_devs_lock);
    usbi_mutex_destroy(&ctx->usb_devs_lock);
    err_free_ctx:
    free(ctx);
    err_unlock:
    usbi_mutex_static_unlock(&default_context_lock);
    return r;
}
