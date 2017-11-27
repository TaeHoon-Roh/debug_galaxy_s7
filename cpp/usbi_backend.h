//
// Created by root on 17. 11. 21.
//

#ifndef KOREAPASSING_USBI_BACKEND_H
#define KOREAPASSING_USBI_BACKEND_H
#include <sys/stat.h>
#include <sys/utsname.h>
#include <dirent.h>
#include "usb_device.h"
#include "usb_init.h"

#define IOCTL_USBFS_CLAIMINTF	_IOR('U', 15, unsigned int)
struct _device_handle_priv {

    int fd;
    int fd_removed;
    uint32_t caps;
};

struct linux_device_priv{

    char *sysfs_dir;
    unsigned char *dev_descriptor;
    unsigned char *config_descriptor;
    int descriptors_len;
    int active_config;
};



int op_init(struct libusb_context *ctx);
int op_exit();
int op_claim_interface(libusb_device_handle *handle, int iface);


extern uint32_t caps_backend;

//usb_open
int op_get_device_list(libusb_context *ctx, discovered_devs **_discdevs);


//usb_open -> inline funtion
inline void list_add(list_head *entry, list_head *head) {

    entry->next = head->next;
    entry->prev = head;

    head->next->prev = entry;
    head->next = entry;
}

inline void list_add_tail(list_head *entry, list_head *head) {

    entry->next = head;
    entry->prev = head->prev;

    head->prev->next = entry;
    head->prev = entry;
}

inline void list_del(list_head *entry) {

    entry->next->prev = entry->prev;
    entry->prev->next = entry->next;
}

void libusb_unref_device(libusb_device *dev);

libusb_device *libusb_ref_device(libusb_device *dev);

int op_get_device_descriptor(libusb_device *dev, unsigned char *buffer, int *host_endian);

int op_get_device_list(libusb_context *ctx, discovered_devs **_discdevs);

#define DEVICE_DESC_LENGTH		18

#define DEVICE_CTX(dev) ((dev)->ctx)
#define HANDLE_CTX(handle) (DEVICE_CTX((handle)->dev))

size_t device_handle_priv_size();

int op_open(libusb_device_handle *handle);
void usbi_fd_notification(libusb_context *ctx);

#define usbi_write write
#define usbi_read read

#endif //KOREAPASSING_USBI_BACKEND_H
