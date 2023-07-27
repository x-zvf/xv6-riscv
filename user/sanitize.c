#include "user/user.h"

void __asan_init(uint64 address) {
  // printf("__asan_init");
}


void __asan_version_mismatch_check_v8(uint64 adress) {
  printf("__asan_version_mismatch_check_v8");
  exit(-2);
}


void __asan_register_globals(uint64 address) {
  printf("__asan_register_globals");
  exit(-2);
}

void __asan_unregister_globals(uint64 address) {
  printf("__asan_unregister_globals");
  exit(-2);
}


void __asan_handle_no_return(uint64 address) {
  printf("__asan_handle_no_return");
  exit(-2);
}

void __asan_option_detect_stack_use_after_return(uint64 address) {
  printf("__asan_option_detect_stack_use_after_return");
  exit(-2);
}

#undef ADDRESS_SANITIZER_LOAD_STORE
#define ADDRESS_SANITIZER_LOAD_STORE(size)                                                         \
  void __asan_report_load##size(uint64 address) {                                                  \
    printf("__asan_load" + size);                                                                  \
    exit(-2);                                                                                      \
  }                                                                                                \
  void __asan_report_store##size(uint64 address) {                                                 \
    printf("__asan_store" + size);                                                                 \
    exit(-2);                                                                                      \
  }                                                                                                \
  void __asan_stack_malloc_##size(uint64 address) {                                                \
    printf("__asan_store" + size);                                                                 \
    exit(-2);                                                                                      \
  }                                                                                                \
  void __asan_stack_free_##size(uint64 address) {                                                  \
    printf("__asan_store" + size);                                                                 \
    exit(-2);                                                                                      \
  }

ADDRESS_SANITIZER_LOAD_STORE(0);
ADDRESS_SANITIZER_LOAD_STORE(1);
ADDRESS_SANITIZER_LOAD_STORE(2);
ADDRESS_SANITIZER_LOAD_STORE(3);
ADDRESS_SANITIZER_LOAD_STORE(4);
ADDRESS_SANITIZER_LOAD_STORE(8);

#undef ASAN_REPORT_ERROR_N
#define ASAN_REPORT_ERROR_N(type, is_write)                                                        \
  void __asan_report_##type##_n(uint64 addr, uint64 size) {                                        \
    printf("__asan_report_##type##_n");                                                            \
    exit(-2);                                                                                      \
  }

ASAN_REPORT_ERROR_N(load, false);
ASAN_REPORT_ERROR_N(store, true);
