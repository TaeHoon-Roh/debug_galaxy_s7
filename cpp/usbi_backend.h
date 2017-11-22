//
// Created by root on 17. 11. 21.
//

#ifndef KOREAPASSING_USBI_BACKEND_H
#define KOREAPASSING_USBI_BACKEND_H
#include <sys/stat.h>
#include <sys/utsname.h>
#include <dirent.h>

int op_init(struct libusb_context *ctx);
int op_exit();

#endif //KOREAPASSING_USBI_BACKEND_H
