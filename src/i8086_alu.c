/* alu.c
 * Thomas J. Armytage 2025 ( https://github.com/tommojphillips/ )
 * Intel 8086 Arithmetic Logic Unit
 */

#include <stdint.h>

#include "i8086.h"
#include "i8086_alu.h"
#include "sign_extend.h"

#define SF cpu->status.sf
#define CF cpu->status.cf
#define ZF cpu->status.zf
#define PF cpu->status.pf
#define OF cpu->status.of
#define AF cpu->status.af

#define SET_ZF(r) ZF = (r) == 0

#define SET_SF8(r) SF = ((r) & 0x80) >> 7
#define SET_ZF8(r) SET_ZF(r)

#define SET_PF8(r) { \
	uint8_t pf = (r);\
	pf ^= (pf >> 4); \
	pf ^= (pf >> 2); \
	pf ^= (pf >> 1); \
	PF = (~pf) & 1; }

#define SET_AF_ADD8(x,y,r) AF = (((x) ^ (y) ^ (r)) & 0x10) != 0
#define SET_AF_SUB8(x,y,r) AF = (((uint8_t)(x) ^ (uint8_t)(y) ^ (uint8_t)(r)) & 0x10) != 0
#define SET_OF_ADD8(x,y,r) OF = ((((r) ^ (x)) & ((r) ^ (y))) & 0x80) != 0
#define SET_OF_SUB8(x,y,r) OF = ((((uint8_t)(x) ^ (uint8_t)(y)) & ((uint8_t)(x) ^ (uint8_t)(r))) & 0x80) != 0
#define SET_CF_ADD8(r)     CF = (r) > 0xFF
#define SET_CF_SUB8(x,y)   CF = (y) > (x)

#define SET_SF16(r) SF = ((r) & 0x8000) >> 15
#define SET_ZF16(r) SET_ZF(r)

#define SET_PF16(r) SET_PF8(r & 0xFF)

#define SET_AF_ADD16(x,y,r) AF = (((x) ^ (y) ^ (r)) & 0x10) != 0
#define SET_AF_SUB16(x,y,r) AF = (((uint16_t)(x) ^ (uint16_t)(y) ^ (uint16_t)(r)) & 0x10) != 0
#define SET_OF_ADD16(x,y,r) OF = ((((r) ^ (x)) & ((r) ^ (y))) & 0x8000) != 0
#define SET_OF_SUB16(x,y,r) OF = ((((uint16_t)(x) ^ (uint16_t)(y)) & ((uint16_t)(x) ^ (uint16_t)(r))) & 0x8000) != 0
#define SET_CF_ADD16(r)     CF = (r) > 0xFFFF
#define SET_CF_SUB16(x,y)   CF = (y) > (x)

/* DBZ */
#define INT_DBZ 0 // ITC 0
extern void i8086_int(I8086* cpu, uint8_t type);

