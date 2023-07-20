#include "defs.h"
#include "asan.h"

int __asan_option_detect_stack_use_after_return;

void ReportGenericError(uptr pc, uptr bp, uptr sp, uptr addr, bool is_write, uptr access_size, bool fatal) {
  if (fatal) { exit(-2); }
  /* Do something later */
}

#define ASAN_REPORT_ERROR(type, is_write, size)                                                    \
  NOINLINE ASAN_INTERFACE_ATTRIBUTE void __asan_report_##type##size(uptr addr) {                   \
    GET_CALLER_PC_BP_SP;                                                                           \
    ReportGenericError(pc, bp, sp, addr, is_write, size, true);                                    \
    /* Do something */                                                                             \
  }

ASAN_REPORT_ERROR(load, false, 1)
ASAN_REPORT_ERROR(load, false, 2)
ASAN_REPORT_ERROR(load, false, 4)
ASAN_REPORT_ERROR(load, false, 8)
ASAN_REPORT_ERROR(store, true, 1)
ASAN_REPORT_ERROR(store, true, 2)
ASAN_REPORT_ERROR(store, true, 4)
ASAN_REPORT_ERROR(store, true, 8)

#define ASAN_REPORT_ERROR_N(type, is_write)                                                        \
  NOINLINE ASAN_INTERFACE_ATTRIBUTE void __asan_report_##type##_n(uptr addr, uptr size) {          \
    GET_CALLER_PC_BP_SP;                                                                           \
    ReportGenericError(pc, bp, sp, addr, is_write, size, true);                                    \
  }

ASAN_REPORT_ERROR_N(load, false)
ASAN_REPORT_ERROR_N(store, true)

#define DEFINE_STACK_MALLOC_FREE_WITH_CLASS_ID(class_id)                                           \
  ASAN_INTERFACE_ATTRIBUTE uptr __asan_stack_malloc_##class_id(uptr size) {                        \
    /*return OnMalloc(class_id, size);*/                                                           \
    return 0;                                                                                      \
  }                                                                                                \
  ASAN_INTERFACE_ATTRIBUTE void __asan_stack_free_##class_id(uptr ptr, uptr size) {                \
    /*OnFree(ptr, class_id, size);*/                                                               \
  }

DEFINE_STACK_MALLOC_FREE_WITH_CLASS_ID(0)
DEFINE_STACK_MALLOC_FREE_WITH_CLASS_ID(1)
DEFINE_STACK_MALLOC_FREE_WITH_CLASS_ID(2)
DEFINE_STACK_MALLOC_FREE_WITH_CLASS_ID(3)
DEFINE_STACK_MALLOC_FREE_WITH_CLASS_ID(4)

void __asan_handle_no_return() {}

void __asan_register_globals(struct __asan_global *globals, uptr n) {}
void __asan_unregister_globals(struct __asan_global *globals, uptr n) {}

void __asan_version_mismatch_check_v8() {
  // NOOP
}

static NOINLINE void force_interface_symbols() {
  volatile int fake_condition = 0;
  // clang-format off
  switch (fake_condition) {
    case 1: __asan_report_load1(0); break;
    case 2: __asan_report_load2(0); break;
    case 3: __asan_report_load4(0); break;
    case 4: __asan_report_load8(0); break;
    case 5: __asan_report_load_n(0, 0); break;
    case 6: __asan_report_store1(0); break;
    case 7: __asan_report_store2(0); break;
    case 8: __asan_report_store4(0); break;
    case 9: __asan_report_store8(0); break;
    case 10: __asan_report_store_n(0, 0); break;
    case 11: __asan_register_globals(NULL, 0); break;
    case 12: __asan_unregister_globals(NULL, 0); break;
    case 13: __asan_handle_no_return(); break;
  }
  // clang-format on
}

void __asan_init() {
  // Currently, I can't see no way to implement this.
  __asan_option_detect_stack_use_after_return = 0;
  force_interface_symbols();
}
