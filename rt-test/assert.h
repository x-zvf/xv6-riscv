
/*! \file assert.h
 * \brief implement user space assertions
 */

#ifndef INCLUDED_rt_test_assert_h
#define INCLUDED_rt_test_assert_h

#include "user/user.h"

#ifdef __cplusplus
extern "C" {
#endif

#define assert(assertion)                                                                          \
  do {                                                                                             \
    if (!(assertion)) {                                                                            \
      printf(                                                                                      \
        "\033[4;31m"                                                                               \
        "assertion [" #assertion "] failed on [%s@" __FILE__                                       \
        ":%d]\n"                                                                                   \
        "\033[0m",                                                                                 \
        __func__, __LINE__);                                                                       \
      exit(1);                                                                                     \
    }                                                                                              \
  } while (false)


#ifdef __cplusplus
}
#endif

#endif