void alu_daa(I8086* cpu, uint8_t* x1) {
	uint8_t old = *x1;
	uint8_t correction = 0;
	uint8_t af = AF;
	uint8_t cf = CF;

	// Lower nibble adjustment
	if ((old & 0x0F) > 9 || af) {
		correction |= 0x06;
		af = 1;  // AF is set if low nibble adjustment occurs
	}
	else {
		af = 0;
	}

	// Upper nibble adjustment
	if (old > 0x99 || cf) {
		correction |= 0x60;
		cf = 1;
	}
	else {
		cf = 0;
	}

	// Apply correction
	alu_add8(cpu, x1, correction);

	// Update flags
	AF = af;
	CF = cf;
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

	uint16_t t = ((uint16_t)*h << 8) | *l;
	SET_ZF16(t);
	SET_SF8(*h);
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
void alu_aam(I8086* cpu, uint8_t* l, uint8_t* h, uint8_t divisor) {
	if (divisor != 0) {
		*h = (*l / divisor);
		*l = (*l % divisor);
	}
	else {
		*h = 0;
		*l = 0;
	}
	SET_PF8(*l);
	SET_SF8(*l);
	SET_ZF8(*l);
}
void alu_aad(I8086* cpu, uint8_t* l, uint8_t* h, uint8_t divisor) {	
	*l = (*h * divisor) + *l;
	*h = 0;
	SET_PF8(*l);
	SET_SF8(*l);
	SET_ZF8(*l);
}

/* 8bit alu */

void alu_add8(I8086* cpu, uint8_t* x1, uint8_t x2) {
	uint16_t tmp = (*x1 + x2);
	SET_AF_ADD8(*x1, x2, tmp);
	SET_OF_ADD8(*x1, x2, tmp);
	SET_CF_ADD8(tmp);
	*x1 = (tmp & 0xFF);
	SET_SF8(*x1);
	SET_PF8(*x1);
	SET_ZF8(*x1);
}
void alu_adc8(I8086* cpu, uint8_t* x1, uint8_t x2) {
	uint16_t tmp = ((uint16_t)*x1 + (uint16_t)x2 + CF);
	SET_AF_ADD8(*x1, x2, tmp);
	SET_OF_ADD8(*x1, x2, tmp);
	SET_CF_ADD8(tmp);
	*x1 = (tmp & 0xFF);
	SET_SF8(*x1);
	SET_PF8(*x1);
	SET_ZF8(*x1);
}
void alu_sub8(I8086* cpu, uint8_t* x1, uint8_t x2) {
	uint16_t tmp = (*x1 - x2);
	SET_AF_SUB8(*x1, x2, tmp);
	SET_OF_SUB8(*x1, x2, tmp);
	SET_CF_SUB8(*x1, x2);
	*x1 = (tmp & 0xFF);
	SET_SF8(*x1);
	SET_PF8(*x1);
	SET_ZF8(*x1);
}
void alu_sbb8(I8086* cpu, uint8_t* x1, uint8_t x2) {
	uint16_t tmp = ((uint16_t)*x1 - ((uint16_t)x2 + CF));
	SET_AF_SUB8(*x1, x2, tmp);
	SET_OF_SUB8(*x1, x2, tmp);
	SET_CF_SUB8(*x1, ((uint16_t)x2 + CF));
	*x1 = (tmp & 0xFF);
	SET_SF8(*x1);
	SET_PF8(*x1);
	SET_ZF8(*x1);
}

void alu_and8(I8086* cpu, uint8_t* x1, uint8_t x2) {
	*x1 &= x2;
	CF = 0;
	OF = 0;
	SET_SF8(*x1);
	SET_PF8(*x1);
	SET_ZF8(*x1);
}
void alu_xor8(I8086* cpu, uint8_t* x1, uint8_t x2) {
	*x1 ^= x2;
	CF = 0;
	OF = 0;
	SET_SF8(*x1);
	SET_PF8(*x1);
	SET_ZF8(*x1);
}
void alu_or8(I8086* cpu, uint8_t* x1, uint8_t x2) {
	*x1 |= x2;
	CF = 0;
	OF = 0;
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
		*x1 = (*x1 << 1) | cf;
	}
	if (count == 1) {
		OF = CF ^ ((*x1 >> 7) & 1);
	}
}
void alu_rcr8(I8086* cpu, uint8_t* x1, uint8_t count) {
	for (int i = 0; i < count; ++i) {
		uint8_t cf = CF;
		CF = (*x1 & 1);
		*x1 = (*x1 >> 1) | (cf << 7);
	}
	if (count == 1) {
		OF = (*x1 >> 7) ^ ((*x1 >> 6) & 1);
	}
}
void alu_rol8(I8086* cpu, uint8_t* x1, uint8_t count) {
	for (int i = 0; i < count; ++i) {
		uint8_t cf = (*x1 >> 7) & 1;
		CF = cf;
		*x1 = (*x1 << 1) | cf;
	}
	if (count == 1) {
		OF = ((*x1 >> 7) & 1) ^ CF;
	}
}
void alu_ror8(I8086* cpu, uint8_t* x1, uint8_t count) {
	for (int i = 0; i < count; ++i) {
		uint8_t cf = (*x1 & 1);
		CF = cf;
		*x1 = (*x1 >> 1) | (cf << 7);
	}
	if (count == 1) {
		OF = (*x1 >> 7) ^ ((*x1 >> 6) & 1);
	}
}
void alu_shl8(I8086* cpu, uint8_t* x1, uint8_t count) {
	for (int i = 0; i < count; ++i) {
		CF = (*x1 >> 7) & 1;
		*x1 <<= 1;
	}
	if (count == 1) {
		OF = ((*x1 >> 7) ^ (*x1 >> 6)) & 1;
	}
	//SET_SF8(*x1);
	//SET_PF8(*x1);
	//SET_ZF8(*x1);	
}
void alu_shr8(I8086* cpu, uint8_t* x1, uint8_t count) {
	if (count == 1) {
		OF = (*x1 >> 7) & 1;
	}
	for (int i = 0; i < count; ++i) {
		CF = (*x1 & 1);		
		*x1 >>= 1;
	}
	//SET_SF8(*x1);
	//SET_PF8(*x1);
	//SET_ZF8(*x1);
}
void alu_sar8(I8086* cpu, uint8_t* x1, uint8_t count) {
	for (int i = 0; i < count; ++i) {
		CF = (*x1 & 1);
		uint8_t msb = (*x1 & 0x80);
		*x1 >>= 1;
		*x1 |= msb;
	}
	if (count == 1) {
		OF = 0;
	}
	//SET_SF8(*x1);
	//SET_PF8(*x1);
	//SET_ZF8(*x1);
}

