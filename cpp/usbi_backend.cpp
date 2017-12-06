//
// Created by root on 17. 11. 20.
//

#include "native-lib.h"
#include "usbi_backend.h"
#include <sys/ioctl.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>

char *usbfs_path = NULL;
clockid_t monotonic_clkid = -1;

static const char *find_usbfs_path(void) {

    const char *ret = "/dev/bus/usb";

    return ret;
}

static clockid_t find_monotonic_clock(void) {

#ifdef CLOCK_MONOTONIC
    struct timespec ts;
    int r;

    r = clock_gettime(CLOCK_MONOTONIC, &ts);
    if (r == 0)
        return CLOCK_MONOTONIC;
#endif

    return CLOCK_REALTIME;
}

static int supports_flag_bulk_continuation = -1;
static int supports_flag_zero_packet = -1;
static int sysfs_can_relate_devices = 0;
static int sysfs_has_descriptors = 0;

static int kernel_version_ge(int major, int minor, int sublevel) {

    struct utsname uts;
    int atoms, kmajor, kminor, ksublevel;

    if (uname(&uts) < 0)
        return -1;
    atoms = sscanf(uts.release, "%d.%d.%d", &kmajor, &kminor, &ksublevel);
    if (atoms < 1)
        return -1;

    if (kmajor > major)
        return 1;
    if (kmajor < major)
        return 0;

    if (atoms < 2)
        return 0 == minor && 0 == sublevel;
    if (kminor > minor)
        return 1;
    if (kminor < minor)
        return 0;

    if (atoms < 3)
        return 0 == sublevel;

    return ksublevel >= sublevel;
}

static int sysfs_has_file(const char *dirname, const char *filename) {

    struct stat statbuf;
    char path[PATH_MAX];
    int r;

    snprintf(path, PATH_MAX, "%s/%s/%s", SYSFS_DEVICE_PATH, dirname, filename);
    r = stat(path, &statbuf);
    if (r == 0 && S_ISREG(statbuf.st_mode))
        return 1;

    return 0;
}

int op_init(struct libusb_context *ctx) {
    struct stat statbuf;
    int r;

    usbfs_path = (char *) find_usbfs_path();
    if (!usbfs_path) {
        return -1;
    }

    if (monotonic_clkid == -1)
        monotonic_clkid = find_monotonic_clock();

    if (supports_flag_bulk_continuation == -1) {
        supports_flag_bulk_continuation = kernel_version_ge(2, 6, 32);
        if (supports_flag_bulk_continuation == -1) {
            return LIBUSB_ERROR_OTHER;
        }
    }

    if (supports_flag_bulk_continuation)

        if (-1 == supports_flag_zero_packet) {
            supports_flag_zero_packet = kernel_version_ge(2, 6, 31);
            if (-1 == supports_flag_zero_packet) {
                return LIBUSB_ERROR_OTHER;
            }
        }

    if (supports_flag_zero_packet)
        r = stat(SYSFS_DEVICE_PATH, &statbuf);
    if (r == 0 && S_ISDIR(statbuf.st_mode)) {
        DIR *devices = opendir(SYSFS_DEVICE_PATH);
        struct dirent *entry;

        if (!devices) {
            return LIBUSB_ERROR_IO;
        }

        while ((entry = readdir(devices))) {
            int has_busnum = 0, has_devnum = 0, has_descriptors = 0;
            int has_configuration_value = 0;

            if (strncmp(entry->d_name, "usb", 3) != 0)
                continue;

            has_busnum = sysfs_has_file(entry->d_name, "busnum");
            has_devnum = sysfs_has_file(entry->d_name, "devnum");
            has_descriptors = sysfs_has_file(entry->d_name, "descriptors");
            has_configuration_value = sysfs_has_file(entry->d_name, "bConfigurationValue");

            if (has_busnum && has_devnum && has_configuration_value)
                sysfs_can_relate_devices = 1;
            if (has_descriptors)
                sysfs_has_descriptors = 1;

            if (sysfs_has_descriptors && sysfs_can_relate_devices)
                break;
        }
        closedir(devices);

        if (!sysfs_can_relate_devices)
            sysfs_has_descriptors = 0;
    } else {
        sysfs_has_descriptors = 0;
        sysfs_can_relate_devices = 0;
    }


    return 0;
}

int op_exit() {
    return 0;
}


struct _device_handle_priv *device_handle_priv(libusb_device_handle *handle) {

