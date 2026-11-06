// Host stand-in for the Pico SDK's pico.h: the section/inlining attributes the
// firmware headers use, as no-ops (or plain inline) off-target.
#pragma once
#include <cstring>
#include <cstdint>

#define __not_in_flash(group)
#define __not_in_flash_func(func_name) func_name
#ifndef __force_inline
#define __force_inline inline __attribute__((always_inline))
#endif
