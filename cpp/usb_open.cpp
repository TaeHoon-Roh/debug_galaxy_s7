//
// Created by root on 17. 11. 22.
//

#include "native-lib.h"
#include "usb_device.h"
#include "usbi_backend.h"


static struct discovered_devs *discovered_devs_alloc(void) {

    struct discovered_devs *ret = (discovered_devs *) malloc(
            sizeof(*ret) + (sizeof(void *) * DISCOVERED_DEVICES_SIZE_STEP));

    if (ret) {
        ret->len = 0;
        ret->capacity = DISCOVERED_DEVICES_SIZE_STEP;
    }
    return ret;
}

void discovered_devs_free(discovered_devs *discdevs) {

    size_t i;

    for (i = 0; i < discdevs->len; i++)
        libusb_unref_device(discdevs->devices[i]);

    free(discdevs);
}


ssize_t libusb_get_device_list(libusb_context *ctx, libusb_device ***list) {

    struct discovered_devs *discdevs = discovered_devs_alloc();
    struct libusb_device **ret;
    int r = 0;
    ssize_t i, len;
    USBI_GET_CONTEXT(ctx);

    if (!discdevs)
        return LIBUSB_ERROR_NO_MEM;

    r = op_get_device_list(ctx, &discdevs);
    if (r < 0) {
        len = r;
        goto out;
    }

    /* convert discovered_devs into a list */
    len = discdevs->len;
    ret = (libusb_device **) malloc(sizeof(void *) * (len + 1));
    if (!ret) {
        len = LIBUSB_ERROR_NO_MEM;
        goto out;
    }

    ret[len] = NULL;
    for (i = 0; i < len; i++) {
        struct libusb_device *dev = discdevs->devices[i];
        ret[i] = libusb_ref_device(dev);
    }
    *list = ret;

    out:
    discovered_devs_free(discdevs);
    return len;
}

int libusb_get_device_descriptor(libusb_device *dev, libusb_device_descriptor *desc) {

    unsigned char raw_desc[DEVICE_DESC_LENGTH];
    int host_endian = 0;
    int r;

    //LOGD("usb_open : libusb_get_device_descriptor - ");
    r = op_get_device_descriptor(dev, raw_desc, &host_endian);
    if (r < 0)
        return r;

    memcpy((unsigned char *) desc, raw_desc, sizeof(raw_desc));
    if (!host_endian) {
        desc->bcdUSB = libusb_le16_to_cpu(desc->bcdUSB);
        desc->idVendor = libusb_le16_to_cpu(desc->idVendor);
        desc->idProduct = libusb_le16_to_cpu(desc->idProduct);
        desc->bcdDevice = libusb_le16_to_cpu(desc->bcdDevice);
    }
    return 0;
}

int libusb_open(libusb_device *dev, libusb_device_handle **handle){

    struct libusb_context *ctx = DEVICE_CTX(dev);
    struct libusb_device_handle *_handle;
    size_t priv_size = device_handle_priv_size();
    int r;
    ////LOGD("usb_open : libusb_open - open %d.%d", dev->bus_number, dev->device_address);

    _handle = (libusb_device_handle *) malloc(sizeof(*_handle) + priv_size);
    if (!_handle)
        return LIBUSB_ERROR_NO_MEM;

    r = usbi_mutex_init(&_handle->lock, NULL);
    if (r) {
        free(_handle);
        return LIBUSB_ERROR_OTHER;
    }

    _handle->dev = libusb_ref_device(dev);
    _handle->claimed_interfaces = 0;
    memset(&_handle->os_priv, 0, priv_size);

    //error check111 2017-11-24 - need debug
    r = op_open(_handle);
    if (r < 0) {
        //LOGD("usb_open : libusb_open - open %d.%d returns %d", dev->bus_number, dev->device_address, r);
        libusb_unref_device(dev);
        usbi_mutex_destroy(&_handle->lock);
        free(_handle);
        return r;
    }

    usbi_mutex_lock(&ctx->open_devs_lock);
    list_add(&_handle->list, &ctx->open_devs);
    usbi_mutex_unlock(&ctx->open_devs_lock);
    *handle = _handle;

    //error check222 2017-11-24 - need debug
    usbi_fd_notification(ctx);

    return 0;
}

void libusb_free_device_list(libusb_device **list, int unref_devices) {

    if (!list)
        return;

    if (unref_devices) {
        int i = 0;
        struct libusb_device *dev;

        while ((dev = list[i++]) != NULL)
            libusb_unref_device(dev);
    }
    free(list);
}

libusb_device_handle * libusb_open_device_with_vid_pid(libusb_context *ctx, uint16_t vendor_id, uint16_t product_id) {

    struct libusb_device **devs;
    struct libusb_device *found = NULL;
    struct libusb_device *dev;
    struct libusb_device_handle *handle = NULL;
    size_t i = 0;
    int r;

    if (libusb_get_device_list(ctx, &devs) < 0)
        return NULL;

    while ((dev = devs[i++]) != NULL) {
        struct libusb_device_descriptor desc;
        r = libusb_get_device_descriptor(dev, &desc);
        if (r < 0)
            goto out;
        if (desc.idVendor == vendor_id && desc.idProduct == product_id) {
            found = dev;
            break;
        }
    }

    if (found) {
        r = libusb_open(found, &handle);
        if (r < 0)
            handle = NULL;
    }

    out:
    //error check333 2017-11-24 -need debug
    libusb_free_device_list(devs, 1);
    return handle;
}