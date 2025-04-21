/* alu.c
 * tommojphillips 2025 ( https://github.com/tommojphillips/ )
 * Intel 8086 Arithmetic Logic Unit
 */

#include <stdint.h>

#include "i8086.h"
#include "alu.h"

/* 8bit alu */

uint8_t alu_cal_parity8(uint8_t value) {
	value ^= value >> 4;
	value ^= value >> 2;
	value ^= value >> 1;
	return (~value & 1) ^ 1;
}

void alu_add8(I8086* cpu, uint8_t* x1, uint8_t x2) {
	uint16_t tmp = (*x1 + x2);
	SET_AF_ADD8(*x1, x2, tmp);
	SET_CF8(tmp);
	*x1 = (tmp & 0xFF);
	SET_OF8(*x1);
	SET_SF8(*x1);
	SET_PF8(*x1);
	SET_ZF8(*x1);
}
void alu_adc8(I8086* cpu, uint8_t* x1, uint8_t x2) {
	uint16_t tmp = (*x1 + x2 + CF);
	SET_AF_ADD8(*x1, x2, tmp);
	SET_CF8(tmp);
	*x1 = (tmp & 0xFF);
	SET_OF8(*x1);
	SET_SF8(*x1);
	SET_PF8(*x1);
	SET_ZF8(*x1);
}
void alu_sub8(I8086* cpu, uint8_t* x1, uint8_t x2) {
	uint16_t tmp = (*x1 - x2);
	SET_AF_SUB8(*x1, x2, tmp);
	SET_CF8(tmp);
	*x1 = (tmp & 0xFF);
	SET_OF8(*x1);
	SET_SF8(*x1);
	SET_PF8(*x1);
	SET_ZF8(*x1);
}
void alu_sbb8(I8086* cpu, uint8_t* x1, uint8_t x2) {
	uint16_t tmp = (*x1 - x2 - CF);
	SET_AF_SUB8(*x1, x2, tmp);
	SET_CF8(tmp);
	*x1 = (tmp & 0xFF);
	SET_OF8(*x1);
	SET_SF8(*x1);
	SET_PF8(*x1);
	SET_ZF8(*x1);
}
void alu_inc8(I8086* cpu, uint8_t* x1) {
	uint16_t tmp = (*x1 + 1);
	SET_AF_ADD8(*x1, 1, tmp);
	*x1 = (tmp & 0xFF);
	SET_OF8(*x1);
	SET_SF8(*x1);
	SET_PF8(*x1);
	SET_ZF8(*x1);
}
void alu_dec8(I8086* cpu, uint8_t* x1) {
	uint16_t tmp = (*x1 - 1);
	SET_AF_SUB8(*x1, 1, tmp);
	*x1 = (tmp & 0xFF);
	SET_OF8(*x1);
	SET_SF8(*x1);
	SET_PF8(*x1);
	SET_ZF8(*x1);
}

void alu_cmp8(I8086* cpu, uint8_t  x1, uint8_t x2) {
	uint8_t tmp = x1;
	alu_sub8(cpu, &tmp, x2);
}
void alu_test8(I8086* cpu, uint8_t x1, uint8_t x2) {
	uint8_t tmp = x1;
	alu_and8(cpu, &tmp, x2);
}

