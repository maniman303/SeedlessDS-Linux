#ifndef SEEDLESSDS_LINUX_COMPAT_H
#define SEEDLESSDS_LINUX_COMPAT_H
#include <setjmp.h>
#include <stddef.h>
#include <seedlessds/platform.h>
extern jmp_buf seedlessds_linux_fatal_jump;
const nds_platform_t *seedless_linux_platform(void);
void seedless_linux_fatal_longjmp(int value);
#endif
