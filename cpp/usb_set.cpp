//
// Created by root on 17. 11. 22.
//
#include "native-lib.h"
#include "usb_device.h"
#include "usbi_backend.h"

uint32_t caps_backend = 0;

int libusb_claim_interface(libusb_device_handle *dev, int interface_number) {

    int r = 0;
    if (interface_number >= USB_MAXINTERFACES)
        return LIBUSB_ERROR_INVALID_PARAM;

    usbi_mutex_lock(&dev->lock);
    if (dev->claimed_interfaces & (1 << interface_number))
        goto out;

    r = op_claim_interface(dev, interface_number);
    if (r == 0)
        dev->claimed_interfaces |= 1 << interface_number;

    out:
    usbi_mutex_unlock(&dev->lock);
    return r;
}


int libusb_set_auto_detach_kernel_driver(libusb_device_handle *dev_handle, int enable) {

    if (!(caps_backend & USBI_CAP_SUPPORTS_DETACH_KERNEL_DRIVER))
        return LIBUSB_ERROR_NOT_SUPPORTED;

    dev_handle->auto_detach_kernel_driver = enable;
    return LIBUSB_SUCCESS;
}

libusb_device *libusb_ref_device(libusb_device *dev) {

    usbi_mutex_lock(&dev->lock);
    dev->refcnt++;
    usbi_mutex_unlock(&dev->lock);
    return dev;
}

int libusb_set_option(libusb_context *ctx, enum libusb_option option, ...) {
    int arg;
    int r = LIBUSB_SUCCESS;
    va_list ap;

    USBI_GET_CONTEXT(ctx);

    va_start(ap, option);
    switch (option) {
        case LIBUSB_OPTION_LOG_LEVEL:
            arg = va_arg(ap, int);
            if (arg < LIBUSB_LOG_LEVEL_NONE || arg > LIBUSB_LOG_LEVEL_DEBUG) {
                r = LIBUSB_ERROR_INVALID_PARAM;
                break;
            }
#if defined(ENABLE_LOGGING) && !defined(ENABLE_DEBUG_LOGGING)
        if (!ctx->debug_fixed)
            ctx->debug = (enum libusb_log_level)arg;
#endif
            break;
#if 0
        case LIBUSB_OPTION_<...>:
        if (usbi_backend.set_option)
            r = usbi_backend.set_option(ctx, option, ap);
        else
            r = LIBUSB_ERROR_NOT_SUPPORTED;
        break;
#endif
        default:
            r = LIBUSB_ERROR_INVALID_PARAM;
    }

    va_end(ap);
    return r;


}