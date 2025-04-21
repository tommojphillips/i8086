/* alu.h
 * tommojphillips 2025 ( https://github.com/tommojphillips/ )
 * Intel 8086 Arithmetic Logic Unit
 */

#ifndef ALU_H
#define ALU_H

#ifdef __cplusplus
extern "C" {
#endif

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
#define SET_PF8(r) PF = alu_cal_parity8(r)
#define SET_ZF8(r) SET_ZF(r)

#define SET_AF_ADD8(x,y,r) AF = (((x) ^ (y) ^ (r)) & 0x10) == 0x10
#define SET_AF_SUB8(x,y,r) AF = (((x) ^ (y) ^ (r)) & 0x10) == 0

#define SET_CF16(r) CF = (r) > 0xFFFF
#define SET_OF16(r) OF = (r) > 0x7FFF
#define SET_SF16(r) SF = ((r) & 0x8000) >> 15
#define SET_PF16(r) PF = alu_cal_parity16(r)
#define SET_ZF16(r) SET_ZF(r)

#define SET_AF_ADD16(x,y,r) AF = (((x) ^ (y) ^ (r)) & 0x10) == 0x10
#define SET_AF_SUB16(x,y,r) AF = (((x) ^ (y) ^ (r)) & 0x10) == 0

uint8_t alu_cal_parity8(uint8_t value);
void alu_and8(I8086* cpu, uint8_t* x1, uint8_t x2);
void alu_xor8(I8086* cpu, uint8_t* x1, uint8_t x2);
void alu_or8(I8086*  cpu, uint8_t* x1, uint8_t x2);
void alu_add8(I8086* cpu, uint8_t* x1, uint8_t x2);
void alu_adc8(I8086* cpu, uint8_t* x1, uint8_t x2);
void alu_sub8(I8086* cpu, uint8_t* x1, uint8_t x2);
void alu_sbb8(I8086* cpu, uint8_t* x1, uint8_t x2);
void alu_cmp8(I8086* cpu, uint8_t  x1, uint8_t x2);
void alu_test8(I8086* cpu, uint8_t  x1, uint8_t x2);
void alu_inc8(I8086* cpu, uint8_t* x1);
void alu_dec8(I8086* cpu, uint8_t* x1);
void alu_rcl8(I8086* cpu, uint8_t* x1, uint8_t count);
void alu_rcr8(I8086* cpu, uint8_t* x1, uint8_t count);
void alu_rol8(I8086* cpu, uint8_t* x1, uint8_t count);
void alu_ror8(I8086* cpu, uint8_t* x1, uint8_t count);
void alu_shl8(I8086* cpu, uint8_t* x1, uint8_t count);
void alu_shr8(I8086* cpu, uint8_t* x1, uint8_t count);
void alu_sar8(I8086* cpu, uint8_t* x1, uint8_t count);

uint8_t alu_cal_parity16(uint16_t value);
void alu_and16(I8086* cpu, uint16_t* x1, uint16_t x2);
void alu_xor16(I8086* cpu, uint16_t* x1, uint16_t x2);
void alu_or16(I8086*  cpu, uint16_t* x1, uint16_t x2);
void alu_add16(I8086* cpu, uint16_t* x1, uint16_t x2);
void alu_adc16(I8086* cpu, uint16_t* x1, uint16_t x2);
void alu_sub16(I8086* cpu, uint16_t* x1, uint16_t x2);
void alu_sbb16(I8086* cpu, uint16_t* x1, uint16_t x2);
void alu_cmp16(I8086* cpu, uint16_t  x1, uint16_t x2);
void alu_test16(I8086* cpu, uint16_t  x1, uint16_t x2);
void alu_inc16(I8086* cpu, uint16_t* x1);
void alu_dec16(I8086* cpu, uint16_t* x1);
void alu_rcl16(I8086* cpu, uint16_t* x1, uint8_t count);
void alu_rcr16(I8086* cpu, uint16_t* x1, uint8_t count);
void alu_rol16(I8086* cpu, uint16_t* x1, uint8_t count);
void alu_ror16(I8086* cpu, uint16_t* x1, uint8_t count);
void alu_shl16(I8086* cpu, uint16_t* x1, uint8_t count);
void alu_shr16(I8086* cpu, uint16_t* x1, uint8_t count);
void alu_sar16(I8086* cpu, uint16_t* x1, uint8_t count);

#ifdef __cplusplus
};
#endif

#endif
