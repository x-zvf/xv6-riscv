/*! \file asan.h
 * \brief asan structure
 */
#include "defs.h"
#ifndef ASAN_H
#define ASAN_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ASAN_HIDDEN
#define ASAN_INTERFACE_ATTRIBUTE __attribute__((visibility("default")))
#else
#define ASAN_INTERFACE_ATTRIBUTE
#endif

#define NOINLINE __attribute__((noinline))

struct __asan_global {};

ASAN_INTERFACE_ATTRIBUTE int __asan_option_detect_stack_use_after_return;

ASAN_INTERFACE_ATTRIBUTE void __asan_init();

ASAN_INTERFACE_ATTRIBUTE void __asan_handle_no_return();

ASAN_INTERFACE_ATTRIBUTE void __asan_register_globals(struct __asan_global *globals, uptr n);
ASAN_INTERFACE_ATTRIBUTE void __asan_unregister_globals(struct __asan_global *globals, uptr n);

ASAN_INTERFACE_ATTRIBUTE void __asan_version_mismatch_check_v8();

#ifdef __cplusplus
}
#endif

#endif