    return (_device_handle_priv *) handle->os_priv;
}

int op_claim_interface(libusb_device_handle *handle, int iface) {

    int fd = device_handle_priv(handle)->fd;
    int r = ioctl(fd, IOCTL_USBFS_CLAIMINTF, &iface);
    if (r) {
        if (errno == ENOENT)
            return LIBUSB_ERROR_NOT_FOUND;
        else if (errno == EBUSY)
            return LIBUSB_ERROR_BUSY;
        else if (errno == ENODEV)
            return LIBUSB_ERROR_NO_DEVICE;

        return LIBUSB_ERROR_OTHER;
    }
    return 0;
}

size_t op_device_priv_size() {
    return sizeof(linux_device_priv);
}

//usb_open method



int __read_sysfs_attr(libusb_context *ctx, const char *devname, const char *attr) {

    char filename[PATH_MAX];
    FILE *f;
    int r, value;

    snprintf(filename, PATH_MAX, "%s/%s/%s", SYSFS_DEVICE_PATH,
             devname, attr);
    f = fopen(filename, "r");
    if (f == NULL) {
        if (errno == ENOENT) {
            return LIBUSB_ERROR_NO_DEVICE;
        }
        return LIBUSB_ERROR_IO;
    }

    r = fscanf(f, "%d", &value);
    fclose(f);
    if (r != 1) {
        return LIBUSB_ERROR_NO_DEVICE;
    }
    if (value < 0) {
        return LIBUSB_ERROR_IO;
    }

    return value;
}


//I don't know... this macro is necessary for list (usbi_get_device_by_session_id)
#define list_entry(ptr, type, member) \
    ((type *)((uintptr_t)(ptr) - (uintptr_t)(&((type *)0L)->member)))

#define list_for_each_entry(pos, head, member, type)            \
    for (pos = list_entry((head)->next, type, member);            \
         &pos->member != (head);                                \
         pos = list_entry(pos->member.next, type, member))

struct libusb_device *usbi_get_device_by_session_id(libusb_context *ctx, unsigned long session_id) {

    struct libusb_device *dev;
    struct libusb_device *ret = NULL;

    usbi_mutex_lock(&ctx->usb_devs_lock);
    list_for_each_entry(dev, &ctx->usb_devs, list, struct libusb_device)
        if (dev->session_data ==
            session_id) {
            ret = dev;
            break;
        }
    usbi_mutex_unlock(&ctx->usb_devs_lock);

    return ret;
}

struct libusb_device *usbi_alloc_device(libusb_context *ctx, unsigned long session_id) {

    size_t priv_size = op_device_priv_size();
    struct libusb_device *dev = (libusb_device *) calloc(1, sizeof(*dev) + priv_size);
    int r;

    if (!dev)
        return NULL;

    r = usbi_mutex_init(&dev->lock, NULL);
    if (r) {
        free(dev);
        return NULL;
    }

    dev->ctx = ctx;
    dev->refcnt = 1;
    dev->session_data = session_id;
    dev->speed = LIBUSB_SPEED_UNKNOWN;
    memset(&dev->os_priv, 0, priv_size);

    usbi_mutex_lock(&ctx->usb_devs_lock);
    list_add(&dev->list, &ctx->usb_devs);
    usbi_mutex_unlock(&ctx->usb_devs_lock);
    return dev;
}

static struct linux_device_priv *_device_priv(struct libusb_device *dev) {

    return (struct linux_device_priv *) dev->os_priv;
}

int _open_sysfs_attr(libusb_device *dev, const char *attr) {

    struct linux_device_priv *priv = _device_priv(dev);
    char filename[PATH_MAX];
    int fd;

    snprintf(filename, PATH_MAX, "%s/%s/%s",
             SYSFS_DEVICE_PATH, priv->sysfs_dir, attr);
    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        //LOGD("usbi_backend : _open_sysfs_attr_ open failed");
        return LIBUSB_ERROR_IO;
    }

    return fd;
}

