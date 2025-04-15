/* i8086_mnem.h
 * tommojphillips 2025 ( https://github.com/tommojphillips/ )
 * Intel 8086 Mnemonics
 */

#ifndef I8086_MNEM_H
#define I8086_MNEM_H

#include <stdint.h>

#include "i8086.h"

/* I8086 CPU State */
typedef struct I8086_MNEM {
	char mnem[32];
	int index;
	char addressing_str[32];
	uint16_t counter;
	I8086* state;

	uint8_t opcode;
	uint8_t segment_prefix;
	uint8_t rep_prefix;
	I8086_MOD_RM modrm;

} I8086_MNEM;

#ifdef __cplusplus
extern "C" {
#endif

int i8086_mnem(I8086_MNEM* cpu);

#ifdef __cplusplus
};
#endif
#endif
