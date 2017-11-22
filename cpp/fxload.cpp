//
// Created by root on 17. 11. 20.
//
#include "native-lib.h"
#include "usb_device.h"

void fxload(){
    fx_known_device known_device[] = FX_KNOWN_DEVICES;
    char *path[] = {NULL, NULL};
    char *device_id = NULL;
    char *device_path = getenv("DEVICE");
    char *type = NULL;
    char *fx_name[FX_TYPE_MAX] = FX_TYPE_NAMES;
    char *ext;
    char *img_name[] = IMG_TYPE_NAMES;
    int fx_type = FX_TYPE_UNDEFINED;
    int img_type [sizeof(path)];
    int opt, status;
    unsigned int i,j;
    unsigned vid = 0;
    unsigned pid = 0;
    unsigned busnum = 0;
    unsigned devaddr = 0;
    unsigned _busnum;
    unsigned _devaddr;
    struct libusb_device *dev, **devs;
    struct libusb_device_handle *device = NULL;
    struct libusb_device_descriptor desc;

    device_id = "04b4:00f3";
    vid = 0x04b4;
    pid = 0x00f3;

    path[FIRMWARE] = "/storage/emulated/0/usb_stream_firmware.img";

    type = "fx3";

    //open device
    status = libusb_init(NULL);
    //libusb_set_auto_detach_kernel_driver -> delete funtion
    device->auto_detach_kernel_driver = 1;
    status = libusb_claim_interface(device, 0);



}