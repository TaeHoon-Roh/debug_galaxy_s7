//
// Created by root on 17. 11. 20.
//

#ifndef KOREAPASSING_USB_DEVICE_H
#define KOREAPASSING_USB_DEVICE_H

#include "native-lib.h"
#include "thread.h"
#include <sys/poll.h>

#define USB_MAXENDPOINTS	32
#define USB_MAXINTERFACES	32
#define USB_MAXCONFIG		8

/* Backend specific capabilities */
#define USBI_CAP_HAS_HID_ACCESS			0x00010000
#define USBI_CAP_SUPPORTS_DETACH_KERNEL_DRIVER	0x00020000

uint32_t caps_backend = 0;

enum libusb_error {
    /** Success (no error) */
            LIBUSB_SUCCESS = 0,

    /** Input/output error */
            LIBUSB_ERROR_IO = -1,

    /** Invalid parameter */
            LIBUSB_ERROR_INVALID_PARAM = -2,

    /** Access denied (insufficient permissions) */
            LIBUSB_ERROR_ACCESS = -3,

    /** No such device (it may have been disconnected) */
            LIBUSB_ERROR_NO_DEVICE = -4,

    /** Entity not found */
            LIBUSB_ERROR_NOT_FOUND = -5,

    /** Resource busy */
            LIBUSB_ERROR_BUSY = -6,

    /** Operation timed out */
            LIBUSB_ERROR_TIMEOUT = -7,

    /** Overflow */
            LIBUSB_ERROR_OVERFLOW = -8,

    /** Pipe error */
            LIBUSB_ERROR_PIPE = -9,

    /** System call interrupted (perhaps due to signal) */
            LIBUSB_ERROR_INTERRUPTED = -10,

    /** Insufficient memory */
            LIBUSB_ERROR_NO_MEM = -11,

    /** Operation not supported or unimplemented on this platform */
            LIBUSB_ERROR_NOT_SUPPORTED = -12,

    /* NB! Remember to update libusb_error_name()
       when adding new error codes here. */

    /** Other error */
            LIBUSB_ERROR_OTHER = -99,
};

#define SYSFS_DEVICE_PATH "/sys/bus/usb/devices"

enum libusb_speed {
    /** The OS doesn't report or know the device speed. */
            LIBUSB_SPEED_UNKNOWN = 0,

    /** The device is operating at low speed (1.5MBit/s). */
            LIBUSB_SPEED_LOW = 1,

    /** The device is operating at full speed (12MBit/s). */
            LIBUSB_SPEED_FULL = 2,

    /** The device is operating at high speed (480MBit/s). */
            LIBUSB_SPEED_HIGH = 3,

    /** The device is operating at super speed (5000MBit/s). */
            LIBUSB_SPEED_SUPER = 4,
};

struct list_head {
    struct list_head *prev, *next;
};

struct libusb_device_descriptor {
    /** Size of this descriptor (in bytes) */
    uint8_t bLength;

    /** Descriptor type. Will have value
     * \ref libusb_descriptor_type::LIBUSB_DT_DEVICE LIBUSB_DT_DEVICE in this
     * context. */
    uint8_t bDescriptorType;

    /** USB specification release number in binary-coded decimal. A value of
     * 0x0200 indicates USB 2.0, 0x0110 indicates USB 1.1, etc. */
    uint16_t bcdUSB;

    /** USB-IF class code for the device. See \ref libusb_class_code. */
    uint8_t bDeviceClass;

    /** USB-IF subclass code for the device, qualified by the bDeviceClass
     * value */
    uint8_t bDeviceSubClass;

    /** USB-IF protocol code for the device, qualified by the bDeviceClass and
     * bDeviceSubClass values */
    uint8_t bDeviceProtocol;

    /** Maximum packet size for endpoint 0 */
    uint8_t bMaxPacketSize0;

    /** USB-IF vendor ID */
    uint16_t idVendor;

    /** USB-IF product ID */
    uint16_t idProduct;

    /** Device release number in binary-coded decimal */
    uint16_t bcdDevice;

    /** Index of string descriptor describing manufacturer */
    uint8_t iManufacturer;

    /** Index of string descriptor describing product */
    uint8_t iProduct;

    /** Index of string descriptor containing device serial number */
    uint8_t iSerialNumber;

    /** Number of possible configurations */
    uint8_t bNumConfigurations;
};


struct libusb_device {
    /* lock protects refcnt, everything else is finalized at initialization
     * time */
    usbi_mutex_t lock;
    int refcnt;

    struct libusb_context *ctx;

    uint8_t bus_number;
    uint8_t port_number;
    struct libusb_device *parent_dev;
    uint8_t device_address;
    uint8_t num_configurations;
    enum libusb_speed speed;

    struct list_head list;
    unsigned long session_data;

    struct libusb_device_descriptor device_descriptor;
    int attached;

    unsigned char os_priv
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
            [] /* valid C99 code */
#else
    [0] /* non-standard, but usually working code */
#endif
#if defined(OS_SUNOS)
    __attribute__ ((aligned (8)));
#else
    ;
#endif
};


struct libusb_device_handle {
    /* lock protects claimed_interfaces */
    usbi_mutex_t lock;
    unsigned long claimed_interfaces;

    struct list_head list;
    struct libusb_device *dev;
    int auto_detach_kernel_driver;
    unsigned char os_priv
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
            [] /* valid C99 code */
#else
    [0] /* non-standard, but usually working code */
#endif
#if defined(OS_SUNOS)
    __attribute__ ((aligned (8)));
#else
    ;
#endif
};

enum {
    USBI_CLOCK_MONOTONIC,
    USBI_CLOCK_REALTIME
};


struct libusb_context {
    int debug;
    int debug_fixed;

    int ctrl_pipe[2];

    struct list_head usb_devs;
    usbi_mutex_t usb_devs_lock;

    struct list_head open_devs;
    usbi_mutex_t open_devs_lock;

    struct list_head hotplug_cbs;
    usbi_mutex_t hotplug_cbs_lock;

    struct list_head flying_transfers;
    usbi_mutex_t flying_transfers_lock;

    void *fd_cb_user_data;

    usbi_mutex_t events_lock;

    int event_handler_active;

    usbi_tls_key_t event_handling_key;

    usbi_mutex_t event_waiters_lock;
    usbi_cond_t event_waiters_cond;

    usbi_mutex_t event_data_lock;

    unsigned int event_flags;

    unsigned int device_close;

    struct list_head ipollfds;
    struct pollfd *pollfds;
    usbi_mutex_t pollfds_lock;

    unsigned int pollfd_modify;
    usbi_mutex_t pollfd_modify_lock;

    struct list_head hotplug_msgs;

    struct list_head completed_transfers;

#ifdef USBI_TIMERFD_AVAILABLE
    int timerfd;
#endif

    struct list_head list;

    struct pollfd fd_added_cb;
    struct pollfd fd_removed_db;

};


#endif //KOREAPASSING_USB_DEVICE_H