int sysfs_get_active_config(libusb_device *dev, int *config) {

    char *endptr;
    char tmp[4] = {0, 0, 0, 0};
    long num;
    int fd;
    ssize_t r;

    fd = _open_sysfs_attr(dev, "bConfigurationValue");
    if (fd < 0)
        return fd;

    r = read(fd, tmp, sizeof(tmp));
    close(fd);
    if (r < 0) {
        //LOGE("usbi_backend : sysfs_get_active_config - read bConfigurationValue failed");
        return LIBUSB_ERROR_IO;
    } else if (r == 0) {
        //LOGE("usbi_backend : sysfs_get_active_config - device unconfigured");
        *config = -1;
        return 0;
    }

    if (tmp[sizeof(tmp) - 1] != 0) {
        //LOGE("usbi_backend : sysfs_get_active_config - not null-terminated?");
        return LIBUSB_ERROR_IO;
    } else if (tmp[0] == 0) {
        //LOGE("usbi_backend : sysfs_get_active_config - no configuration value?");
        return LIBUSB_ERROR_IO;
    }

    num = strtol(tmp, &endptr, 10);
    if (endptr == tmp) {
        //LOGE("usbi_backend : sysfs_get_active_config - error converting");
        return LIBUSB_ERROR_IO;
    }

    *config = (int) num;
    return 0;
}

static int usbdev_names = 0;

void _get_usbfs_path(libusb_device *dev, char *path) {

    if (usbdev_names)
        snprintf(path, PATH_MAX, "%s/usbdev%d.%d",
                 usbfs_path, dev->bus_number, dev->device_address);
    else
        snprintf(path, PATH_MAX, "%s/%03d/%03d",
                 usbfs_path, dev->bus_number, dev->device_address);
}

int usbfs_get_active_config(libusb_device *dev, int fd) {

    unsigned char active_config = 0;
    int r;

    struct usbfs_ctrltransfer ctrl = {
            .bmRequestType = LIBUSB_ENDPOINT_IN,
            .bRequest = LIBUSB_REQUEST_GET_CONFIGURATION,
            .wValue = 0,
            .wIndex = 0,
            .wLength = 1,
            .timeout = 1000,
            .data = &active_config
    };

    r = ioctl(fd, IOCTL_USBFS_CONTROL, &ctrl);
    if (r < 0) {
        if (errno == ENODEV)
            return LIBUSB_ERROR_NO_DEVICE;

        /* we hit this error path frequently with buggy devices :( */
        /*LOGW("usbi_backend : usbfs_get_active_config - get_configuration failed ret=%d errno=%d", r,
             errno);*/
        return LIBUSB_ERROR_IO;
    }

    return active_config;
}

int
usbi_parse_descriptor(unsigned char *source, const char *descriptor, void *dest, int host_endian) {

    unsigned char *sp = source, *dp = (unsigned char *) dest;
    uint16_t w;
    const char *cp;

    for (cp = descriptor; *cp; cp++) {
        switch (*cp) {
            case 'b':    /* 8-bit byte */
                *dp++ = *sp++;
                break;
            case 'w':    /* 16-bit word, convert from little endian to CPU */
                dp += ((uintptr_t) dp & 1);    /* Align to word boundary */

                if (host_endian) {
                    memcpy(dp, sp, 2);
                } else {
                    w = (sp[1] << 8) | sp[0];
                    *((uint16_t *) dp) = w;
                }
                sp += 2;
                dp += 2;
                break;
        }
    }

    return (int) (sp - source);
}

int seek_to_next_config(libusb_context *ctx, int fd, int host_endian) {

    struct libusb_config_descriptor config;
    unsigned char tmp[6];
    off_t off;
    ssize_t r;

    r = read(fd, tmp, sizeof(tmp));
    if (r < 0) {
        //LOGE("usbi_backend : seek_to_next_config - read failed ret=%d errno=%d", r, errno);
        return LIBUSB_ERROR_IO;
    } else if (r < sizeof(tmp)) {
        //LOGE("usbi_backend : seek_to_next_config - short descriptor read %d/%d", r, sizeof(tmp));
        return LIBUSB_ERROR_IO;
    }

    usbi_parse_descriptor(tmp, "bbwbb", &config, host_endian);
    off = lseek(fd, config.wTotalLength - sizeof(tmp), SEEK_CUR);
    if (off < 0) {
        //LOGE("usbi_backend : seek_to_next_config - seek failed ret=%d errno=%d", off, errno);
        return LIBUSB_ERROR_IO;
    }

    return 0;
}

