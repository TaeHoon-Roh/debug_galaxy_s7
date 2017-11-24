//
// Created by root on 17. 11. 24.
//

#ifndef KOREAPASSING_USB_INIT_H
#define KOREAPASSING_USB_INIT_H

#include "native-lib.h"
#include "usbi_backend.h"
#include "thread.h"
#include <unistd.h>

static usbi_mutex_static_t default_context_lock = USBI_MUTEX_INITIALIZER;
static struct timeval timestamp_origin = {0, 0};
static int default_context_refcnt = 0;

#define usbi_pipe pipe

int usbi_add_pollfd(libusb_context *ctx, int fd, short events);
int usbi_mutex_init_recursive(pthread_mutex_t *mutex, pthread_mutexattr_t *attr);
inline void list_init(list_head *entry);
int usbi_io_init(libusb_context *ctx);
int libusb_init(libusb_context **context);




#endif //KOREAPASSING_USB_INIT_H
