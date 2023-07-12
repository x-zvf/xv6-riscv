/*! \file asan.h
 * \brief asan structure
 */
#include "defs.h"

#ifndef ASAN_H
#define ASAN_H

#define uptr uint64
#define u32 uint32
#define __asan_version_mismatch_check __asan_version_mismatch_check_v8

#ifdef __cplusplus
extern "C" {
#endif

// This function should be called at the very beginning of the process,
// before any instrumented code is executed and before any call to malloc.
void __asan_init();

// This function exists purely to get a linker/loader error when using
// incompatible versions of instrumentation and runtime library. Please note
// that __asan_version_mismatch_check is a macro that is replaced with
// __asan_version_mismatch_check_vXXX at compile-time.
void __asan_version_mismatch_check();

// This structure is used to describe the source location of a place where
// global was defined.
struct __asan_global_source_location {
  const char *filename;
  int line_no;
  int column_no;
};

// This structure describes an instrumented global variable.
struct __asan_global {
  uptr beg;                                // The address of the global.
  uptr size;                               // The original size of the global.
  uptr size_with_redzone;                  // The size with the redzone.
  const char *name;                        // Name as a C string.
  const char *module_name;                 // Module name as a C string. This pointer is a
                                           // unique identifier of a module.
  uptr has_dynamic_init;                   // Non-zero if the global has dynamic initializer.
  __asan_global_source_location *location; // Source location of a global,
                                           // or NULL if it is unknown.
  uptr odr_indicator;                      // The address of the ODR indicator symbol.
};

// These functions can be called on some platforms to find globals in the same
// loaded image as `flag' and apply __asan_(un)register_globals to them,
// filtering out redundant calls.

void __asan_register_image_globals(uptr *flag);

void __asan_unregister_image_globals(uptr *flag);

// These two functions should be called by the instrumented code.
// 'globals' is an array of structures describing 'n' globals.

void __asan_register_globals(__asan_global *globals, uptr n);

void __asan_unregister_globals(__asan_global *globals, uptr n);

// These two functions should be called before and after dynamic initializers
// of a single module run, respectively.

void __asan_before_dynamic_init(const char *module_name);

void __asan_after_dynamic_init();

// Sets bytes of the given range of the shadow memory into specific value.

void __asan_set_shadow_00(uptr addr, uptr size);

void __asan_set_shadow_f1(uptr addr, uptr size);

void __asan_set_shadow_f2(uptr addr, uptr size);

void __asan_set_shadow_f3(uptr addr, uptr size);

void __asan_set_shadow_f5(uptr addr, uptr size);

void __asan_set_shadow_f8(uptr addr, uptr size);

// These two functions are used by instrumented code in the
// use-after-scope mode. They mark memory for local variables as
// unaddressable when they leave scope and addressable before the
// function exits.

void __asan_poison_stack_memory(uptr addr, uptr size);

void __asan_unpoison_stack_memory(uptr addr, uptr size);

// Performs cleanup before a NoReturn function. Must be called before things
// like _exit and execl to avoid false positives on stack.
void __asan_handle_no_return();


void __asan_poison_memory_region(void const volatile *addr, uptr size);

void __asan_unpoison_memory_region(void const volatile *addr, uptr size);


int __asan_address_is_poisoned(void const volatile *addr);


uptr __asan_region_is_poisoned(uptr beg, uptr size);


void __asan_describe_address(uptr addr);


int __asan_report_present();


uptr __asan_get_report_pc();

uptr __asan_get_report_bp();

uptr __asan_get_report_sp();

uptr __asan_get_report_address();

int __asan_get_report_access_type();

uptr __asan_get_report_access_size();

const char *__asan_get_report_description();


const char *__asan_locate_address(uptr addr, char *name, uptr name_size, uptr *region_address, uptr *region_size);


uptr __asan_get_alloc_stack(uptr addr, uptr *trace, uptr size, u32 *thread_id);


uptr __asan_get_free_stack(uptr addr, uptr *trace, uptr size, u32 *thread_id);


void __asan_get_shadow_mapping(uptr *shadow_scale, uptr *shadow_offset);


void __asan_report_error(uptr pc, uptr bp, uptr sp, uptr addr, int is_write, uptr access_size, u32 exp);


void __asan_set_death_callback(void (*callback)(void));

void __asan_set_error_report_callback(void (*callback)(const char *));

/* OPTIONAL */ void __asan_on_error();

void __asan_print_accumulated_stats();

/* OPTIONAL */ const char *__asan_default_options();


extern uptr __asan_shadow_memory_dynamic_address;

// Global flag, copy of ASAN_OPTIONS=detect_stack_use_after_return

extern int __asan_option_detect_stack_use_after_return;


extern uptr *__asan_test_only_reported_buggy_pointer;

void __asan_load1(uptr p);
void __asan_load2(uptr p);
void __asan_load4(uptr p);
void __asan_load8(uptr p);
void __asan_load16(uptr p);
void __asan_store1(uptr p);
void __asan_store2(uptr p);
void __asan_store4(uptr p);
void __asan_store8(uptr p);
void __asan_store16(uptr p);
void __asan_loadN(uptr p, uptr size);
void __asan_storeN(uptr p, uptr size);

void __asan_load1_noabort(uptr p);
void __asan_load2_noabort(uptr p);
void __asan_load4_noabort(uptr p);
void __asan_load8_noabort(uptr p);
void __asan_load16_noabort(uptr p);
void __asan_store1_noabort(uptr p);
void __asan_store2_noabort(uptr p);
void __asan_store4_noabort(uptr p);
void __asan_store8_noabort(uptr p);
void __asan_store16_noabort(uptr p);
void __asan_loadN_noabort(uptr p, uptr size);
void __asan_storeN_noabort(uptr p, uptr size);

void __asan_exp_load1(uptr p, u32 exp);
void __asan_exp_load2(uptr p, u32 exp);
void __asan_exp_load4(uptr p, u32 exp);
void __asan_exp_load8(uptr p, u32 exp);
void __asan_exp_load16(uptr p, u32 exp);
void __asan_exp_store1(uptr p, u32 exp);
void __asan_exp_store2(uptr p, u32 exp);
void __asan_exp_store4(uptr p, u32 exp);
void __asan_exp_store8(uptr p, u32 exp);
void __asan_exp_store16(uptr p, u32 exp);
void __asan_exp_loadN(uptr p, uptr size, u32 exp);
void __asan_exp_storeN(uptr p, uptr size, u32 exp);


void *__asan_memcpy(void *dst, const void *src, uptr size);

void *__asan_memset(void *s, int c, uptr n);

void *__asan_memmove(void *dest, const void *src, uptr n);


void __asan_poison_cxx_array_cookie(uptr p);

uptr __asan_load_cxx_array_cookie(uptr *p);

void __asan_poison_intra_object_redzone(uptr p, uptr size);

void __asan_unpoison_intra_object_redzone(uptr p, uptr size);

void __asan_alloca_poison(uptr addr, uptr size);

void __asan_allocas_unpoison(uptr top, uptr bottom);

#ifdef __cplusplus
}
#endif
#endif