void alu_and8(I8086* cpu, uint8_t* x1, uint8_t x2) {
	*x1 &= x2;
	CLR_CF();
	CLR_OF();
	SET_SF8(*x1);
	SET_PF8(*x1);
	SET_ZF8(*x1);
}
void alu_xor8(I8086* cpu, uint8_t* x1, uint8_t x2) {
	*x1 ^= x2;
	CLR_CF();
	CLR_OF();
	SET_SF8(*x1);
	SET_PF8(*x1);
	SET_ZF8(*x1);
}
void alu_or8(I8086* cpu, uint8_t* x1, uint8_t x2) {
	*x1 |= x2;
	CLR_CF();
	CLR_OF();
	SET_SF8(*x1);
	SET_PF8(*x1);
	SET_ZF8(*x1);
}
void alu_rcl8(I8086* cpu, uint8_t* x1, uint8_t count) {
	for (int i = 0; i < count; ++i) {
		uint8_t cf = CF;
		CF = (*x1 >> 7) & 1;
		OF = CF != ((*x1 >> 6) & 1);
		*x1 = (*x1 << 1) | cf;
	}
}
void alu_rcr8(I8086* cpu, uint8_t* x1, uint8_t count) {
	for (int i = 0; i < count; ++i) {
		uint8_t cf = CF;
		CF = (*x1 & 1);
		OF = CF != ((*x1 >> 7) & 1);
		*x1 = (*x1 >> 1) | (cf << 7);
	}
}
void alu_rol8(I8086* cpu, uint8_t* x1, uint8_t count) {
	for (int i = 0; i < count; ++i) {
		uint8_t cf = (*x1 >> 7) & 1;
		CF = cf;
		*x1 = (*x1 << 1) | cf;
		// OF ?
	}
}
void alu_ror8(I8086* cpu, uint8_t* x1, uint8_t count) {
	for (int i = 0; i < count; ++i) {
		uint8_t cf = (*x1 & 1);
		CF = cf;
		*x1 = (*x1 >> 1) | (cf << 7);
		// OF ?
	}
}
void alu_shl8(I8086* cpu, uint8_t* x1, uint8_t count) {
	if (count > 0) {
		for (int i = 0; i < count; ++i) {
			CF = (*x1 >> 7) & 1;
			OF = CF != ((*x1 >> 6) & 1);
			*x1 <<= 1;
		}
		SET_SF8(*x1);
		SET_PF8(*x1);
		SET_ZF8(*x1);
	}
}
void alu_shr8(I8086* cpu, uint8_t* x1, uint8_t count) {
	if (count > 0) {
		for (int i = 0; i < count; ++i) {
			CF = (*x1 & 1);
			OF = (*x1 >> 7) & 1;
			*x1 >>= 1;
		}
		SET_SF8(*x1);
		SET_PF8(*x1);
		SET_ZF8(*x1);
	}
}
void alu_sar8(I8086* cpu, uint8_t* x1, uint8_t count) {
	if (count > 0) {
		for (int i = 0; i < count; ++i) {
			CF = (*x1 & 1);
			uint8_t ms = (*x1 & 7);
			*x1 >>= 1;
			*x1 |= ms;
		}
		SET_SF8(*x1);
		SET_PF8(*x1);
		SET_ZF8(*x1);
		CLR_OF();
	}
}

/* 16bit alu */

uint8_t alu_cal_parity16(uint16_t value) {
	value ^= value >> 8;
	value ^= value >> 4;
	value ^= value >> 2;
	value ^= value >> 1;
	return (~value & 1) ^ 1;
}

