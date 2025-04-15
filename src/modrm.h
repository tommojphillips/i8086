/* modrm.h
 * tommojphillips 2025 ( https://github.com/tommojphillips/ )
 * MOD RM
 */

#ifndef MODRM_H
#define MODRM_H

#include <stdint.h>

#include "i8086.h"

#ifdef __cplusplus
extern "C" {
#endif

uint8_t* modrm_get_ptr8(I8086* cpu);
uint16_t* modrm_get_ptr16(I8086* cpu);

#ifdef __cplusplus
};
#endif

#endif