void alu_inc8(I8086* cpu, uint8_t* x1) {
	uint16_t tmp = (*x1 + 1);
	SET_AF_ADD8(*x1, 1, tmp);
	SET_OF_ADD8(*x1, 1, tmp);
	*x1 = (tmp & 0xFF);
	SET_SF8(*x1);
	SET_PF8(*x1);
	SET_ZF8(*x1);
}
void alu_dec8(I8086* cpu, uint8_t* x1) {
	uint16_t tmp = (*x1 - 1);
	SET_AF_SUB8(*x1, 1, tmp);
	SET_OF_SUB8(*x1, 1, tmp);
	*x1 = (tmp & 0xFF);
	SET_SF8(*x1);
	SET_PF8(*x1);
	SET_ZF8(*x1);
}

void alu_mul8(I8086* cpu, uint8_t multiplicand, uint8_t multiplier, uint8_t* lo, uint8_t* hi) {
	uint16_t tmp = (multiplicand * multiplier);
	*lo = (tmp & 0xFF);
	*hi = ((tmp >> 8) & 0xFF);
	CF = (*hi != 0);
	OF = (*hi != 0);
	
	//SET_SF8(*lo);
	//SET_PF8(*lo);
	//SET_ZF8(*lo);
}
void alu_imul8(I8086* cpu, uint8_t multiplicand, uint8_t multiplier, uint8_t* lo, uint8_t* hi) {
	/*int16_t tmp = ((int8_t)multiplicand * (int8_t)multiplier);
	*lo = (tmp & 0xFF);
	*hi = ((tmp >> 8) & 0xFF);
	CF = (*hi != 0);
	OF = (*hi != 0);

	SET_SF16(tmp);
	SET_PF16(tmp);
	SET_ZF16(tmp);*/

	int16_t tmp = (int8_t)multiplicand * (int8_t)multiplier;
	*lo = (uint8_t)(tmp & 0xFF);
	*hi = (uint8_t)((tmp >> 8) & 0xFF);
	
	int8_t hi_signed = (int8_t)*hi;
	CF = OF = !(hi_signed == 0x00 || hi_signed == -1);

	//CF = (*hi != 0);
	//OF = (*hi != 0);
	
	//SET_SF8(*lo);
	//SET_PF8(*lo);
	//SET_ZF8(*lo);
}