void alu_and16(I8086* cpu, uint16_t* x1, uint16_t x2) {
	*x1 &= x2;
	CLR_CF();
	CLR_OF();
	SET_SF16(*x1);
	SET_PF16(*x1);
	SET_ZF16(*x1);
}
void alu_xor16(I8086* cpu, uint16_t* x1, uint16_t x2) {
	*x1 ^= x2;
	CLR_CF();
	CLR_OF();
	SET_SF16(*x1);
	SET_PF16(*x1);
	SET_ZF16(*x1);
}
void alu_or16(I8086*  cpu, uint16_t* x1, uint16_t x2) {
	*x1 |= x2;
	CLR_CF();
	CLR_OF();
	SET_SF16(*x1);
	SET_PF16(*x1);
	SET_ZF16(*x1);
}
void alu_add16(I8086* cpu, uint16_t* x1, uint16_t x2) {
	uint32_t tmp = (*x1 + x2);
	SET_AF_ADD16(*x1, x2, tmp);
	SET_CF16(tmp);
	*x1 = (tmp & 0xFFFF);
	SET_OF16(*x1);
	SET_SF16(*x1);
	SET_PF16(*x1);
	SET_ZF16(*x1);
}
void alu_adc16(I8086* cpu, uint16_t* x1, uint16_t x2) {
	uint32_t tmp = (*x1 + x2 + CF);
	SET_AF_ADD16(*x1, x2, tmp);
	SET_CF16(tmp);
	*x1 = (tmp & 0xFFFF);
	SET_OF16(*x1);
	SET_SF16(*x1);
	SET_PF16(*x1);
	SET_ZF16(*x1);
}
void alu_sub16(I8086* cpu, uint16_t* x1, uint16_t x2) {
	uint32_t tmp = (*x1 - x2);
	SET_AF_SUB16(*x1, x2, tmp);
	SET_CF16(tmp);
	*x1 = (tmp & 0xFFFF);
	SET_OF16(*x1);
	SET_SF16(*x1);
	SET_PF16(*x1);
	SET_ZF16(*x1);
}
void alu_sbb16(I8086* cpu, uint16_t* x1, uint16_t x2) {
	uint32_t tmp = (*x1 - x2 - CF);
	SET_AF_SUB16(*x1, x2, tmp);
	SET_CF16(tmp);
	*x1 = (tmp & 0xFFFF);
	SET_OF16(*x1);
	SET_SF16(*x1);
	SET_PF16(*x1);
	SET_ZF16(*x1);
}
void alu_cmp16(I8086* cpu, uint16_t  x1, uint16_t x2) {
	alu_sub16(cpu, &x1, x2);
}
void alu_test16(I8086* cpu, uint16_t x1, uint16_t x2) {
	alu_and16(cpu, &x1, x2);
}
void alu_inc16(I8086* cpu, uint16_t* x1) {
	uint32_t tmp = (*x1 + 1);
	SET_AF_ADD16(*x1, 1, tmp);	
	*x1 = (tmp & 0xFFFF);
	SET_OF16(*x1);
	SET_SF16(*x1);
	SET_PF16(*x1);
	SET_ZF16(*x1);
}
void alu_dec16(I8086* cpu, uint16_t* x1) {
	uint32_t tmp = (*x1 - 1);
	SET_AF_SUB16(*x1, 1, tmp);
	*x1 = (tmp & 0xFFFF);
	SET_OF16(*x1);
	SET_SF16(*x1);
	SET_PF16(*x1);
	SET_ZF16(*x1);
}
void alu_rcl16(I8086* cpu, uint16_t* x1, uint8_t count) {
	for (int i = 0; i < count; ++i) {
		uint8_t cf = CF;
		CF = (*x1 >> 15) & 1;
		OF = CF != ((*x1 >> 14) & 1);
		*x1 = (*x1 << 1) | cf;
	}
}
void alu_rcr16(I8086* cpu, uint16_t* x1, uint8_t count) {
	for (int i = 0; i < count; ++i) {
		uint8_t cf = CF;
		CF = (*x1 & 1);
		OF = CF != ((*x1 >> 15) & 1);
		*x1 = (*x1 >> 1) | (cf << 15);
	}
}
void alu_rol16(I8086* cpu, uint16_t* x1, uint8_t count) {
	for (int i = 0; i < count; ++i) {
		uint8_t cf = (*x1 >> 15) & 1;
		CF = cf;
		*x1 = (*x1 << 1) | cf;
		// OF ?
	}
}
void alu_ror16(I8086* cpu, uint16_t* x1, uint8_t count) {
	for (int i = 0; i < count; ++i) {
		uint8_t cf = (*x1 & 1);
		CF = cf;
		*x1 = (*x1 >> 1) | (cf << 15);
		// OF ?
	}
}
void alu_shl16(I8086* cpu, uint16_t* x1, uint8_t count) {
	if (count > 0) {
		for (int i = 0; i < count; ++i) {
			CF = (*x1 >> 15) & 1;
			OF = CF == ((*x1 >> 14) & 1);
			*x1 <<= 1;
		}
		SET_SF16(*x1);
		SET_PF16(*x1);
		SET_ZF16(*x1);
	}
}
void alu_shr16(I8086* cpu, uint16_t* x1, uint8_t count) {
	if (count > 0) {
		for (int i = 0; i < count; ++i) {
			CF = (*x1 & 1);
			OF = (*x1 >> 15) & 1;
			*x1 >>= 1;
		}
		SET_SF16(*x1);
		SET_PF16(*x1);
		SET_ZF16(*x1);
	}
}
void alu_sar16(I8086* cpu, uint16_t* x1, uint8_t count) {
	if (count > 0) {
		for (int i = 0; i < count; ++i) {
			CF = (*x1 & 1);
			uint8_t ms = (*x1 & 15);
			*x1 >>= 1;
			*x1 |= ms;
		}
		SET_SF16(*x1);
		SET_PF16(*x1);
		SET_ZF16(*x1);
		CLR_OF();
	}
}
