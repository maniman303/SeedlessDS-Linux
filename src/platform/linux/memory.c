#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include "compat.h"

jmp_buf seedlessds_linux_fatal_jump;

int32_t seedless_linux_shared_region_create(const char *name, uint32_t size)
{
    if (size == 0) { errno = EINVAL; return -1; }
    int fd = memfd_create(name && *name ? name : "seedlessds-shared", MFD_CLOEXEC);
    if (fd < 0) return -1;
    if (ftruncate(fd, (off_t)size) != 0) {
        int saved = errno;
        close(fd);
        errno = saved;
        return -1;
    }
    return fd;
}