void alu_div8(I8086* cpu, uint8_t dividend_lo, uint8_t dividend_hi, uint8_t divider, uint8_t* quotient, uint8_t* remainder) {
	if (divider == 0) {
		i8086_int(cpu, INT_DBZ); /* divide by zero */
	}
	else {
		uint16_t dividend = ((uint16_t)dividend_hi << 8) | dividend_lo;
		uint16_t quotient16 = (dividend / divider);
		uint16_t remainder16 = (dividend % divider);
		if (quotient16 > 0xFF) {
			i8086_int(cpu, INT_DBZ); /* quotient too large */
		}
		else {
			*quotient = quotient16 & 0xFF;   // quotient
			*remainder = remainder16 & 0xFF; // remainder
		}
	}
}
void alu_idiv8(I8086* cpu, uint8_t dividend_lo, uint8_t dividend_hi, uint8_t divider, uint8_t* quotient, uint8_t* remainder) {
	int8_t divisor = (int8_t)divider;
	if (divisor == 0) {
		i8086_int(cpu, INT_DBZ); /* divide by zero */
	}
	else {
		int16_t dividend = ((int16_t)(int8_t)dividend_hi << 8) | dividend_lo; //(dividend_hi << 8) | dividend_lo;
		int16_t quotient16 = (dividend / divisor);
		int16_t remainder16 = (dividend % divisor);
		if (quotient16 >= -128 && quotient16 <= 127) {
			*quotient = (uint8_t)(int8_t)quotient16;//quotient16 & 0xFF;   // quotient
			*remainder = (uint8_t)(int8_t)remainder16;//remainder16 & 0xFF; // remainder
		}
		else {
			i8086_int(cpu, INT_DBZ); /* quotient too large */
		}
	}
}

void alu_neg8(I8086* cpu, uint8_t* x1) {
	uint8_t tmp = (0 - *x1);
	SET_AF_SUB8(0, *x1, tmp);
	SET_OF_SUB8(0, *x1, tmp);
	CF = (*x1 != 0);
	*x1 = tmp;
	SET_SF8(*x1);
	SET_PF8(*x1);
	SET_ZF8(*x1);
}

/* 16bit alu */

void alu_add16(I8086* cpu, uint16_t* x1, uint16_t x2) {
	uint32_t tmp = (*x1 + x2);
	SET_AF_ADD16(*x1, x2, tmp);
	SET_OF_ADD16(*x1, x2, tmp);
	SET_CF_ADD16(tmp);
	*x1 = (tmp & 0xFFFF);
	SET_SF16(*x1);
	SET_PF16(*x1);
	SET_ZF16(*x1);
}
void alu_adc16(I8086* cpu, uint16_t* x1, uint16_t x2) {
	uint32_t tmp = ((uint32_t)*x1 + (uint32_t)x2 + CF);
	SET_AF_ADD16(*x1, x2, tmp);
	SET_OF_ADD16(*x1, x2, tmp);
	SET_CF_ADD16(tmp);
	*x1 = (tmp & 0xFFFF);
	SET_SF16(*x1);
	SET_PF16(*x1);
	SET_ZF16(*x1);
}
void alu_sub16(I8086* cpu, uint16_t* x1, uint16_t x2) {
	uint32_t tmp = (*x1 - x2);
	SET_AF_SUB16(*x1, x2, tmp);
	SET_OF_SUB16(*x1, x2, tmp);
	SET_CF_SUB16(*x1, x2);
	*x1 = (tmp & 0xFFFF);
	SET_SF16(*x1);
	SET_PF16(*x1);
	SET_ZF16(*x1);
}
void alu_sbb16(I8086* cpu, uint16_t* x1, uint16_t x2) {
	uint32_t tmp = ((uint32_t)*x1 - ((uint32_t)x2 + CF));
	SET_AF_SUB16(*x1, x2, tmp);
	SET_OF_SUB16(*x1, x2, tmp);
	SET_CF_SUB16(*x1, ((uint32_t)x2 + CF));
	*x1 = (tmp & 0xFFFF);
	SET_SF16(*x1);
	SET_PF16(*x1);
	SET_ZF16(*x1);
}

void alu_and16(I8086* cpu, uint16_t* x1, uint16_t x2) {
	*x1 &= x2;
	CF = 0;
	OF = 0;
	SET_SF16(*x1);
	SET_PF16(*x1);
	SET_ZF16(*x1);
}
void alu_xor16(I8086* cpu, uint16_t* x1, uint16_t x2) {
	*x1 ^= x2;
	CF = 0;
	OF = 0;
	SET_SF16(*x1);
	SET_PF16(*x1);
	SET_ZF16(*x1);
}
void alu_or16(I8086*  cpu, uint16_t* x1, uint16_t x2) {
	*x1 |= x2;
	CF = 0;
	OF = 0;
	SET_SF16(*x1);
	SET_PF16(*x1);
	SET_ZF16(*x1);
}