int get_config_descriptor(libusb_context *ctx, int fd, uint8_t config_index, unsigned char *buffer,
                          size_t len) {

    off_t off;
    ssize_t r;

    off = lseek(fd, DEVICE_DESC_LENGTH, SEEK_SET);
    if (off < 0) {
        //LOGE("usbi_backend : get_config_descriptor - seek failed ret=%d errno=%d", off, errno);
        return LIBUSB_ERROR_IO;
    }

    /* might need to skip some configuration descriptors to reach the
     * requested configuration */
    while (config_index > 0) {
        r = seek_to_next_config(ctx, fd, 1);
        if (r < 0)
            return r;
        config_index--;
    }

    /* read the rest of the descriptor */
    r = read(fd, buffer, len);
    if (r < 0) {
        //LOGE("usbi_backend : get_config_descriptor - read failed ret=%d errno=%d", r, errno);
        return LIBUSB_ERROR_IO;
    } else if (r < len) {
        //LOGE("usbi_backend : get_config_descriptor - short output read %d/%d", r, len);
        return LIBUSB_ERROR_IO;
    }

    return 0;
}

int op_get_config_descriptor(libusb_device *dev, uint8_t config_index, unsigned char *buffer,
                             size_t len, int *host_endian) {

    char filename[PATH_MAX];
    int fd;
    int r;

    _get_usbfs_path(dev, filename);
    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        /*LOGE("usbi_backend : get_config_descriptor - open '%s' failed, ret=%d errno=%d", filename,
             fd, errno);*/
        return LIBUSB_ERROR_IO;
    }

    r = get_config_descriptor(DEVICE_CTX(dev), fd, config_index, buffer, len);
    close(fd);
    return r;
}

int usbi_get_config_index_by_value(libusb_device *dev, uint8_t bConfigurationValue, int *idx) {

    uint8_t i;

    //LOGD("usbi_backend : usbi_get_config_index_by_value - value %d", bConfigurationValue);

    for (i = 0; i < dev->num_configurations; i++) {
        unsigned char tmp[6];
        int host_endian;
        int r = op_get_config_descriptor(dev, i, tmp, sizeof(tmp), &host_endian);
        if (r < 0)
            return r;
        if (tmp[5] == bConfigurationValue) {
            *idx = i;
            return 0;
        }
    }

    *idx = -1;
    return 0;
}

int cache_active_config(libusb_device *dev, int fd, int active_config) {

    linux_device_priv *priv = _device_priv(dev);
    libusb_config_descriptor config;
    unsigned char tmp[8];
    unsigned char *buf;
    int idx;
    int r;

    if (active_config == -1) {
        idx = 0;
    } else {
        r = usbi_get_config_index_by_value(dev, active_config, &idx);
        if (r < 0)
            return r;
        if (idx == -1)
            return LIBUSB_ERROR_NOT_FOUND;
    }

    r = get_config_descriptor(DEVICE_CTX(dev), fd, idx, tmp, sizeof(tmp));
    if (r < 0) {
        //LOGE("usbi_backend : cache_active_config - first read error %d", r);
        return r;
    }

    usbi_parse_descriptor(tmp, "bbw", &config, 0);
    buf = (unsigned char *) malloc(config.wTotalLength);
    if (!buf)
        return LIBUSB_ERROR_NO_MEM;

    r = get_config_descriptor(DEVICE_CTX(dev), fd, idx, buf,
                              config.wTotalLength);
    if (r < 0) {
        free(buf);
        return r;
    }

    if (priv->config_descriptor)
        free(priv->config_descriptor);
    priv->config_descriptor = buf;
    return 0;
}

