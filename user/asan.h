/*! \file asan.h
 *  \brief asan structure
 */

#ifndef ASAN_USER_H
#define ASAN_USER_H

#include "kernel/asan.h"

ASAN_INTERFACE_ATTRIBUTE void __asan_poison_stack_memory(uptr addr, uptr size);
ASAN_INTERFACE_ATTRIBUTE void __asan_unpoison_stack_memory(uptr addr, uptr size);
#endif
