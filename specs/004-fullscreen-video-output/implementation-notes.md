# Implementation Notes: Fullscreen Video Output

## External prerequisite: feature 003 configuration

T001 remains incomplete. The required feature 003 implementation is not present
in this checkout:

- `specs/003-frontend-json-config/` contains `spec.md` and its requirements
  checklist, but no implementation mapping or implementation notes.
- `src/runner.c` still uses the existing data-directory interface and has no
  `--config PATH` selection or JSON configuration loading.
- `src/config.c` and `src/config.h` do not exist. No shared configuration
  implementation is available to inspect for default creation, validation,
  independent BIOS paths, profile/visual/save settings, or fixed policies.

The required implementation mapping is therefore unavailable. Do not add a
second configuration loader as part of feature 004. Foundation and integrated
video work remain gated on feature 003 being implemented and reviewed.

## Setup work

T002–T004 are complete. Core preparation now uses only a pinned local checkout
or cache, SDL2 discovery uses an installed pkg-config target with cross-sysroot
isolation, and setup guidance leaves package installation to the user.

Validation completed: `tests/test_prepare_core_offline.py` proves missing local
Lua inputs fail without any `git clone`; the existing device diagnostic tests
pass; shell syntax and ShellCheck pass; an isolated CMake configure resolved
installed SDL2 2.32.10 and exposed `SDL2::SDL2`; and `scripts/setup-ubuntu.sh
--cross --dry-run` printed manual host/sysroot guidance without running package
installation. No full project configure or build was run because the feature
003 configuration prerequisite is absent and the configure graph also fetches
the pinned zlib archive.
