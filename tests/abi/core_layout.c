#include <pthread.h>
#include <setjmp.h>
#include <stddef.h>
#include <sys/stat.h>
#include "core/nds_state.h"
#include "frontend/video_out_gl.h"

_Static_assert(sizeof(jmp_buf) <= 1024, "target jmp_buf exceeds the adapter-owned storage policy");
_Static_assert(sizeof(((nds_t *)0)->fatal_jump) == 0x100, "upstream machine layout changed");
_Static_assert(sizeof(nds_t) == NDS_MACHINE_SIZE, "fixed core machine mapping size changed");
_Static_assert(_Alignof(nds_t) >= 0x4000, "core machine must remain 16 KiB aligned");
_Static_assert(sizeof(((video_out_gl_t *)0)->mutex) == 40,
               "core mutex opaque key storage changed; review Linux registry mapping");
_Static_assert(sizeof(((video_out_gl_t *)0)->cond) == 48,
               "core condition opaque key storage changed; review Linux registry mapping");
_Static_assert(sizeof(struct stat) <= 0x80, "legacy stat buffer is too small");
_Static_assert(offsetof(struct stat, st_mtim) == 88,
               "legacy mtime consumer requires stat.st_mtim at offset 88");
