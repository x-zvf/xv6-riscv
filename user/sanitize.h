#ifndef INCLUDED_user_sanitize_h
#define INCLUDED_user_sanitize_h

void __asan_init(uint64);


void __asan_version_mismatch_check_v8(uint64);


void __asan_register_globals(uint64);

void __asan_unregister_globals(uint64);

void __asan_handle_no_return(uint64);

void __asan_option_detect_stack_use_after_return(uint64);

#define ADDRESS_SANITIZER_LOAD_STORE(size)                                                         \
  void __asan_report_load##size(uint64);                                                           \
  void __asan_report_store##size(uint64);                                                          \
  void __asan_stack_malloc_##size(uint64);                                                         \
  void __asan_stack_free_##size(uint64);

ADDRESS_SANITIZER_LOAD_STORE(0);
ADDRESS_SANITIZER_LOAD_STORE(1);
ADDRESS_SANITIZER_LOAD_STORE(2);
ADDRESS_SANITIZER_LOAD_STORE(3);
ADDRESS_SANITIZER_LOAD_STORE(4);
ADDRESS_SANITIZER_LOAD_STORE(8);


#define ASAN_REPORT_ERROR_N(type, is_write) void __asan_report_##type##_n(uint64, uint64);

ASAN_REPORT_ERROR_N(load, false);
ASAN_REPORT_ERROR_N(store, true);

#endif
