//
// Created by root on 17. 12. 5.
//
#include "native-lib.h"
#include "usb_device.h"

void print_devs(libusb_device **pDevice);

int listdevs() {
    libusb_device **devs;
    int r;
    ssize_t cnt;

    r = libusb_init(NULL);
    if (r < 0)
        return r;
    cnt = libusb_get_device_list(NULL, &devs);
    if(cnt < 0)
        return (int)cnt;
    print_devs(devs);
    libusb_free_device_list(devs, 1);

    libusb_exit(NULL);
    return 0;

}

void print_devs(libusb_device **pDevice) {

}