int initialize_device(libusb_device *dev, uint8_t busnum, uint8_t devaddr, const char *sysfs_dir) {

    linux_device_priv *priv = _device_priv(dev);
    unsigned char *dev_buf;
    char path[PATH_MAX];
    int fd, speed;
    int active_config = 0;
    int device_configured = 1;
    ssize_t r;

    dev->bus_number = busnum;
    dev->device_address = devaddr;

    if (sysfs_dir) {
        priv->sysfs_dir = (char *) malloc(strlen(sysfs_dir) + 1);
        if (!priv->sysfs_dir)
            return LIBUSB_ERROR_NO_MEM;
        strcpy(priv->sysfs_dir, sysfs_dir);

        speed = __read_sysfs_attr(DEVICE_CTX(dev), sysfs_dir, "speed");
        if (speed >= 0) {
            switch (speed) {
                case 1:
                    dev->speed = LIBUSB_SPEED_LOW;
                    break;
                case 12:
                    dev->speed = LIBUSB_SPEED_FULL;
                    break;
                case 480:
                    dev->speed = LIBUSB_SPEED_HIGH;
                    break;
                case 5000:
                    dev->speed = LIBUSB_SPEED_SUPER;
                    break;
                default:
                    LOGD("usbi_backend : initialize_device speed");
            }
        }
    }

    if (sysfs_has_descriptors)
        return 0;

    priv->dev_descriptor = NULL;
    priv->config_descriptor = NULL;

    if (sysfs_can_relate_devices) {
        int tmp = sysfs_get_active_config(dev, &active_config);
        if (tmp < 0)
            return tmp;
        if (active_config == -1)
            device_configured = 0;
    }

    _get_usbfs_path(dev, path);
    fd = open(path, O_RDWR);
    if (fd < 0 && errno == EACCES) {
        fd = open(path, O_RDONLY);
        active_config = -1;
    }

    if (fd < 0) {
        //LOGE("usbi_backend : initialize_device - open failed,");
        return LIBUSB_ERROR_IO;
    }

    if (!sysfs_can_relate_devices) {
        if (active_config == -1) {
            /*LOGE("usbi_backend : initialize_device - access to %s is read-only; cannot determine active configuration descriptor",
                 path);*/
        } else {
            active_config = usbfs_get_active_config(dev, fd);
            if (active_config == LIBUSB_ERROR_IO) {
                //LOGW("usbi_backend : initialize_device - couldn't query active configuration, assumung unconfigured");
                device_configured = 0;
            } else if (active_config < 0) {
                close(fd);
                return active_config;
            } else if (active_config == 0) {
                //LOGD("usbi_backend : initialize_device - active cfg 0? assuming unconfigured device");
                device_configured = 0;
            }
        }
    }

    dev_buf = (unsigned char *) malloc(DEVICE_DESC_LENGTH);
    if (!dev_buf) {
        close(fd);
        return LIBUSB_ERROR_NO_MEM;
    }

    r = read(fd, dev_buf, DEVICE_DESC_LENGTH);
    if (r < 0) {
        /*LOGE("usbi_backend : initialize_device - read descriptor failed ret=%d errno=%d", fd,
             errno);*/
        free(dev_buf);
        close(fd);
        return LIBUSB_ERROR_IO;
    } else if (r < DEVICE_DESC_LENGTH) {
        //LOGE("usbi_backend : initialize_device - short descriptor read (%d)", r);
        free(dev_buf);
        close(fd);
        return LIBUSB_ERROR_IO;
    }

    dev->num_configurations = dev_buf[DEVICE_DESC_LENGTH - 1];

    if (device_configured) {
        r = cache_active_config(dev, fd, active_config);
        if (r < 0) {
            close(fd);
            free(dev_buf);
            return r;
        }
    }

    close(fd);
    priv->dev_descriptor = dev_buf;
    return 0;
}

int sysfs_get_device_descriptor(libusb_device *dev, unsigned char *buffer) {

    int fd;
    ssize_t r;

    fd = _open_sysfs_attr(dev, "descriptors");
    if (fd < 0)
        return fd;

    r = read(fd, buffer, DEVICE_DESC_LENGTH);;
    close(fd);
    if (r < 0) {
        /*LOGE("usbi_backend : sysfs_get_device_descriptor - read failed, ret=%d errno=%d", fd,
             errno);*/
        return LIBUSB_ERROR_IO;
    } else if (r < DEVICE_DESC_LENGTH) {
        /*LOGE("usbi_backend : sysfs_get_device_descriptor - short read %d/%d", r,
             DEVICE_DESC_LENGTH);*/
        return LIBUSB_ERROR_IO;
    }

    return 0;
}

int usbfs_get_device_descriptor(libusb_device *dev, unsigned char *buffer) {

    struct linux_device_priv *priv = _device_priv(dev);

    memcpy(buffer, priv->dev_descriptor, DEVICE_DESC_LENGTH);
    return 0;
}

int op_get_device_descriptor(libusb_device *dev, unsigned char *buffer, int *host_endian) {

    if (sysfs_has_descriptors) {
        return sysfs_get_device_descriptor(dev, buffer);
    } else {
        *host_endian = 1;
        return usbfs_get_device_descriptor(dev, buffer);
    }
}

