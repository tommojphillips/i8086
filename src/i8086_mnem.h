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
	char str[32];
	char addressing_str[32];
	I8086 const* state;     // CPU state
	uint16_t counter;       // instruction length
	uint16_t offset;		// IP
	uint16_t segment;		// SEGMENT
	uint8_t opcode;         // opcode
	uint8_t segment_prefix; // segement override index
	uint8_t rep_prefix;     // rep prefix
	I8086_MOD_RM modrm;     // modrm structure

} I8086_MNEM;

#ifdef __cplusplus
extern "C" {
#endif

int i8086_mnem(I8086_MNEM* cpu);
int i8086_mnem_at(I8086_MNEM* mnem, uint16_t cs, uint16_t ip);

#ifdef __cplusplus
};
#endif
#endif
