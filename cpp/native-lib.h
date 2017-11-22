//
// Created by root on 17. 11. 20.
//

#ifndef KOREAPASSING_NATIVE_LIB_H
#define KOREAPASSING_NATIVE_LIB_H

#include <stdio.h>
#include <stdlib.h>
#include <jni.h>
#include "usb_device.h"
#include "thread.h"

#define FX_TYPE_UNDEFINED  -1
#define FX_TYPE_AN21       0	/* Original AnchorChips parts */
#define FX_TYPE_FX1        1	/* Updated Cypress versions */
#define FX_TYPE_FX2        2	/* USB 2.0 versions */
#define FX_TYPE_FX2LP      3	/* Updated FX2 */
#define FX_TYPE_FX3        4	/* USB 3.0 versions */
#define FX_TYPE_MAX        5
#define FX_TYPE_NAMES      { "an21", "fx", "fx2", "fx2lp", "fx3" }

#define IMG_TYPE_UNDEFINED -1
#define IMG_TYPE_HEX       0	/* Intel HEX */
#define IMG_TYPE_IIC       1	/* Cypress 8051 IIC */
#define IMG_TYPE_BIX       2	/* Cypress 8051 BIX */
#define IMG_TYPE_IMG       3	/* Cypress IMG format */
#define IMG_TYPE_MAX       4
#define IMG_TYPE_NAMES     { "Intel HEX", "Cypress 8051 IIC", "Cypress 8051 BIX", "Cypress IMG format" }

typedef struct {
    uint16_t vid;
    uint16_t pid;
    int type;
    const char* designation;
} fx_known_device;

#define FX_KNOWN_DEVICES { \
	{ 0x0547, 0x2122, FX_TYPE_AN21, "Cypress EZ-USB (2122S)" },\
	{ 0x0547, 0x2125, FX_TYPE_AN21, "Cypress EZ-USB (2121S/2125S)" },\
	{ 0x0547, 0x2126, FX_TYPE_AN21, "Cypress EZ-USB (2126S)" },\
	{ 0x0547, 0x2131, FX_TYPE_AN21, "Cypress EZ-USB (2131Q/2131S/2135S)" },\
	{ 0x0547, 0x2136, FX_TYPE_AN21, "Cypress EZ-USB (2136S)" },\
	{ 0x0547, 0x2225, FX_TYPE_AN21, "Cypress EZ-USB (2225)" },\
	{ 0x0547, 0x2226, FX_TYPE_AN21, "Cypress EZ-USB (2226)" },\
	{ 0x0547, 0x2235, FX_TYPE_AN21, "Cypress EZ-USB (2235)" },\
	{ 0x0547, 0x2236, FX_TYPE_AN21, "Cypress EZ-USB (2236)" },\
	{ 0x04b4, 0x6473, FX_TYPE_FX1, "Cypress EZ-USB FX1" },\
	{ 0x04b4, 0x8613, FX_TYPE_FX2LP, "Cypress EZ-USB FX2LP (68013A/68014A/68015A/68016A)" }, \
	{ 0x04b4, 0x00f3, FX_TYPE_FX3, "Cypress FX3" },\
}
void fxload();
int libusb_init(struct libusb_context **context);

#define FIRMWARE 0
#define LOADER 1


#endif //KOREAPASSING_NATIVE_LIB_H