void alu_cmp16(I8086* cpu, uint16_t  x1, uint16_t x2) {
	uint16_t tmp = x1;
	alu_sub16(cpu, &tmp, x2);
}
void alu_test16(I8086* cpu, uint16_t x1, uint16_t x2) {
	uint16_t tmp = x1;
	alu_and16(cpu, &tmp, x2);
}

void alu_rcl16(I8086* cpu, uint16_t* x1, uint8_t count) {
	for (int i = 0; i < count; ++i) {
		uint8_t cf = CF;
		CF = (*x1 >> 15) & 1;
		*x1 = (*x1 << 1) | cf;
	}
	if (count == 1) {
		OF = CF ^ ((*x1 >> 15) & 1);
	}
}
void alu_rcr16(I8086* cpu, uint16_t* x1, uint8_t count) {
	for (int i = 0; i < count; ++i) {
		uint8_t cf = CF;
		CF = (*x1 & 1);		
		*x1 = (*x1 >> 1) | (cf << 15);
	}
	if (count == 1) {
		OF = (*x1 >> 15) ^ ((*x1 >> 14) & 1);
	}
}
void alu_rol16(I8086* cpu, uint16_t* x1, uint8_t count) {
	for (int i = 0; i < count; ++i) {
		uint8_t cf = (*x1 >> 15) & 1;
		CF = cf;
		*x1 = (*x1 << 1) | cf;
	}
	if (count == 1) {
		OF = ((*x1 >> 15) & 1) ^ CF;
	}
}
void alu_ror16(I8086* cpu, uint16_t* x1, uint8_t count) {
	for (int i = 0; i < count; ++i) {
		uint8_t cf = (*x1 & 1);
		CF = cf;
		*x1 = (*x1 >> 1) | (cf << 15);
	}
	if (count == 1) {
		OF = (*x1 >> 15) ^ ((*x1 >> 14) & 1);
	}
}
void alu_shl16(I8086* cpu, uint16_t* x1, uint8_t count) {
	for (int i = 0; i < count; ++i) {
		CF = (*x1 >> 15) & 1;
		*x1 <<= 1;	
	}
	if (count == 1) {
		OF = ((*x1 >> 15) ^ (*x1 >> 14)) & 1;
	}
	//SET_SF16(*x1);
	//SET_PF16(*x1);
	//SET_ZF16(*x1);
}
void alu_shr16(I8086* cpu, uint16_t* x1, uint8_t count) {
	if (count == 1) {
		OF = (*x1 >> 15) & 1;
	}
	for (int i = 0; i < count; ++i) {
		CF = (*x1 & 1);
		*x1 >>= 1;
	}
	//SET_SF16(*x1);
	//SET_PF16(*x1);
	//SET_ZF16(*x1);	
}
void alu_sar16(I8086* cpu, uint16_t* x1, uint8_t count) {	
	for (int i = 0; i < count; ++i) {
		CF = (*x1 & 1);
		uint16_t msb = (*x1 & 0x8000);
		*x1 >>= 1;
		*x1 |= msb;
	}
	if (count == 1) {
		OF = 0;
	}
	//SET_SF16(*x1);
	//SET_PF16(*x1);
	//SET_ZF16(*x1);
}

void alu_inc16(I8086* cpu, uint16_t* x1) {
	uint32_t tmp = (*x1 + 1);
	SET_AF_ADD16(*x1, 1, tmp);
	SET_OF_ADD16(*x1, 1, tmp);
	*x1 = (tmp & 0xFFFF);
	SET_SF16(*x1);
	SET_PF16(*x1);
	SET_ZF16(*x1);
}
void alu_dec16(I8086* cpu, uint16_t* x1) {
	uint32_t tmp = (*x1 - 1);
	SET_AF_SUB16(*x1, 1, tmp);
	SET_OF_SUB16(*x1, 1, tmp);
	*x1 = (tmp & 0xFFFF);
	SET_SF16(*x1);
	SET_PF16(*x1);
	SET_ZF16(*x1);
}

