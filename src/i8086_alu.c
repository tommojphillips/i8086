/* alu.c
 * Thomas J. Armytage 2025 ( https://github.com/tommojphillips/ )
 * Intel 8086 Arithmetic Logic Unit
 */

#include <stdint.h>

#include "i8086.h"
#include "i8086_alu.h"

#define SF cpu->status.sf
#define CF cpu->status.cf
#define ZF cpu->status.zf
#define PF cpu->status.pf
#define OF cpu->status.of
#define AF cpu->status.af
#define DF cpu->status.df

#define CLR_CF() CF = 0
#define CLR_OF() OF = 0
#define CLR_SF() SF = 0
#define CLR_PF() PF = 0
#define CLR_ZF() ZF = 0
#define CLR_AF() AF = 0

#define SET_ZF(r) ZF = (r) == 0

#define SET_CF8(r) CF = (r) > 0xFF
#define SET_OF8(r) OF = (r) > 0x7F
#define SET_SF8(r) SF = ((r) & 0x80) >> 7
#define SET_ZF8(r) SET_ZF(r)

#define SET_PF8(r) { \
	uint8_t pf = (r);\
	pf ^= (pf >> 4); \
	pf ^= (pf >> 2); \
	pf ^= (pf >> 1); \
	PF = (~pf & 1) ^ 1; }

#define SET_AF_ADD8(x,y,r) AF = (((x) ^ (y) ^ (r)) & 0x10) == 0x10
#define SET_AF_SUB8(x,y,r) AF = (((x) ^ (y) ^ (r)) & 0x10) == 0

#define SET_CF16(r) CF = (r) > 0xFFFF
#define SET_OF16(r) OF = (r) > 0x7FFF
#define SET_SF16(r) SF = ((r) & 0x8000) >> 15
#define SET_ZF16(r) SET_ZF(r)

/* 16bit PF only checks LSB */
#define SET_PF16(r) SET_PF8(r & 0xFF)

#define SET_AF_ADD16(x,y,r) AF = (((x) ^ (y) ^ (r)) & 0x100) == 0x100
#define SET_AF_SUB16(x,y,r) AF = (((x) ^ (y) ^ (r)) & 0x100) == 0

void alu_daa(I8086* cpu, uint8_t* x1) {
	uint8_t correction = 0;
	if ((*x1 & 0x0F) > 9 || AF) {
		correction |= 6;
		AF = 1;
	}

	if (*x1 > 0x9F || CF) {
		correction |= 0x60;
		CF = 1;
	}

	alu_add8(cpu, x1, correction);
}
void alu_das(I8086* cpu, uint8_t* x1) {
	uint8_t correction = 0;
	if ((*x1 & 0x0F) > 9 || AF) {
		correction |= 6;
		AF = 1;
	}

	if (*x1 > 0x9F || CF) {
		correction |= 0x60;
		CF = 1;
	}

	alu_sub8(cpu, x1, correction);
}
void alu_aaa(I8086* cpu, uint8_t* l, uint8_t* h) {
	*l &= 0x0F;
	if (*l > 9 || AF) {
		*l += 6;
		*h += 1;
		AF = 1;
		CF = 1;
	}
	else {
		AF = 0;
		CF = 0;
	}
}
void alu_aas(I8086* cpu, uint8_t* l, uint8_t* h) {
	*l &= 0x0F;
	if (*l > 9 || AF) {
		*l -= 6;
		*h -= 1;
		AF = 1;
		CF = 1;
	}
	else {
		AF = 0;
		CF = 0;
	}
}
void alu_aad(I8086* cpu, uint8_t* l, uint8_t* h, uint8_t divisor) {
	*h = (*l / divisor);
	*l = (*l % divisor);
	SET_CF8(*h);
	SET_OF8(*h);
	SET_PF8(*l);
	SET_SF8(*l);
	SET_ZF8(*l);
}
void alu_aam(I8086* cpu, uint8_t* l, uint8_t* h, uint8_t divisor) {	
	*l = (*h * divisor) + *l;
	*h = 0;
	SET_CF8(*l);
	SET_OF8(*l);
	SET_PF8(*l);
	SET_SF8(*l);
	SET_ZF8(*l);
}

/* 8bit alu */

/*uint8_t alu_cal_parity8(uint8_t value) {
	value ^= value >> 4;
	value ^= value >> 2;
	value ^= value >> 1;
	return (~value & 1) ^ 1;
}*/

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

void alu_cmp8(I8086* cpu, uint8_t  x1, uint8_t x2) {
	uint8_t tmp = x1;
	alu_sub8(cpu, &tmp, x2);
}
void alu_test8(I8086* cpu, uint8_t x1, uint8_t x2) {
	uint8_t tmp = x1;
	alu_and8(cpu, &tmp, x2);
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

/* 16bit alu */

/*uint8_t alu_cal_parity16(uint16_t value) {
	value ^= value >> 8;
	value ^= value >> 4;
	value ^= value >> 2;
	value ^= value >> 1;
	return (~value & 1) ^ 1;
}*/

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
