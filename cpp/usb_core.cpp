//
// Created by root on 17. 12. 5.
//
#include "usb_core.h"

int libusb_has_capability(uint32_t capability) {
    switch (capability) {
        case LIBUSB_CAP_HAS_CAPABILITY:
            return 1;
        case LIBUSB_CAP_HAS_HOTPLUG:
            return !(get_device_list());
        case LIBUSB_CAP_HAS_HID_ACCESS:
            return (caps_backend & USBI_CAP_HAS_HID_ACCESS);
        case LIBUSB_CAP_SUPPORTS_DETACH_KERNEL_DRIVER:
            return (caps_backend & USBI_CAP_SUPPORTS_DETACH_KERNEL_DRIVER);
    }
    return 0;
}

void usbi_hotplug_deregister_all(struct libusb_context *ctx) {
    struct libusb_hotplug_callback *hotplug_cb, *next;

    usbi_mutex_lock(&ctx->hotplug_cbs_lock);
    list_for_each_entry_safe(hotplug_cb, next, &ctx->hotplug_cbs, list,
                             struct libusb_hotplug_callback) {
        list_del(&hotplug_cb->list);
        free(hotplug_cb);
    }

    usbi_mutex_unlock(&ctx->hotplug_cbs_lock);
}

int libusb_get_next_timeout(libusb_context *ctx, struct timeval *tv) {

    struct usbi_transfer *transfer;
    struct timespec cur_ts;
    struct timeval cur_tv;
    struct timeval next_timeout = {0, 0};
    int r;

    USBI_GET_CONTEXT(ctx);
    if (usbi_using_timerfd(ctx))
        return 0;

    usbi_mutex_lock(&ctx->flying_transfers_lock);
    if (list_empty(&ctx->flying_transfers)) {
        usbi_mutex_unlock(&ctx->flying_transfers_lock);
        return 0;
    }

    list_for_each_entry(transfer, &ctx->flying_transfers, list, struct usbi_transfer) {
        if (transfer->timeout_flags &
            (USBI_TRANSFER_TIMEOUT_HANDLED | USBI_TRANSFER_OS_HANDLES_TIMEOUT))
            continue;

        if (!timerisset(&transfer->timeout))
            break;

        next_timeout = transfer->timeout;
        break;
    }
    usbi_mutex_unlock(&ctx->flying_transfers_lock);

    if (!timerisset(&next_timeout)) {
        return 0;
    }

    r = op_clock_gettime(USBI_CLOCK_MONOTONIC, &cur_ts);
    if (r < 0) {
        return 0;
    }
    TIMESPEC_TO_TIMEVAL(&cur_tv, &cur_ts);

    if (!timercmp(&cur_tv, &next_timeout, <)) {
        timerclear(tv);
    } else {
        timersub(&next_timeout, &cur_tv, tv);
    }

    return 1;
}

static int get_next_timeout(libusb_context *ctx, struct timeval *tv, struct timeval *out) {

    struct timeval timeout;
    int r = libusb_get_next_timeout(ctx, &timeout);
    if (r) {
        /* timeout already expired? */
        if (!timerisset(&timeout))
            return 1;

        /* choose the smallest of next URB timeout or user specified timeout */
        if (timercmp(&timeout, tv, <))
            *out = timeout;
        else
            *out = *tv;
    } else {
        *out = *tv;
    }
    return 0;
}

int
libusb_handle_events_timeout_completed(libusb_context *ctx, struct timeval *tv, int *completed) {

    int r;
    struct timeval poll_timeout;

    USBI_GET_CONTEXT(ctx);
    r = get_next_timeout(ctx, tv, &poll_timeout);
    if (r) {
        return handle_timeouts(ctx);
    }

    retry:
    if (libusb_try_lock_events(ctx) == 0) {
        if (completed == NULL || !*completed) {
            r = handle_events(ctx, &poll_timeout);
        }
        libusb_unlock_events(ctx);
        return r;
    }

    libusb_lock_event_waiters(ctx);

    if (completed && *completed)
        goto already_done;

    if (!libusb_event_handler_active(ctx)) {
        libusb_unlock_event_waiters(ctx);
        goto retry;
    }

    r = libusb_wait_for_event(ctx, &poll_timeout);

    already_done:
    libusb_unlock_event_waiters(ctx);

    if (r < 0)
        return r;
    else if (r == 1)
        return handle_timeouts(ctx);
    else
        return 0;
}

int libusb_handle_events_timeout(libusb_context *ctx, struct timeval *tv) {

    return libusb_handle_events_timeout_completed(ctx, tv, NULL);

}

void usbi_io_exit(struct libusb_context *ctx) {

    usbi_remove_pollfd(ctx, ctx->ctrl_pipe[0]);
#ifdef USBI_TIMERFD_AVAILABLE
    if (usbi_using_timerfd(ctx)) {
		usbi_remove_pollfd(ctx, ctx->timerfd);
		close(ctx->timerfd);
	}
#endif
    usbi_mutex_destroy(&ctx->flying_transfers_lock);
    usbi_mutex_destroy(&ctx->events_lock);
    usbi_mutex_destroy(&ctx->event_waiters_lock);
    usbi_cond_destroy(&ctx->event_waiters_cond);
    usbi_mutex_destroy(&ctx->event_data_lock);
    usbi_tls_key_delete(ctx->event_handling_key);
    if (ctx->pollfds)
        free(ctx->pollfds);
}
