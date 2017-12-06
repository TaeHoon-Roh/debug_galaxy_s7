//
// Created by root on 17. 12. 5.
//

#ifndef KOREAPASSING_USB_CORE_H_H
#define KOREAPASSING_USB_CORE_H_H
#include "usb_device.h"
#include "usbi_backend.h"


//usb_core
int libusb_has_capability(uint32_t capability);


//usb_hotplug
void usbi_hotplug_deregister_all(struct libusb_context *ctx);

//usb_io
int libusb_get_next_timeout(libusb_context *ctx, struct timeval *tv);
static int get_next_timeout(libusb_context *ctx, struct timeval *tv,struct timeval *out);
int libusb_handle_events_timeout_completed(libusb_context *ctx, struct timeval *tv, int *completed);
int libusb_handle_events_timeout(libusb_context *ctx, struct timeval *tv);
void usbi_io_exit(struct libusb_context *ctx);

#endif //KOREAPASSING_USB_CORE_H_H
