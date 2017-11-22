//
// Created by root on 17. 11. 22.
//

#include "usb_device.h"

int libusb_claim_interface(libusb_device_handle *dev, int interface_number){
    int r = 0;

    if (interface_number >= USB_MAXINTERFACES)
        return LIBUSB_ERROR_INVALID_PARAM;

    usbi_mutex_lock(&dev->lock);
    if (dev->claimed_interfaces & (1 << interface_number))
        goto out;

    r = usbi_backend.claim_interface(dev, interface_number);
    if (r == 0)
        dev->claimed_interfaces |= 1 << interface_number;

    out:
    usbi_mutex_unlock(&dev->lock);
    return r;
}