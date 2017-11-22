//
// Created by root on 17. 11. 21.
//

#ifndef KOREAPASSING_USBI_BACKEND_H
#define KOREAPASSING_USBI_BACKEND_H
#include <sys/stat.h>
#include <sys/utsname.h>
#include <dirent.h>
#include "usb_device.h"

#define IOCTL_USBFS_CLAIMINTF	_IOR('U', 15, unsigned int)
struct _device_handle_priv {
    int fd;
    int fd_removed;
    uint32_t caps;
};


int op_init(struct libusb_context *ctx);
int op_exit();
int op_claim_interface(libusb_device_handle *handle, int iface);

#endif //KOREAPASSING_USBI_BACKEND_H
