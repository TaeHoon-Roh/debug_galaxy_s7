//
// Created by root on 17. 12. 5.
//
#include "native-lib.h"
#include "usb_device.h"
#include "thread.h"
#include "usbi_backend.h"
#include "usb_core.h"

void libusb_exit(struct libusb_context *ctx) {

    struct libusb_device *dev, *next;
    struct timeval tv = { 0, 0 };

    USBI_GET_CONTEXT(ctx);

    /* if working with default context, only actually do the deinitialization
     * if we're the last user */
    usbi_mutex_static_lock(&default_context_lock);
    if (ctx == usbi_default_context) {
        if (--default_context_refcnt > 0) {
            usbi_mutex_static_unlock(&default_context_lock);
            return;
        }
        usbi_default_context = NULL;
    }
    usbi_mutex_static_unlock(&default_context_lock);

    usbi_mutex_static_lock(&active_contexts_lock);
    list_del (&ctx->list);
    usbi_mutex_static_unlock(&active_contexts_lock);

    if (libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
        usbi_hotplug_deregister_all(ctx);

        if (list_empty(&ctx->open_devs))
            libusb_handle_events_timeout(ctx, &tv);

        usbi_mutex_lock(&ctx->usb_devs_lock);
        list_for_each_entry_safe(dev, next, &ctx->usb_devs, list, struct libusb_device) {
            list_del(&dev->list);
            libusb_unref_device(dev);
        }
        usbi_mutex_unlock(&ctx->usb_devs_lock);
    }

    if (!list_empty(&ctx->usb_devs))

    if (!list_empty(&ctx->open_devs))

    usbi_io_exit(ctx);
    op_exit();
    usbi_mutex_destroy(&ctx->open_devs_lock);
    usbi_mutex_destroy(&ctx->usb_devs_lock);
    usbi_mutex_destroy(&ctx->hotplug_cbs_lock);
    free(ctx);
}