void alu_mul16(I8086* cpu, uint16_t multiplicand, uint16_t multiplier, uint16_t* lo, uint16_t* hi) {
	uint32_t tmp = (multiplicand * multiplier);
	*lo = (tmp & 0xFFFF);
	*hi = ((tmp >> 16) & 0xFFFF);
	CF = (*hi != 0);
	OF = (*hi != 0);

	//SET_SF16(*lo);
	//SET_PF16(*lo);
	//SET_ZF16(*lo);
}
void alu_imul16(I8086* cpu, uint16_t multiplicand, uint16_t multiplier, uint16_t* lo, uint16_t* hi) {
	/*int32_t tmp = ((int16_t)multiplicand * (int16_t)multiplier);
	*lo = (tmp & 0xFFFF);
	*hi = ((tmp >> 16) & 0xFFFF);
	CF = (*hi != 0);
	OF = (*hi != 0);

	SET_SF16(tmp);
	SET_PF16(tmp);
	SET_ZF16(tmp);*/

	int16_t a = (int16_t)multiplicand;
	int16_t b = (int16_t)multiplier;

	int32_t result = (int32_t)a * (int32_t)b;

	*lo = (uint16_t)(result & 0xFFFF);
	*hi = (uint16_t)((result >> 16) & 0xFFFF);

	/*if (result < -32768 || result > 32767) {
		CF = 1;
		OF = 1;
	}
	else {
		CF = 0;
		OF = 0;
	}*/

	int16_t hi_signed = (int16_t)*hi;
	CF = OF = !(hi_signed == 0x00 || hi_signed == -1);

	//SET_SF16(*lo);
	//SET_PF16(*lo);
	//SET_ZF16(*lo);
}
void alu_div16(I8086* cpu, uint16_t dividend_lo, uint16_t dividend_hi, uint16_t divider, uint16_t* quotient, uint16_t* remainder) {
	if (divider == 0) {
		i8086_int(cpu, INT_DBZ); /* divide by zero */
	}
	else {
		uint32_t dividend = ((uint32_t)dividend_hi << 16) | dividend_lo;
		uint32_t quotient32 = (dividend / divider);
		uint32_t remainder32 = (dividend % divider);
		if (quotient32 > 0xFFFF) {
			i8086_int(cpu, INT_DBZ); /* quotient too large */
		}
		else {
			*quotient = quotient32 & 0xFFFF;   // quotient
			*remainder = remainder32 & 0xFFFF; // remainder
		}
	}
}
void alu_idiv16(I8086* cpu, uint16_t dividend_lo, uint16_t dividend_hi, uint16_t divider, uint16_t* quotient, uint16_t* remainder) {
	int16_t divisor = (int16_t)divider;	
	if (divisor == 0) {
		i8086_int(cpu, INT_DBZ); /* divide by zero */
	}
	else {
		int32_t dividend = ((int32_t)(int16_t)dividend_hi << 16) | dividend_lo; //(dividend_hi << 16) | dividend_lo;
		int32_t quotient32 = (dividend / divisor);
		int32_t remainder32 = (dividend % divisor);

		if (quotient32 >= -32768 && quotient32 <= 32767) {
			*quotient = (uint16_t)(int16_t)quotient32;//quotient32 & 0xFFFF;   // quotient
			*remainder = (uint16_t)(int16_t)remainder32;//remainder32 & 0xFFFF; // remainder
		}
		else {
			i8086_int(cpu, INT_DBZ); /* quotient too large */
		}
	}
}

void alu_neg16(I8086* cpu, uint16_t* x1) {
	uint16_t tmp = (0 - *x1);
	SET_AF_SUB16(0, *x1, tmp);
	SET_OF_SUB16(0, *x1, tmp);
	CF = (*x1 != 0);
	*x1 = tmp;
	SET_SF16(*x1);
	SET_PF16(*x1);
	SET_ZF16(*x1);
}
