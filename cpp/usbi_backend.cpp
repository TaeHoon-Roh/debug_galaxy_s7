//
// Created by root on 17. 11. 20.
//

#include "native-lib.h"
#include "usbi_backend.h"


char *usbfs_path = NULL;
clockid_t monotonic_clkid= -1;

static const char *find_usbfs_path(void)
{
/*  Android SElinux workaround */
    const char *ret = "/dev/bus/usb";

    return ret;
}

static clockid_t find_monotonic_clock(void)
{
#ifdef CLOCK_MONOTONIC
    struct timespec ts;
    int r;

    /* Linux 2.6.28 adds CLOCK_MONOTONIC_RAW but we don't use it
     * because it's not available through timerfd */
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

static int kernel_version_ge(int major, int minor, int sublevel)
{
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

    /* kmajor == major */
    if (atoms < 2)
        return 0 == minor && 0 == sublevel;
    if (kminor > minor)
        return 1;
    if (kminor < minor)
        return 0;

    /* kminor == minor */
    if (atoms < 3)
        return 0 == sublevel;

    return ksublevel >= sublevel;
}

static int sysfs_has_file(const char *dirname, const char *filename)
{
    struct stat statbuf;
    char path[PATH_MAX];
    int r;

    snprintf(path, PATH_MAX, "%s/%s/%s", SYSFS_DEVICE_PATH, dirname, filename);
    r = stat(path, &statbuf);
    if (r == 0 && S_ISREG(statbuf.st_mode))
        return 1;

    return 0;
}

int op_init(struct libusb_context *ctx){
    struct stat statbuf;
    int r;

    usbfs_path = (char *) find_usbfs_path();
    if (!usbfs_path) {
        return -1;
    }

    if (monotonic_clkid == -1)
        monotonic_clkid = find_monotonic_clock();

    if (supports_flag_bulk_continuation == -1) {
        /* bulk continuation URB flag available from Linux 2.6.32 */
        supports_flag_bulk_continuation = kernel_version_ge(2,6,32);
        if (supports_flag_bulk_continuation == -1) {
            return LIBUSB_ERROR_OTHER;
        }
    }

    if (supports_flag_bulk_continuation)

    if (-1 == supports_flag_zero_packet) {
        /* zero length packet URB flag fixed since Linux 2.6.31 */
        supports_flag_zero_packet = kernel_version_ge(2,6,31);
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

        /* Make sure sysfs supports all the required files. If it
         * does not, then usbfs will be used instead.  Determine
         * this by looping through the directories in
         * SYSFS_DEVICE_PATH.  With the assumption that there will
         * always be subdirectories of the name usbN (usb1, usb2,
         * etc) representing the root hubs, check the usbN
         * subdirectories to see if they have all the needed files.
         * This algorithm uses the usbN subdirectories (root hubs)
         * because a device disconnection will cause a race
         * condition regarding which files are available, sometimes
         * causing an incorrect result.  The root hubs are used
         * because it is assumed that they will always be present.
         * See the "sysfs vs usbfs" comment at the top of this file
         * for more details.  */
        while ((entry = readdir(devices))) {
            int has_busnum=0, has_devnum=0, has_descriptors=0;
            int has_configuration_value=0;

            /* Only check the usbN directories. */
            if (strncmp(entry->d_name, "usb", 3) != 0)
                continue;

            /* Check for the files libusb needs from sysfs. */
            has_busnum = sysfs_has_file(entry->d_name, "busnum");
            has_devnum = sysfs_has_file(entry->d_name, "devnum");
            has_descriptors = sysfs_has_file(entry->d_name, "descriptors");
            has_configuration_value = sysfs_has_file(entry->d_name, "bConfigurationValue");

            if (has_busnum && has_devnum && has_configuration_value)
                sysfs_can_relate_devices = 1;
            if (has_descriptors)
                sysfs_has_descriptors = 1;

            /* Only need to check until we've found ONE device which
               has all the attributes. */
            if (sysfs_has_descriptors && sysfs_can_relate_devices)
                break;
        }
        closedir(devices);

        /* Only use sysfs descriptors if the rest of
           sysfs will work for libusb. */
        if (!sysfs_can_relate_devices)
            sysfs_has_descriptors = 0;
    } else {
        sysfs_has_descriptors = 0;
        sysfs_can_relate_devices = 0;
    }


    return 0;
}

int op_exit(){
    return 0;
}

int op_claim_interface(libusb_device_handle *handle, int iface){
    int fd = _device_handle_priv(handle)->fd;
    int r = ioctl(fd, IOCTL_USBFS_CLAIMINTF, &iface);
    if (r) {
        if (errno == ENOENT)
            return LIBUSB_ERROR_NOT_FOUND;
        else if (errno == EBUSY)
            return LIBUSB_ERROR_BUSY;
        else if (errno == ENODEV)
            return LIBUSB_ERROR_NO_DEVICE;

        usbi_err(HANDLE_CTX(handle),
                 "claim interface failed, error %d errno %d", r, errno);
        return LIBUSB_ERROR_OTHER;
    }
    return 0;
}