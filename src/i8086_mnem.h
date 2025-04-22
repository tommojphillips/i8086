/* i8086_mnem.h
 * Thomas J. Armytage 2025 ( https://github.com/tommojphillips/ )
 * Intel 8086 Mnemonics
 */

#ifndef I8086_MNEM_H
#define I8086_MNEM_H

#include <stdint.h>

#include "i8086.h"

/* I8086 CPU State */
typedef struct I8086_MNEM {
	char mnem[32];
	char addressing_str[32];
	I8086* state;           // CPU state
	uint16_t counter;       // instruction counter
	uint8_t opcode;         // opcode
	uint8_t segment_prefix; // segement override index
	uint8_t rep_prefix;     // rep prefix
	I8086_MOD_RM modrm;     // modrm structure

} I8086_MNEM;

#ifdef __cplusplus
extern "C" {
#endif

int i8086_mnem(I8086_MNEM* cpu);

#ifdef __cplusplus
};
#endif
#endif
