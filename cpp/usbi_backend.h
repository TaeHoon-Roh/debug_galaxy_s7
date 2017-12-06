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


//about list
#define list_entry(ptr, type, member) \
	((type *)((uintptr_t)(ptr) - (uintptr_t)offsetof(type, member)))

#define list_first_entry(ptr, type, member) \
	list_entry((ptr)->next, type, member)

#define list_for_each_entry(pos, head, member, type)			\
	for (pos = list_entry((head)->next, type, member);		\
		 &pos->member != (head);				\
		 pos = list_entry(pos->member.next, type, member))

#define list_for_each_entry_safe(pos, n, head, member, type)		\
	for (pos = list_entry((head)->next, type, member),		\
		 n = list_entry(pos->member.next, type, member);	\
		 &pos->member != (head);				\
		 pos = n, n = list_entry(n->member.next, type, member))

#define list_empty(entry) ((entry)->next == (entry))


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

//list end

#ifdef USBI_TIMERFD_AVAILABLE
#define usbi_using_timerfd(ctx) ((ctx)->timerfd >= 0)
#else
#define usbi_using_timerfd(ctx) (0)
#endif

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

extern uint32_t caps_backend;

enum usbi_transfer_timeout_flags {
            USBI_TRANSFER_OS_HANDLES_TIMEOUT = 1 << 0,
            USBI_TRANSFER_TIMEOUT_HANDLED = 1 << 1,
            USBI_TRANSFER_TIMED_OUT = 1 << 2,
};


libusb_device *libusb_ref_device(libusb_device *dev);

#define DEVICE_DESC_LENGTH		18

#define DEVICE_CTX(dev) ((dev)->ctx)
#define HANDLE_CTX(handle) (DEVICE_CTX((handle)->dev))

size_t device_handle_priv_size();

void usbi_fd_notification(libusb_context *ctx);

#define usbi_write write
#define usbi_read read




//usbi_backend method

static int kernel_version_ge(int major, int minor, int sublevel);
static int sysfs_has_file(const char *dirname, const char *filename);
int op_init(struct libusb_context *ctx);
int op_exit();
struct _device_handle_priv *device_handle_priv(libusb_device_handle *handle);
int op_claim_interface(libusb_device_handle *handle, int iface);
size_t op_device_priv_size();
int __read_sysfs_attr(libusb_context *ctx, const char *devname, const char *attr);
struct libusb_device *usbi_get_device_by_session_id(libusb_context *ctx, unsigned long session_id);
struct libusb_device *usbi_alloc_device(libusb_context *ctx, unsigned long session_id);
static struct linux_device_priv *_device_priv(struct libusb_device *dev);
int _open_sysfs_attr(libusb_device *dev, const char *attr);
int sysfs_get_active_config(libusb_device *dev, int *config);
void _get_usbfs_path(libusb_device *dev, char *path);
int usbfs_get_active_config(libusb_device *dev, int fd);
int usbi_parse_descriptor(unsigned char *source, const char *descriptor, void *dest, int host_endian);
int seek_to_next_config(libusb_context *ctx, int fd, int host_endian);
int get_config_descriptor(libusb_context *ctx, int fd, uint8_t config_index, unsigned char *buffer, size_t len);
int op_get_config_descriptor(libusb_device *dev, uint8_t config_index, unsigned char *buffer, size_t len, int *host_endian);
int usbi_get_config_index_by_value(libusb_device *dev, uint8_t bConfigurationValue, int *idx);
int cache_active_config(libusb_device *dev, int fd, int active_config);
int initialize_device(libusb_device *dev, uint8_t busnum, uint8_t devaddr, const char *sysfs_dir);
int sysfs_get_device_descriptor(libusb_device *dev, unsigned char *buffer);
int usbfs_get_device_descriptor(libusb_device *dev, unsigned char *buffer);
int op_get_device_descriptor(libusb_device *dev, unsigned char *buffer, int *host_endian);
int usbi_sanitize_device(struct libusb_device *dev);
struct discovered_devs * discovered_devs_append(discovered_devs *discdevs, struct libusb_device *dev);
void op_destroy_device(struct libusb_device *dev);
void libusb_unref_device(libusb_device *dev);
int enumerate_device(libusb_context *ctx, discovered_devs **_discdevs, uint8_t busnum, uint8_t devaddr, const char *sysfs_dir);
int sysfs_scan_device(libusb_context *ctx, discovered_devs **_discdevs, const char *devname);
static int sysfs_get_device_list(libusb_context *ctx, discovered_devs **_discdevs);
int _is_usbdev_entry(dirent *entry, int *bus_p, int *dev_p);
int usbfs_scan_busdir(libusb_context *ctx, discovered_devs **_discdevs, uint8_t busnum);
int usbfs_get_device_list(libusb_context *ctx, discovered_devs **_discdevs);
int op_get_device_list(libusb_context *ctx, discovered_devs **_discdevs);
size_t device_handle_priv_size();
int find_fd_by_name(char *file_name);
int op_open(libusb_device_handle *handle);
void libusb_lock_events(libusb_context *ctx);
void libusb_unlock_events(libusb_context *ctx);
void usbi_fd_notification(libusb_context *ctx);
int get_device_list();
int op_clock_gettime(int clk_id, struct timespec *tp);



#endif //KOREAPASSING_USBI_BACKEND_H