int usbi_sanitize_device(struct libusb_device *dev) {

    int r;
    unsigned char raw_desc[DEVICE_DESC_LENGTH];
    uint8_t num_configurations;
    int host_endian;

    r = op_get_device_descriptor(dev, raw_desc, &host_endian);
    if (r < 0)
        return r;

    num_configurations = raw_desc[DEVICE_DESC_LENGTH - 1];
    if (num_configurations > USB_MAXCONFIG) {
        //LOGE("usbi_backend : usbi_sanitize_device - too many configurations");
        return LIBUSB_ERROR_IO;
    } else if (0 == num_configurations)
        //LOGE("usbi_backend : usbi_sanitize_device - zero configurations, maybe an unauthorized device");

    dev->num_configurations = num_configurations;
    return 0;
}

struct discovered_devs *
discovered_devs_append(discovered_devs *discdevs, struct libusb_device *dev) {

    size_t len = discdevs->len;
    size_t capacity;

    if (len < discdevs->capacity) {
        discdevs->devices[len] = libusb_ref_device(dev);
        discdevs->len++;
        return discdevs;
    }

    //LOGD("usbi_backend : discovered_devs_append - need to increase capacity");
    capacity = discdevs->capacity + DISCOVERED_DEVICES_SIZE_STEP;
    discdevs = (discovered_devs *) realloc(discdevs,
                                           sizeof(*discdevs) + (sizeof(void *) * capacity));
    if (discdevs) {
        discdevs->capacity = capacity;
        discdevs->devices[len] = libusb_ref_device(dev);
        discdevs->len++;
    }

    return discdevs;
}

//check
void op_destroy_device(struct libusb_device *dev) {

    struct linux_device_priv *priv = _device_priv(dev);
    if (!sysfs_has_descriptors) {
        if (priv->dev_descriptor)
            free(priv->dev_descriptor);
        if (priv->config_descriptor)
            free(priv->config_descriptor);
    }
    if (priv->sysfs_dir)
        free(priv->sysfs_dir);
}

//check
void libusb_unref_device(libusb_device *dev) {

    int refcnt;

    if (!dev)
        return;

    usbi_mutex_lock(&dev->lock);
    refcnt = --dev->refcnt;
    usbi_mutex_unlock(&dev->lock);

    if (refcnt == 0) {
        //LOGD("usbi_backend : libusb_unref_device - destroy device %d.%d", dev->bus_number, dev->device_address);

        op_destroy_device(dev);

        usbi_mutex_lock(&dev->ctx->usb_devs_lock);
        list_del(&dev->list);
        usbi_mutex_unlock(&dev->ctx->usb_devs_lock);

        usbi_mutex_destroy(&dev->lock);
        free(dev);
    }
}

int enumerate_device(libusb_context *ctx, discovered_devs **_discdevs, uint8_t busnum, uint8_t devaddr, const char *sysfs_dir) {

    struct discovered_devs *discdevs;
    unsigned long session_id;
    int need_unref = 0;
    struct libusb_device *dev;
    int r = 0;

    //merge bit
    session_id = busnum << 8 | devaddr;

    dev = usbi_get_device_by_session_id(ctx, session_id);
    if (dev) {
    } else {
        dev = usbi_alloc_device(ctx, session_id);
        if (!dev)
            return LIBUSB_ERROR_NO_MEM;
        need_unref = 1;
        r = initialize_device(dev, busnum, devaddr, sysfs_dir);
        if (r < 0)
            goto out;
        r = usbi_sanitize_device(dev);
        if (r < 0)
            goto out;
    }

    discdevs = discovered_devs_append(*_discdevs, dev);
    if (!discdevs)
        r = LIBUSB_ERROR_NO_MEM;
    else
        *_discdevs = discdevs;

    out:
    if (need_unref)
        libusb_unref_device(dev);
    return r;
}

int sysfs_scan_device(libusb_context *ctx, discovered_devs **_discdevs, const char *devname) {

    int busnum;
    int devaddr;


    busnum = __read_sysfs_attr(ctx, devname, "busnum");
    if (busnum < 0)
        return busnum;

    devaddr = __read_sysfs_attr(ctx, devname, "devnum");
    if (devaddr < 0)
        return devaddr;

    if (busnum > 255 || devaddr > 255)
        return LIBUSB_ERROR_INVALID_PARAM;

    return enumerate_device(ctx, _discdevs, busnum & 0xff, devaddr & 0xff,
                            devname);
}


