#include "user/asan.h"
#include "user/user.h"

int __asan_option_detect_stack_use_after_return;

void __asan_version_mismatch_check_v8() {
  // NOOP
}

void __asan_handle_no_return() {
  // printf("__asan_handle_no_return\n");
}

void __asan_register_globals(struct __asan_global *globals, uptr n) {
  // printf("__asan_register_globals\n");
}

void __asan_unregister_globals(struct __asan_global *globals, uptr n) {
  // printf("__asan_unregister_globals\n");
}

void PoisonAlignedStackMemory(uptr addr, uptr size, bool setPoisoned) {
  if (setPoisoned)
    printf("Posoning %d bytes of memory @%d.\n", size, addr);
  else
    printf("Unpoisoning %d bytes of memory @%d.\n", size, addr);
}

void __asan_poison_stack_memory(uptr addr, uptr size) {
  // printf("__asan_poison_stack_memory\n");
  PoisonAlignedStackMemory(addr, size, true);
}
void __asan_unpoison_stack_memory(uptr addr, uptr size) {
  // printf("__asan_unpoison_stack_memory\n");
  PoisonAlignedStackMemory(addr, size, false);
}

#define U_DEFINE_STACK_MALLOC_FREE_WITH_CLASS_ID(class_id)                                         \
  ASAN_INTERFACE_ATTRIBUTE uptr __asan_stack_malloc_##class_id(uptr size) {                        \
    /*return OnMalloc(class_id, size);*/                                                           \
    return 0;                                                                                      \
  }                                                                                                \
  ASAN_INTERFACE_ATTRIBUTE void __asan_stack_free_##class_id(uptr ptr, uptr size) {                \
    /*OnFree(ptr, class_id, size);*/                                                               \
  }

U_DEFINE_STACK_MALLOC_FREE_WITH_CLASS_ID(0)
U_DEFINE_STACK_MALLOC_FREE_WITH_CLASS_ID(1)
U_DEFINE_STACK_MALLOC_FREE_WITH_CLASS_ID(2)
U_DEFINE_STACK_MALLOC_FREE_WITH_CLASS_ID(3)
U_DEFINE_STACK_MALLOC_FREE_WITH_CLASS_ID(4)
U_DEFINE_STACK_MALLOC_FREE_WITH_CLASS_ID(7)

void ReportGenericError(uptr pc, uptr bp, uptr sp, uptr addr, bool is_write, uptr access_size, bool fatal) {
  if (fatal) { asm volatile("li a7, %0" : : "i"(101) : "a7", "memory"); }
  /* Do something later */
}

#define ASAN_U_REPORT_ERROR(type, is_write, size)                                                  \
  NOINLINE ASAN_INTERFACE_ATTRIBUTE void __asan_report_##type##size(uptr addr) {                   \
    GET_CALLER_PC_BP_SP;                                                                           \
    ReportGenericError(pc, bp, sp, addr, is_write, size, true);                                    \
    /* printf("__asan_report_" #type #size "\n"); */                                               \
  }

ASAN_U_REPORT_ERROR(load, false, 1)
ASAN_U_REPORT_ERROR(load, false, 2)
ASAN_U_REPORT_ERROR(load, false, 4)
ASAN_U_REPORT_ERROR(load, false, 8)
ASAN_U_REPORT_ERROR(store, true, 1)
ASAN_U_REPORT_ERROR(store, true, 2)
ASAN_U_REPORT_ERROR(store, true, 4)
ASAN_U_REPORT_ERROR(store, true, 8)

#define ASAN_U_REPORT_ERROR_N(type, is_write)                                                      \
  NOINLINE ASAN_INTERFACE_ATTRIBUTE void __asan_report_##type##_n(uptr addr, uptr size) {          \
    GET_CALLER_PC_BP_SP;                                                                           \
    ReportGenericError(pc, bp, sp, addr, is_write, size, true);                                    \
  }

ASAN_U_REPORT_ERROR_N(load, false)
ASAN_U_REPORT_ERROR_N(store, true)

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
    case 14: __asan_poison_stack_memory(0, 0); break;
    case 15: __asan_unpoison_stack_memory(0, 0); break;
  }
  // clang-format on
}

void __asan_init() {
  __asan_option_detect_stack_use_after_return = 0;
  force_interface_symbols();
  // printf("__asan_init\n");
}