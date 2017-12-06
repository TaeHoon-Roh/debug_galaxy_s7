//
// Created by root on 17. 11. 24.
// This header file include usb_init, usb_set, usb_open, usb_exit
//

#ifndef KOREAPASSING_USB_INIT_H
#define KOREAPASSING_USB_INIT_H

#include "native-lib.h"
#include "usbi_backend.h"
#include "thread.h"
#include "usb_device.h"
#include <unistd.h>

static usbi_mutex_static_t default_context_lock = USBI_MUTEX_INITIALIZER;
static struct timeval timestamp_origin = {0, 0};
static int default_context_refcnt = 0;

#define usbi_pipe pipe


inline void list_init(list_head *entry) {

    entry->prev = entry->next = entry;

}

//usb_init method
int usbi_io_init(libusb_context *ctx);
int libusb_init(libusb_context **context);
int usbi_add_pollfd(libusb_context *ctx, int fd, short events);
void usbi_remove_pollfd(struct libusb_context *ctx, int fd);
int usbi_mutex_init_recursive(pthread_mutex_t *mutex, pthread_mutexattr_t *attr);

//usb_set method
int libusb_claim_interface(libusb_device_handle *dev, int interface_number);
int libusb_set_auto_detach_kernel_driver(libusb_device_handle *dev_handle, int enable);
libusb_device *libusb_ref_device(libusb_device *dev);
int libusb_set_option(libusb_context *ctx, enum libusb_option option, ...);

//usb_open method
static struct discovered_devs *discovered_devs_alloc(void);
void discovered_devs_free(discovered_devs *discdevs);
ssize_t libusb_get_device_list(libusb_context *ctx, libusb_device ***list);
int libusb_get_device_descriptor(libusb_device *dev, libusb_device_descriptor *desc);
int libusb_open(libusb_device *dev, libusb_device_handle **handle);
void libusb_free_device_list(libusb_device **list, int unref_devices);
libusb_device_handle * libusb_open_device_with_vid_pid(libusb_context *ctx, uint16_t vendor_id, uint16_t product_id);


//usb_exit method
void libusb_exit(struct libusb_context *ctx);


#endif //KOREAPASSING_USB_INIT_H