static int sysfs_get_device_list(libusb_context *ctx, discovered_devs **_discdevs) {

    struct discovered_devs *discdevs = *_discdevs;
    DIR *devices = opendir(SYSFS_DEVICE_PATH);
    struct dirent *entry;
    int r = LIBUSB_ERROR_IO;

    if (!devices) {
        return r;
    }

    while ((entry = readdir(devices))) {
        struct discovered_devs *discdevs_new = discdevs;

        if ((!isdigit(entry->d_name[0]) && strncmp(entry->d_name, "usb", 3))
            || strchr(entry->d_name, ':'))
            continue;

        if (sysfs_scan_device(ctx, &discdevs_new, entry->d_name)) {
            continue;
        }

        r = 0;
        discdevs = discdevs_new;
    }

    if (!r)
        *_discdevs = discdevs;
    closedir(devices);
    return r;
}

int _is_usbdev_entry(dirent *entry, int *bus_p, int *dev_p) {

    int busnum, devnum;

    if (sscanf(entry->d_name, "usbdev%d.%d", &busnum, &devnum) != 2)
        return 0;

    //LOGD("usbi_backend : _is_usbdev_entry - found: %s", entry->d_name);
    if (bus_p != NULL)
        *bus_p = busnum;
    if (dev_p != NULL)
        *dev_p = devnum;
    return 1;
}

int usbfs_scan_busdir(libusb_context *ctx, discovered_devs **_discdevs, uint8_t busnum) {

    DIR *dir;
    char dirpath[PATH_MAX];
    struct dirent *entry;
    struct discovered_devs *discdevs = *_discdevs;
    int r = LIBUSB_ERROR_IO;

    snprintf(dirpath, PATH_MAX, "%s/%03d", usbfs_path, busnum);
    //LOGD("usbi_backend : usbfs_scan_busdir - %s", dirpath);
    dir = opendir(dirpath);
    if (!dir) {
        //LOGD("usbi_backend : usbfs_scan_busdir - opendir '%s' failed, errno=%d", dirpath, errno);
        return r;
    }

    while ((entry = readdir(dir))) {
        int devaddr;

        if (entry->d_name[0] == '.')
            continue;

        devaddr = atoi(entry->d_name);
        if (devaddr == 0) {
            //LOGD("usbi_backend : usbfs_scan_busdir - unknown dir entry %s", entry->d_name);
            continue;
        }

        if (enumerate_device(ctx, &discdevs, busnum, (uint8_t) devaddr, NULL)) {
            //LOGD("usbi_backend : usbfs_scan_busdir - failed to enumerate dir entry %s", entry->d_name);
            continue;
        }

        r = 0;
    }

    if (!r)
        *_discdevs = discdevs;
    closedir(dir);
    return r;
}

int usbfs_get_device_list(libusb_context *ctx, discovered_devs **_discdevs) {

    struct dirent *entry;
    DIR *buses = opendir(usbfs_path);
    struct discovered_devs *discdevs = *_discdevs;
    int r = 0;

    if (!buses) {
        //LOGD("usbi_backend : usbfs_get_device_list - opendir buses failed errno=%d", errno);
        return LIBUSB_ERROR_IO;
    }

    while ((entry = readdir(buses))) {
        struct discovered_devs *discdevs_new = discdevs;
        int busnum;

        if (entry->d_name[0] == '.')
            continue;

        if (usbdev_names) {
            int devaddr;
            if (!_is_usbdev_entry(entry, &busnum, &devaddr))
                continue;

            r = enumerate_device(ctx, &discdevs_new, busnum,
                                 (uint8_t) devaddr, NULL);
            if (r < 0) {
                //LOGD("usbi_backend : usbfs_get_device_list - failed to enumerate dir entry %s", entry->d_name);
                continue;
            }
        } else {
            busnum = atoi(entry->d_name);
            if (busnum == 0) {
                //LOGD("usbi_backend : usbfs_get_device_list - unknown dir entry %s", entry->d_name);
                continue;
            }

            r = usbfs_scan_busdir(ctx, &discdevs_new, busnum);
            if (r < 0)
                goto out;
        }
        discdevs = discdevs_new;
    }

    out:
    closedir(buses);
    *_discdevs = discdevs;
    return r;

}

int op_get_device_list(libusb_context *ctx, discovered_devs **_discdevs) {

    if (sysfs_can_relate_devices != 0)
        return sysfs_get_device_list(ctx, _discdevs);
    else
        return usbfs_get_device_list(ctx, _discdevs);
}

size_t device_handle_priv_size(){

    return sizeof(_device_handle_priv);
}

//op_open method

int find_fd_by_name(char *file_name) {

    struct dirent *fd_dirent;
    DIR *proc_fd = opendir("/proc/self/fd");
    int ret = -1;

    while (fd_dirent = readdir(proc_fd))
    {
        char link_file_name[1024];
        char fd_file_name[1024];

        if (fd_dirent->d_type != DT_LNK)
        {
            continue;
        }

        snprintf(link_file_name, 1024, "/proc/self/fd/%s", fd_dirent->d_name);

        memset(fd_file_name, 0, sizeof(fd_file_name));
        readlink(link_file_name, fd_file_name, sizeof(fd_file_name) - 1);

        if (!strcmp(fd_file_name, file_name))
        {
            ret = atoi(fd_dirent->d_name);
        }
    }

    closedir(proc_fd);

    return ret;
}

int op_open(libusb_device_handle *handle){

	struct _device_handle_priv *hpriv = device_handle_priv(handle);
	char filename[PATH_MAX];

	_get_usbfs_path(handle->dev, filename);
    //LOGD("usbi_backend : op_open - opening %s", filename);
	// *** PrimeSense patch for Android ***
	hpriv->fd = find_fd_by_name(filename);

	// Fallback for regular non-java applications
	if (hpriv->fd == -1)
	{
		hpriv->fd = open(filename, O_RDWR);
	}
// *** PrimeSense patch for Android ***
	if (hpriv->fd < 0) {
		if (errno == EACCES) {
            /*LOGE("usbi_backend : op_open - libusb couldn't open USB device %s: "
                         "Permission denied.", filename);*/
            ////LOGE("usbi_backend : op_open - libusb requires write access to USB device nodes.");
			return LIBUSB_ERROR_ACCESS;
		} else if (errno == ENOENT) {
            /*//LOGE("usbi_backend : op_open - libusb couldn't open USB device %s: "
                         "No such file or directory.", filename);*/
			return LIBUSB_ERROR_NO_DEVICE;
		} else {
            ////LOGE("usbi_backend : op_open - open failed, code %d errno %d", hpriv->fd, errno);
			return LIBUSB_ERROR_IO;
		}
	}

	return usbi_add_pollfd(HANDLE_CTX(handle), hpriv->fd, POLLOUT);
}

void libusb_lock_events(libusb_context *ctx) {

    USBI_GET_CONTEXT(ctx);
    usbi_mutex_lock(&ctx->events_lock);
    ctx->event_handler_active = 1;

}

void libusb_unlock_events(libusb_context *ctx) {

    USBI_GET_CONTEXT(ctx);
    ctx->event_handler_active = 0;
    usbi_mutex_unlock(&ctx->events_lock);

    usbi_mutex_lock(&ctx->event_waiters_lock);
    usbi_cond_broadcast(&ctx->event_waiters_cond);
    usbi_mutex_unlock(&ctx->event_waiters_lock);
}

void usbi_fd_notification(libusb_context *ctx) {

    unsigned char dummy = 1;
    ssize_t r;

    if (ctx == NULL)
        return;

    usbi_mutex_lock(&ctx->pollfd_modify_lock);
    ctx->pollfd_modify++;
    usbi_mutex_unlock(&ctx->pollfd_modify_lock);

    r = usbi_write(ctx->ctrl_pipe[1], &dummy, sizeof(dummy));
    if (r <= 0) {
        //LOGW("usbi_backend : usbi_fd_notification - internal signalling write failed");
        usbi_mutex_lock(&ctx->pollfd_modify_lock);
        ctx->pollfd_modify--;
        usbi_mutex_unlock(&ctx->pollfd_modify_lock);
        return;
    }

    libusb_lock_events(ctx);

    /* read the dummy data */
    r = usbi_read(ctx->ctrl_pipe[0], &dummy, sizeof(dummy));
    if (r <= 0)
        //LOGW("usbi_backend : usbi_fd_notification - internal signalling read failed");

    usbi_mutex_lock(&ctx->pollfd_modify_lock);
    ctx->pollfd_modify--;
    usbi_mutex_unlock(&ctx->pollfd_modify_lock);

    libusb_unlock_events(ctx);
}

int get_device_list(){
    return NULL;
}

int op_clock_gettime(int clk_id, struct timespec *tp) {

    switch (clk_id) {
        case USBI_CLOCK_MONOTONIC:
            return clock_gettime(monotonic_clkid, tp);
        case USBI_CLOCK_REALTIME:
            return clock_gettime(CLOCK_REALTIME, tp);
        default:
            return LIBUSB_ERROR_INVALID_PARAM;
    }
}
