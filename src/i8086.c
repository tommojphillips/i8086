/* i8086.c
 * tommojphillips 2025 ( https://github.com/tommojphillips/ )
 * Intel 8086 CPU
 */

#include <stdint.h>

#include "i8086.h"
#include "alu.h"

void i8086_push_byte(I8086* cpu, uint8_t value) {
	SP -= 1;
	WRITE_BYTE(STACK_ADDR(SP), value);
}
void i8086_pop_byte(I8086* cpu, uint8_t* value) {
	*value = READ_BYTE(STACK_ADDR(SP));
	SP += 1;
}
void i8086_push_word(I8086* cpu, uint16_t value) {
	SP -= 2;
	WRITE_WORD(STACK_ADDR(SP), value);
}
void i8086_pop_word(I8086* cpu, uint16_t* value) {
	*value = READ_WORD(STACK_ADDR(SP));
	SP += 2;
}

void i8086_int(I8086* cpu, uint8_t type) {
	i8086_push_word(cpu, cpu->status.word);
	i8086_push_word(cpu, CS);
	i8086_push_word(cpu, IP);
	uint20_t addr = type * 4;
	CS = READ_WORD(addr + 2);
	IP = READ_WORD(addr);
	IF = 0;
	TF = 0;
}

/* Swap pointers if 'D' bit is set in opcode bXXXXXXDX */
static void get_direction(I8086* cpu, void** ptr1, void** ptr2) {
	if (D) {
		void* tmp = *ptr1;
		*ptr1 = *ptr2;
		*ptr2 = tmp;
	}
}

/* Mod R/M */

static uint16_t modrm_get_base_address(I8086* cpu) {
	// Get 1bit address
	switch (cpu->modrm.rm) {
		case 0b000: // base rel indexed - BX + SI
			return BX + SI;
		case 0b001: // base rel indexed - BX + DI
			return BX + DI;
		case 0b010: // base rel indexed stack - BP + SI
			return BP + SI;
		case 0b011: // base rel indexed stack - BP + DI
			return BP + DI;
		case 0b100: // implied SI
			return SI;
		case 0b101: // implied DI
			return DI;
		case 0b110: // implied BP
			return BP;
		case 0b111: // implied BX
			return BX;
	}
	return 0;
}
static uint20_t modrm_get_effective_base_address(I8086* cpu) {
	// Get 20bit effective base address
	switch (cpu->modrm.rm) {
		case 0b000: // base rel indexed - BX + SI
			return DATA_ADDR(BX + SI);
		case 0b001: // base rel indexed - BX + DI
			return DATA_ADDR(BX + DI);
		case 0b010: // base rel indexed stack - BP + SI
			return STACK_ADDR(BP + SI);
		case 0b011: // base rel indexed stack - BP + DI
			return STACK_ADDR(BP + DI);
		case 0b100: // implied SI
			return DATA_ADDR(SI);
		case 0b101: // implied DI
			return DATA_ADDR(DI);
		case 0b110: // implied BP
			return STACK_ADDR(BP);
		case 0b111: // implied BX
			return DATA_ADDR(BX);
	}
	return 0;
}
static uint16_t modrm_get_addr(I8086* cpu) {
	// Get 16bit address
	uint16_t addr = 0;
	switch (cpu->modrm.mod) {

		case 0b00:
			if (cpu->modrm.rm == 0b110) {
				// displacement mode - [ disp16 ]
				addr = FETCH_WORD();
			}
			else {
				// register mode - [ base16 ]
				addr = modrm_get_base_address(cpu);
			}
			break;

		case 0b01:
			// memory mode; 8bit displacement - [ base16 + disp8 ]
			addr = modrm_get_base_address(cpu) + FETCH_BYTE();
			break;

		case 0b10:
			// memory mode; 16bit displacement - [ base16 + disp16 ]
			addr = modrm_get_base_address(cpu) + FETCH_WORD();
			break;
	}
	return addr;
}
static uint20_t modrm_get_effective_addr(I8086* cpu) {
	// Get 20bit effective address
	uint20_t addr = 0;
	switch (cpu->modrm.mod) {

		case 0b00:
			if (cpu->modrm.rm == 0b110) {
				// displacement mode - [ disp16 ]
				uint16_t imm = FETCH_WORD();
				addr = DATA_ADDR(imm);
			}
			else {
				// register mode - [ base16 ]
				addr = modrm_get_effective_base_address(cpu);
			}
			break;

		case 0b01:
			// memory mode; 8bit displacement - [ base16 + disp8 ]
			addr = modrm_get_effective_base_address(cpu) + FETCH_BYTE();
			break;

		case 0b10:
			// memory mode; 16bit displacement - [ base16 + disp16 ]
			addr = modrm_get_effective_base_address(cpu) + FETCH_WORD();
			break;
	}
	return addr;
}
uint16_t* modrm_get_ptr16(I8086* cpu) {
	// Get R/M pointer to 16bit value
	if (cpu->modrm.mod == 0b11) {
		return GET_REG16(cpu->modrm.rm);
	}
	else {
		return GET_MEM_PTR(modrm_get_effective_addr(cpu));
	}
}
uint8_t* modrm_get_ptr8(I8086* cpu) {
	// Get R/M pointer to 8bit value
	if (cpu->modrm.mod == 0b11) {
		return GET_REG8(cpu->modrm.rm);
	}
	else {
		return GET_MEM_PTR(modrm_get_effective_addr(cpu));
	}
}

/* Opcodes */

static void add_rm_imm(I8086* cpu) {
	/* add r/m, imm (80/81/82/83, R/M reg = b000) b100000SW */
	if (W) {
		if (SW == 0b01) {
			/* reg16, disp16 */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint16_t imm = FETCH_WORD();
			alu_add16(cpu, rm, imm);
		}
		else {
			/* reg16, disp8 */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint16_t imm = FETCH_BYTE();
			alu_add16(cpu, rm, imm);
		}
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		uint8_t imm = FETCH_BYTE();
		alu_add8(cpu, rm, imm);
	}
}
static void add_rm_reg(I8086* cpu) {
	/* add r/m, reg (00/01/02/03) b000000DW */
	cpu->modrm.byte = FETCH_BYTE();
	if (W) {
		uint16_t* rm = modrm_get_ptr16(cpu);
		uint16_t* reg = GET_REG16(cpu->modrm.reg);
		get_direction(cpu, &reg, &rm);
		alu_add16(cpu, rm, *reg);
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		uint8_t* reg = GET_REG8(cpu->modrm.reg);
		get_direction(cpu, &reg, &rm);
		alu_add8(cpu, rm, *reg);
	}
}
static void add_accum_imm(I8086* cpu) {
	/* add AL/AX, imm (04/05) b0000010W */
	if (W) {
		uint16_t imm = FETCH_WORD();
		alu_add16(cpu, &AX, imm);
	}
	else {
		uint8_t imm = FETCH_BYTE();
		alu_add8(cpu, &AL, imm);
	}
}

static void or_rm_imm(I8086* cpu) {
	/* or r/m, imm (80/81/82/83, R/M reg = b001) b100000SW */
	if (W) {
		if (SW == 0b01) {
			/* reg16, disp16 */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint16_t imm = FETCH_WORD();
			alu_or16(cpu, rm, imm);
		}
		else {
			/* reg16, disp8 */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint16_t imm = FETCH_BYTE();
			alu_or16(cpu, rm, imm);
		}
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		uint8_t imm = FETCH_BYTE();
		alu_or8(cpu, rm, imm);
	}
}
static void or_rm_reg(I8086* cpu) {
	/* or r/m, reg (08/0A/09/0B) b000010DW */
	cpu->modrm.byte = FETCH_BYTE();
	if (W) {
		uint16_t* rm = modrm_get_ptr16(cpu);
		uint16_t* reg = GET_REG16(cpu->modrm.reg);
		get_direction(cpu, &reg, &rm);
		alu_or16(cpu, rm, *reg);
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		uint8_t* reg = GET_REG8(cpu->modrm.reg);
		get_direction(cpu, &reg, &rm);
		alu_or8(cpu, rm, *reg);
	}
}
static void or_accum_imm(I8086* cpu) {
	/* or AL/AX, imm (0C/0D) b0000110W */
	if (W) {
		uint16_t imm = FETCH_WORD();
		alu_or16(cpu, &AX, imm);
	}
	else {
		uint8_t imm = FETCH_BYTE();
		alu_or8(cpu, &AL, imm);
	}
}

static void adc_rm_imm(I8086* cpu) {
	/* adc r/m, imm (80/81/82/83, R/M reg = b010) b100000SW */
	if (W) {
		if (SW == 0b01) {
			/* reg16, disp16 */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint16_t imm = FETCH_WORD();
			alu_adc16(cpu, rm, imm);
		}
		else {
			/* reg16, disp8 */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint16_t imm = FETCH_BYTE();
			alu_adc16(cpu, rm, imm);
		}
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		uint8_t imm = FETCH_BYTE();
		alu_adc8(cpu, rm, imm);
	}
}
static void adc_rm_reg(I8086* cpu) {
	/* adc r/m, reg (10/12/11/13) b000100DW */
	cpu->modrm.byte = FETCH_BYTE();
	if (W) {
		uint16_t* rm = modrm_get_ptr16(cpu);
		uint16_t* reg = GET_REG16(cpu->modrm.reg);
		get_direction(cpu, &reg, &rm);
		alu_adc16(cpu, rm, *reg);
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		uint8_t* reg = GET_REG8(cpu->modrm.reg);
		get_direction(cpu, &reg, &rm);
		alu_adc8(cpu, rm, *reg);
	}
}
static void adc_accum_imm(I8086* cpu) {
	/* adc AL/AX, imm (14/15) b0001010W */
	if (W) {
		uint16_t imm = FETCH_WORD();
		alu_adc16(cpu, &AX, imm);
	}
	else {
		uint8_t imm = FETCH_BYTE();
		alu_adc8(cpu, &AL, imm);
	}
}

static void sbb_rm_imm(I8086* cpu) {
	/* sbb r/m, imm (80/81/82/83, R/M reg = b011)  b100000SW */
	if (W) {
		if (SW == 0b01) {
			/* reg16 + disp16 */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint16_t imm = FETCH_WORD();
			alu_sbb16(cpu, rm, imm);
		}
		else {
			/* reg16 + disp8 */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint16_t imm = FETCH_BYTE();
			alu_sbb16(cpu, rm, imm);
		}
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		uint8_t imm = FETCH_BYTE();
		alu_sbb8(cpu, rm, imm);
	}
}
static void sbb_rm_reg(I8086* cpu) {
	/* sbb r/m, reg (18/1A/19/1B) b000110DW */
	cpu->modrm.byte = FETCH_BYTE();
	if (W) {
		uint16_t* rm = modrm_get_ptr16(cpu);
		uint16_t* reg = GET_REG16(cpu->modrm.reg);
		get_direction(cpu, &reg, &rm);
		alu_sbb16(cpu, rm, *reg);
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		uint8_t* reg = GET_REG8(cpu->modrm.reg);
		get_direction(cpu, &reg, &rm);
		alu_sbb8(cpu, rm, *reg);
	}
}
static void sbb_accum_imm(I8086* cpu) {
	/* sbb AL/AX, imm (1C/1D) b0001110W */
	if (W) {
		uint16_t imm = FETCH_WORD();
		alu_sbb16(cpu, &AX, imm);
	}
	else {
		uint8_t imm = FETCH_BYTE();
		alu_sbb8(cpu, &AL, imm);
	}
}

static void and_rm_imm(I8086* cpu) {
	/* and r/m, imm (80/81/82/83, R/M reg = b100) b100000SW */
	if (W) {
		if (SW == 0b01) {
			/* reg16, disp16 */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint16_t imm = FETCH_WORD();
			alu_and16(cpu, rm, imm);
		}
		else {
			/* reg16, disp8 */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint16_t imm = FETCH_BYTE();
			alu_and16(cpu, rm, imm);
		}
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		uint8_t imm = FETCH_BYTE();
		alu_and8(cpu, rm, imm);
	}
}
static void and_rm_reg(I8086* cpu) {
	/* and r/m, reg (20/22/21/23) b001000DW */
	cpu->modrm.byte = FETCH_BYTE();
	if (W) {
		uint16_t* rm = modrm_get_ptr16(cpu);
		uint16_t* reg = GET_REG16(cpu->modrm.reg);
		get_direction(cpu, &reg, &rm);
		alu_and16(cpu, rm, *reg);
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		uint8_t* reg = GET_REG8(cpu->modrm.reg);
		get_direction(cpu, &reg, &rm);
		alu_and8(cpu, rm, *reg);
	}
}
static void and_accum_imm(I8086* cpu) {
	/* and AL/AX, imm (24/25) b0010010W */
	if (W) {
		uint16_t imm = FETCH_WORD();
		alu_and16(cpu, &AX, imm);
	}
	else {
		uint8_t imm = FETCH_BYTE();
		alu_and8(cpu, &AL, imm);
	}
}

static void sub_rm_imm(I8086* cpu) {
	/* sub r/m, imm (80/81, R/M reg = b101) b100000SW */
	if (W) {
		if (SW == 0b01) {
			/* reg16, disp16 */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint16_t imm = FETCH_WORD();
			alu_sub16(cpu, rm, imm);
		}
		else {
			/* reg16, disp8 */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint16_t imm = FETCH_BYTE();
			alu_sub16(cpu, rm, imm);
		}
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		uint8_t imm = FETCH_BYTE();
		alu_sub8(cpu, rm, imm);
	}
}
static void sub_rm_reg(I8086* cpu) {
	/* sub r/m, reg (28/2A/29/2B) b001010DW */
	cpu->modrm.byte = FETCH_BYTE();
	if (W) {
		uint16_t* rm = modrm_get_ptr16(cpu);
		uint16_t* reg = GET_REG16(cpu->modrm.reg);
		get_direction(cpu, &reg, &rm);
		alu_sub16(cpu, rm, *reg);
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		uint8_t* reg = GET_REG8(cpu->modrm.reg);
		get_direction(cpu, &reg, &rm);
		alu_sub8(cpu, rm, *reg);
	}
}
static void sub_accum_imm(I8086* cpu) {
	/* sub AL/AX, imm (2C/2D) b0010110W */
	if (W) {
		uint16_t imm = FETCH_WORD();
		alu_sub16(cpu, &AX, imm);
	}
	else {
		uint8_t imm = FETCH_BYTE();
		alu_sub8(cpu, &AL, imm);
	}
}

static void xor_rm_imm(I8086* cpu) {
	/* xor r/m, imm (80/81/82/83, R/M reg = b110) b100000SW */
	if (W) {
		if (SW == 0b01) {
			/* reg16, disp16 */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint16_t imm = FETCH_WORD();
			alu_xor16(cpu, rm, imm);
		}
		else {
			/* reg16, disp8 */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint16_t imm = FETCH_BYTE();
			alu_xor16(cpu, rm, imm);
		}
	} 
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		uint8_t imm = FETCH_BYTE();
		alu_xor8(cpu, rm, imm);
	}
}
static void xor_rm_reg(I8086* cpu) {
	/* xor r/m, reg (30/32/31/33) b001100DW */
	cpu->modrm.byte = FETCH_BYTE();
	if (W) {
		uint16_t* rm = modrm_get_ptr16(cpu);
		uint16_t* reg = GET_REG16(cpu->modrm.reg);
		get_direction(cpu, &reg, &rm);
		alu_xor16(cpu, rm, *reg);
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		uint8_t* reg = GET_REG8(cpu->modrm.reg);
		get_direction(cpu, &reg, &rm);
		alu_xor8(cpu, rm, *reg);
	}
}
static void xor_accum_imm(I8086* cpu) {
	/* xor AL/AX, imm (34/35) b0011010W */
	if (W) {
		uint16_t imm = FETCH_WORD();
		alu_xor16(cpu, &AX, imm);
	}
	else {
		uint8_t imm = FETCH_BYTE();
		alu_xor8(cpu, &AL, imm);
	}
}

static void cmp_rm_imm(I8086* cpu) {
	/* cmp r/m, imm (80/81/82/83, R/M reg = b111)  b100000SW */
	if (W) {
		if (SW == 0b01) {
			/* reg16, disp16 */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint16_t imm = FETCH_WORD();
			alu_cmp16(cpu, *rm, imm);
		}
		else {
			/* reg16, disp8 */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint16_t imm = FETCH_BYTE();
			alu_cmp16(cpu, *rm, imm);
		}
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		uint8_t imm = FETCH_BYTE();
		alu_cmp8(cpu, *rm, imm);
	}
}
static void cmp_rm_reg(I8086* cpu) {
	/* cmp r/m, reg (38/39/3A/3B) b001110DW */
	cpu->modrm.byte = FETCH_BYTE();
	if (W) {
		uint16_t* rm = modrm_get_ptr16(cpu);
		uint16_t* reg = GET_REG16(cpu->modrm.reg);
		get_direction(cpu, &reg, &rm);
		alu_cmp16(cpu, *rm, *reg);
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		uint8_t* reg = GET_REG8(cpu->modrm.reg);
		get_direction(cpu, &reg, &rm);
		alu_cmp8(cpu, *rm, *reg);
	}
}
static void cmp_accum_imm(I8086* cpu) {
	/* cmp AL/AX, imm (3C/3D) b0011110W */
	if (W) {
		uint16_t imm = FETCH_WORD();
		alu_cmp16(cpu, AX, imm);
	}
	else {
		uint8_t imm = FETCH_BYTE();
		alu_cmp8(cpu, AL, imm);
	}
}

static void test_rm_imm(I8086* cpu) {
	/* test r/m, imm (F6/F7, R/M reg = b000) b1111011W */
	if (W) {
		uint16_t* rm = modrm_get_ptr16(cpu);
		uint16_t imm = FETCH_WORD();
		alu_test16(cpu, *rm, imm);
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		uint8_t imm = FETCH_BYTE();
		alu_test8(cpu, *rm, imm);
	}
}
static void test_rm_reg(I8086* cpu) {
	/* test r/m, reg (84/85) b1000010W */
	cpu->modrm.byte = FETCH_BYTE();
	if (W) {
		uint16_t* rm = modrm_get_ptr16(cpu);
		uint16_t* reg = GET_REG16(cpu->modrm.reg);
		alu_test16(cpu, *rm, *reg);
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		uint8_t* reg = GET_REG8(cpu->modrm.reg);
		alu_test8(cpu, *rm, *reg);
	}
}
static void test_accum_imm(I8086* cpu) {
	/* test AL/AX, imm (A8/A9) b1010100W */
	if (W) {
		uint16_t imm = FETCH_WORD();
		alu_test16(cpu, AX, imm);
	}
	else {
		uint8_t imm = FETCH_BYTE();
		alu_test8(cpu, AL, imm);
	}
}

static void daa(I8086* cpu) {
	/* Decimal Adjust for Addition (27) b00100111 */
	uint8_t correction = 0;
	if ((AL & 0x0F) > 9 || AF) {
		correction |= 6;
		AF = 1;
	}

	if (AL > 0x9F || CF) {
		correction |= 0x60;
		CF = 1;
	}

	alu_add8(cpu, &AL, correction);
}
static void das(I8086* cpu) {
	/* Decimal Adjust for Subtraction (2F) b00101111 */
	uint8_t correction = 0;
	if ((AL & 0x0F) > 9 || AF) {
		correction |= 6;
		AF = 1;
	}

	if (AL > 0x9F || CF) {
		correction |= 0x60;
		CF = 1;
	}

	alu_sub8(cpu, &AL, correction);
}
static void aaa(I8086* cpu) {
	/* ASCII Adjust for Addition (37) b00110111 */
	if ((AL & 0x0F) > 9 || AF) {
		AL += 6;
		AH += 1;
		AF = 1;
		CF = 1;
	}
	else {
		AF = 0;
		CF = 0;
	}
	AL &= 0x0F;
}
static void aas(I8086* cpu) {
	/* ASCII Adjust for Subtraction (3F) b00111111 */
	if ((AL & 0x0F) > 9 || AF) {
		AL -= 6;
		AH -= 1;
		AF = 1;
		CF = 1;
	}
	else {
		AF = 0;
		CF = 0;
	}
	AL &= 0x0F;
}
static void aam(I8086* cpu) {
	/* ASCII Adjust for Multiply (D4 0A) b11010100 00001010 */
	uint8_t divisor = FETCH_BYTE(); // undocumented operand; normally 0x0A
	AH = (AL / divisor);
	AL = (AL % divisor);

	SET_CF8(AH);
	SET_OF8(AH);
	SET_PF8(AL);
	SET_SF8(AL);
	SET_ZF(AL);
}
static void aad(I8086* cpu) {
	/* ASCII Adjust for Division (D5 0A) b11010101 00001010 */
	uint8_t divisor = FETCH_BYTE(); // undocumented operand; normally 0x0A
	AL = (AH * divisor) + AL;
	AH = 0;

	SET_CF8(AL);
	SET_OF8(AL);
	SET_PF8(AL);
	SET_SF8(AL);
	SET_ZF(AL);
}
static void salc(I8086* cpu) {
	/* set carry in AL (D6) b11010110 undocumented opcode */
	if (CF) {
		AL = 0xFF;
	}
	else {
		AL = 0;
	}
}

static void inc_reg(I8086* cpu) {
	/* Inc reg16 (40-47) b01000REG */
	alu_inc16(cpu, GET_REG16(cpu->opcode & 7));
}
static void dec_reg(I8086* cpu) {
	/* Dec reg16 (48-4F) b01001REG */
	alu_dec16(cpu, GET_REG16(cpu->opcode & 7));
}

static void push_seg(I8086* cpu) {
	/* Push seg16 (06/0E/16/1E) b000SR110 */
	i8086_push_word(cpu, cpu->segments[SR]);
}
static void pop_seg(I8086* cpu) {
	/* Pop seg16 (07/0F/17/1F) b000SR111 */
	i8086_pop_word(cpu, &cpu->segments[SR]);
}
static void push_reg(I8086* cpu) {
	/* Push reg16 (50-57) b01010REG */
	i8086_push_word(cpu, *GET_REG16(cpu->opcode));
}
static void pop_reg(I8086* cpu) {
	/* Pop reg16 (58-5F) b01011REG */
	i8086_pop_word(cpu, GET_REG16(cpu->opcode));
}
static void push_rm(I8086* cpu) {
	/* Push R/M (FF, R/M reg = 110) b11111111 */
	uint16_t* rm = modrm_get_ptr16(cpu);
	i8086_push_word(cpu, *rm);
}
static void pop_rm(I8086* cpu) {
	/* Pop R/M (8F) b10001111 */
	uint16_t* rm = modrm_get_ptr16(cpu);
	i8086_pop_word(cpu, rm);
}
static void pushf(I8086* cpu) {
	/* push psw (9C) b10011100 */
	cpu->status.word &= 0x7D5;
	i8086_push_word(cpu, cpu->status.word);
}
static void popf(I8086* cpu) {
	/* pop psw (9D) b10011101 */
	i8086_pop_word(cpu, &cpu->status.word);
	cpu->status.word &= 0x7D5;
}

static void nop(I8086* cpu) {
	/* nop (90) b10010000 */
	(void)cpu;
}
static void xchg_accum_reg(I8086* cpu) {
	/* xchg AX, reg16 (91 - 97) b10010REG */	
	uint16_t tmp = AX;
	uint16_t* reg = GET_REG16(cpu->opcode);
	AX = *reg;
	*reg = tmp;
}
static void xchg_rm_reg(I8086* cpu) {
	/* xchg R/M, reg16 (86/87) b1000011W */
	cpu->modrm.byte = FETCH_BYTE();
	if (W) {
		uint16_t* rm = modrm_get_ptr16(cpu);
		uint16_t* reg = GET_REG16(cpu->modrm.reg);
		uint16_t tmp = *rm;
		*rm = *reg;
		*reg = tmp;
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		uint8_t* reg = GET_REG8(cpu->modrm.reg);
		uint8_t tmp = *rm;
		*rm = *reg;
		*reg = tmp;
	}
}

static void cbw(I8086* cpu) {
	/* Convert byte to word (98) b10011000 */
	if (AL & 0x80) {
		AH = 0xFF;
	}
	else {
		AH = 0;
	}
}
static void cwd(I8086* cpu) {
	/* Convert word to dword (99) b10011001 */
	if (AX & 0x8000) {
		DX = 0xFFFF;
	}
	else {
		DX = 0;
	}
}

static void wait(I8086* cpu) {
	/* wait (9B) b10011011 */
	if (!cpu->test_line) {
		IP -= 1;
	}
}

static void sahf(I8086* cpu) {
	/* Store AH into flags (9E) b10011110 */
	cpu->status.word &= 0xFF00;
	cpu->status.word |= AH & 0xD5;
}
static void lahf(I8086* cpu) {
	/* Load flags into AH (9F) b10011111 */
	AH = cpu->status.word & 0xD5;
}

static void hlt(I8086* cpu) {
	/* Halt CPU (F4) b11110100 */
	IP -= 1;
}
static void cmc(I8086* cpu) {
	// Complement carry flag (F5) b11110101
	CF = ~CF;
}
static void clc(I8086* cpu) {
	// clear carry flag (F8) b11111000
	CF = 0;
}
static void stc(I8086* cpu) {
	// set carry flag (F9) b11111001
	CF = 1;
}
static void cli(I8086* cpu) {
	// clear interrupt flag (FA) b11111010
	IF = 0;
}
static void sti(I8086* cpu) {
	// set interrupt flag (FB) b1111011
	IF = 1;
}
static void cld(I8086* cpu) {
	// clear direction flag (FC) b11111100
	DF = 0;
}
static void std(I8086* cpu) {
	// set direction flag (FD) b11111101
	DF = 1;
}

static void inc_rm(I8086* cpu) {
	/* Inc R/M (FE/FF, R/M reg = 000) b1111111W */
	if (W) {
		uint16_t* rm = modrm_get_ptr16(cpu);
		alu_inc16(cpu, rm);
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		alu_inc8(cpu, rm);
	}
}
static void dec_rm(I8086* cpu) {
	/* Dec R/M (FE/FF, R/M reg = 001) b1111111W */
	if (W) {
		uint16_t* rm = modrm_get_ptr16(cpu);
		alu_dec16(cpu, rm);
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		alu_dec8(cpu, rm);
	}
}

static void rol(I8086* cpu) {
	/* Rotate left (D0/D1/D2/D3, R/M reg = 000) b110100VW */
	uint8_t count = 1;
	if (VW) {
		count = CL;
	}
	if (W) {
		uint16_t* rm = modrm_get_ptr16(cpu);
		alu_rol16(cpu, rm, count);
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		alu_rol8(cpu, rm, count);
	}
}
static void ror(I8086* cpu) {
	/* Rotate left (D0/D1/D2/D3, R/M reg = 001) b110100VW */
	uint8_t count = 1;
	if (VW) {
		count = CL;
	}
	if (W) {
		uint16_t* rm = modrm_get_ptr16(cpu);
		alu_ror16(cpu, rm, count);
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		alu_ror8(cpu, rm, count);
	}
}
static void rcl(I8086* cpu) {
	/* Rotate through carry left (D0/D1/D2/D3, R/M reg = 010) b110100VW */
	uint8_t count = 1;
	if (VW) {
		count = CL;
	}
	if (W) {
		uint16_t* rm = modrm_get_ptr16(cpu);
		alu_rcl16(cpu, rm, count);
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		alu_rcl8(cpu, rm, count);
	}
}
static void rcr(I8086* cpu) {
	/* Rotate through carry right (D0/D1/D2/D3, R/M reg = 011) b110100VW */
	uint8_t count = 1;
	if (VW) {
		count = CL;
	}
	if (W) {
		uint16_t* rm = modrm_get_ptr16(cpu);
		alu_rcr16(cpu, rm, count);
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		alu_rcr8(cpu, rm, count);
	}
}
static void shl(I8086* cpu) {
	/* Shift left (D0/D1/D2/D3, R/M reg = 100) b110100VW */
	uint8_t count = 1;
	if (VW) {
		count = CL;
	}
	if (W) {
		uint16_t* rm = modrm_get_ptr16(cpu);
		alu_shl16(cpu, rm, count);
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		alu_shl8(cpu, rm, count);
	}
}
static void shr(I8086* cpu) {
	/* Shift Logical right (D0/D1/D2/D3, R/M reg = 101) b110100VW */
	uint8_t count = 1;
	if (cpu->opcode & 0x2) {
		count = CL;
	}
	if (W) {
		uint16_t* rm = modrm_get_ptr16(cpu);
		alu_shr16(cpu, rm, count);
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		alu_shr8(cpu, rm, count);
	}
}
static void sar(I8086* cpu) {
	/* Shift Arithmetic right (D0/D1/D2/D3, R/M reg = 111) b110100VW */
	uint8_t count = 1;
	if (cpu->opcode & 0x2) {
		count = CL;
	}
	if (W) {
		uint16_t* rm = modrm_get_ptr16(cpu);
		alu_sar16(cpu, rm, count);
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		alu_sar8(cpu, rm, count);
	}
}

static void jcc(I8086* cpu) {
	/* conditional jump(70-7F) b011XCCCC
	   8086 cpu decode 60-6F the same as 70-7F */
	int8_t offset = (int8_t)FETCH_BYTE();
	switch (CCCC) {
		case X86_JCC_JO:
			if (OF) IP += offset;
			break;
		case X86_JCC_JNO:
			if (!OF) IP += offset;
			break;
		case X86_JCC_JC:
			if (CF) IP += offset;
			break;
		case X86_JCC_JNC:
			if (!CF) IP += offset;
			break;
		case X86_JCC_JZ:
			if (ZF) IP += offset;
			break;
		case X86_JCC_JNZ:
			if (!ZF) IP += offset;
			break;
		case X86_JCC_JBE:
			if (CF || ZF) IP += offset;
			break;
		case X86_JCC_JA:
			if (!CF && !ZF) IP += offset;
			break;
		case X86_JCC_JS:
			if (SF) IP += offset;
			break;
		case X86_JCC_JNS:
			if (!SF) IP += offset;
			break;
		case X86_JCC_JPE:
			if (PF) IP += offset;
			break;
		case X86_JCC_JPO:
			if (!PF) IP += offset;
			break;
		case X86_JCC_JL:
			if (SF != OF) IP += offset;
			break;
		case X86_JCC_JGE:
			if (SF == OF) IP += offset;
			break;
		case X86_JCC_JLE:
			if (ZF || SF != OF) IP += offset;
			break;
		case X86_JCC_JG:
			if (!ZF && SF == OF) IP += offset;
			break;
	}
}

static void jmp_intra_direct_short(I8086* cpu) {
	/* Jump near short (EB) b11101011 */
	int8_t imm = (int8_t)FETCH_BYTE();
	IP += imm;
}
static void jmp_intra_direct(I8086* cpu) {
	/* Jump near (E9) b11101001 */
	int16_t imm = (int16_t)FETCH_WORD();
	IP += imm;
}
static void jmp_intra_indirect(I8086* cpu) {
	/* Jump intra indirect (FF, R/M reg = 100) b11111111 */	
	uint16_t* rm = (uint16_t*)modrm_get_ptr16(cpu);
	IP = *rm;
}

static void jmp_inter_direct(I8086* cpu) {
	/* Jump addr:seg (EA) b11101010 */
	uint16_t imm = FETCH_WORD();
	uint16_t imm2 = FETCH_WORD();
	IP = imm;
	CS = imm2;
}
static void jmp_inter_indirect(I8086* cpu) {
	/* Jump inter indirect (FF, R/M reg = 101) b11111111 */
	uint16_t* rm = modrm_get_ptr16(cpu);
	IP = *rm;
	CS = *(rm + 1);
}

static void call_intra_direct(I8086* cpu) {
	/* Call disp (E8) b11101000 */
	int16_t imm = (int16_t)FETCH_WORD();
	i8086_push_word(cpu, IP);
	IP += imm;
}
static void call_intra_indirect(I8086* cpu) {
	/* Call mem/reg (FF, R/M reg = 010) b11111111 */
	i8086_push_word(cpu, IP);
	uint16_t* rm = (uint16_t*)modrm_get_ptr16(cpu);
	IP = *rm;
}

static void call_inter_direct(I8086* cpu) {
	/* Call addr:seg (9A) b10011010 */
	uint16_t imm = FETCH_WORD();
	uint16_t imm2 = FETCH_WORD();
	i8086_push_word(cpu, CS);
	i8086_push_word(cpu, IP);
	IP = imm;
	CS = imm2;
}
static void call_inter_indirect(I8086* cpu) {
	/* Call mem (FF, R/M reg = 011) b11111111 */	
	i8086_push_word(cpu, CS);
	i8086_push_word(cpu, IP);
	uint16_t* rm = modrm_get_ptr16(cpu);
	IP = *rm;
	CS = *(rm + 1);
}

static void ret_intra_add_imm(I8086* cpu) {
	/* Ret imm16 (C2) b11000010 
	   undocumented* C0 decodes identically to C2 */
	uint16_t imm = FETCH_WORD();
	i8086_pop_word(cpu, &IP);
	SP += imm;
}
static void ret_intra(I8086* cpu) {
	/* Ret (C3) b11000011 
	undocumented* C1 decodes identically to C3 */
	i8086_pop_word(cpu, &IP);
}
static void ret_inter_add_imm(I8086* cpu) {
	/* Ret imm16 (CA) b11001010 
	undocumented* C8 decodes identically to CA */
	uint16_t imm = FETCH_WORD();
	i8086_pop_word(cpu, &IP);
	i8086_pop_word(cpu, &CS);
	SP += imm;
}
static void ret_inter(I8086* cpu) {
	/* Ret (CB) b11001011 
	undocumented* C9 decodes identically to CB */
	i8086_pop_word(cpu, &IP);
	i8086_pop_word(cpu, &CS);
}

static void mov_rm_imm(I8086* cpu) {
	/* mov r/m, imm (C6/C7) b1100011W */
	cpu->modrm.byte = FETCH_BYTE();
	if (W) {
		uint16_t* rm = modrm_get_ptr16(cpu);
		uint16_t imm = FETCH_WORD();
		*rm = imm;
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		uint8_t imm = FETCH_BYTE();
		*rm = imm;
	}
}
static void mov_reg_imm(I8086* cpu) {
	/* mov r/m, reg (B0-BF) b1011WREG */
	if (WREG) {
		uint16_t imm = FETCH_WORD();
		uint16_t* reg = GET_REG16(cpu->opcode);
		*reg = imm;
	}
	else {
		uint8_t imm = FETCH_BYTE();
		uint8_t* reg = GET_REG8(cpu->opcode);
		*reg = imm;
	}
}
static void mov_rm_reg(I8086* cpu) {
	/* mov r/m, reg (88/89/8A/8B) b100010DW */
	cpu->modrm.byte = FETCH_BYTE();
	if (W) {
		uint16_t* rm = modrm_get_ptr16(cpu);
		uint16_t* reg = GET_REG16(cpu->modrm.reg);
		get_direction(cpu, &reg, &rm);
		*rm = *reg;
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		uint8_t* reg = GET_REG8(cpu->modrm.reg);
		get_direction(cpu, &reg, &rm);
		*rm = *reg;
	}
}
static void mov_accum_mem(I8086* cpu) {
	/* mov AL/AX, [mem] (A0/A1/A2/A3) b101000DW */
	uint16_t addr = FETCH_WORD();
	if (W) {
		uint16_t* mem = GET_MEM_PTR(DATA_ADDR(addr));
		uint16_t* reg = GET_REG16(REG_AX);
		get_direction(cpu, &reg, &mem);
		*reg = *mem;
	}
	else {
		uint8_t* mem = GET_MEM_PTR(DATA_ADDR(addr));
		uint8_t* reg = GET_REG8(REG_AL);
		get_direction(cpu, &reg, &mem);
		*reg = *mem;
	}
}
static void mov_seg(I8086* cpu) {
	/* mov r/m, seg (8C/8E) b100011D0 */
	cpu->modrm.byte = FETCH_BYTE();
	uint16_t* rm = modrm_get_ptr16(cpu);
	uint16_t* reg = &cpu->segments[cpu->modrm.reg & 3];
	get_direction(cpu, &reg, &rm);
	*rm = *reg;
}

static void lea(I8086* cpu) {
	/* lea reg16, [r/m] (8D) b10001101 */
	cpu->modrm.byte = FETCH_BYTE();
	uint16_t* reg = GET_REG16(cpu->modrm.reg);
	uint16_t addr = modrm_get_addr(cpu);
	*reg = addr;
}

static void not(I8086* cpu) {
	/* not reg (F6/F7, R/M reg = b010) b1111011W */
	if (W) {
		uint16_t* reg = GET_REG16(cpu->modrm.rm);
		*reg = ~*reg;
	}
	else {
		uint8_t* reg = GET_REG8(cpu->modrm.rm);
		*reg = ~*reg;
	}
}
static void neg(I8086* cpu) {
	/* neg reg (F6/F7, R/M reg = b011) b1111011W */
	if (W) {
		uint16_t* reg = GET_REG16(cpu->modrm.rm);
		*reg = 0 - *reg;
	}
	else {
		uint8_t* reg = GET_REG8(cpu->modrm.rm);
		*reg = 0 - *reg;
	}
}
static void mul(I8086* cpu) {
	/* mul r/m (F6/F7, R/M reg = b100) b1111011W */
	if (W) {
		uint16_t* mul = modrm_get_ptr16(cpu);
		uint32_t r = AX * *mul;
		AX = ((r >> 16) & 0xFFFF);
		DX = (r & 0xFFFF);
	}
	else {
		uint8_t* mul = modrm_get_ptr8(cpu);
		uint8_t al = AL;
		AX = al * *mul;
	}
}
static void imul(I8086* cpu) {
	/* imul r/m (F6/F7, R/M reg = b101) b1111011W */
	if (W) {
		int16_t* mul = (int16_t*)modrm_get_ptr16(cpu);
		int32_t r = AX * *mul;
		AX = ((r >> 16) & 0xFFFF);
		DX = (r & 0xFFFF);
	}
	else {
		int8_t* mul = (int8_t*)modrm_get_ptr8(cpu);
		int8_t al = AL;
		AX = al * *mul;
	}
}
static void div(I8086* cpu) {
	/* div r/m (F6/F7, R/M reg = b110) b1111011W */
	if (W) {
		uint16_t* divider = modrm_get_ptr16(cpu);
		uint32_t dividend = (AX << 16) | DX;
		AX = (dividend / *divider) & 0xFFFF; // quotient
		DX = dividend % *divider; // remainder
	}
	else {
		uint8_t* divider = modrm_get_ptr8(cpu);
		uint16_t dividend = AX;
		AH = (dividend / *divider) & 0xFF; // quotient
		AL = dividend % *divider; // remainder
	}
}
static void idiv(I8086* cpu) {
	/* idiv r/m (F6/F7, R/M reg = b111) b1111011W */
	if (W) {
		int16_t* divider = (int16_t*)modrm_get_ptr16(cpu);
		int32_t dividend = (AX << 16) | DX;
		AX = (dividend / *divider) & 0xFFFF; // quotient
		DX = dividend % *divider; // remainder
	}
	else {
		int8_t* divider = (int8_t*)modrm_get_ptr8(cpu);
		int16_t dividend = AX;
		AL = (dividend / *divider) & 0xFF; // quotient
		AH = dividend % *divider; // remainder
	}
}

#define REP_NONE     0xFF
#define REP_NOT_ZERO 0
#define REP_ZERO     1

static int movs(I8086* cpu) {
	/* movs (A4/A5) b1010010W */
	if (W) {
		uint16_t* src = GET_MEM_PTR(DATA_ADDR(SI));
		uint16_t* dest = GET_MEM_PTR(EXTRA_ADDR(DI));
		*dest = *src;
	}
	else {
		uint8_t* src = GET_MEM_PTR(DATA_ADDR(SI));
		uint8_t* dest = GET_MEM_PTR(EXTRA_ADDR(DI));
		*dest = *src;
	}

	if (DF) {
		SI += (1 << W);
		DI += (1 << W);

	}
	else {
		SI -= (1 << W);
		DI -= (1 << W);
	}

	if (cpu->rep_prefix != 0xFF) {
		CX -= 1;
		if (CX > 0)
			return DECODE_REQ_CYCLE;
		else
			return DECODE_OK;
	}
	return DECODE_OK;
}
static int stos(I8086* cpu) {
	/* stos (AA/AB) b1010101W */
	if (W) {
		WRITE_WORD(EXTRA_ADDR(DI), AX);
	}
	else {
		WRITE_BYTE(EXTRA_ADDR(DI), AL);
	}

	if (DF) {
		DI -= (1 << W);

	}
	else {
		DI += (1 << W);
	}

	if (cpu->rep_prefix != 0xFF) {
		CX -= 1;
		if (CX > 0)
			return DECODE_REQ_CYCLE;
		else
			return DECODE_OK;
	}
	return DECODE_OK;
}
static int lods(I8086* cpu) {
	/* lods (AC/AD) b1010110W */
	if (W) {
		AX = READ_WORD(DATA_ADDR(SI));
	}
	else {
		AL = READ_BYTE(DATA_ADDR(SI));		
	}

	if (DF) {
		SI -= (1 << W);

	}
	else {
		SI += (1 << W);
	}

	if (cpu->rep_prefix != 0xFF) {
		CX -= 1;
		if (CX > 0)
			return DECODE_REQ_CYCLE;
		else
			return DECODE_OK;
	}
	return DECODE_OK;
}
static int cmps(I8086* cpu) {
	/* cmps (A6/A7) b1010011W */
	if (W) {
		uint16_t* src = GET_MEM_PTR(DATA_ADDR(SI));
		uint16_t* dest = GET_MEM_PTR(EXTRA_ADDR(DI));
		alu_cmp16(cpu, *src, *dest);
	}
	else {
		uint8_t* src = GET_MEM_PTR(DATA_ADDR(SI));
		uint8_t* dest = GET_MEM_PTR(EXTRA_ADDR(DI));
		alu_cmp8(cpu, *src, *dest);
	}

	if (DF) {
		SI -= (1 << W);
		DI -= (1 << W);
	}
	else {
		SI += (1 << W);
		DI += (1 << W);
	}

	if (cpu->rep_prefix != 0xFF) {
		CX -= 1;
		if (CX > 0 && ZF == cpu->rep_prefix)
			return DECODE_REQ_CYCLE;
		else
			return DECODE_OK;
	}
	return DECODE_OK;
}
static int scas(I8086* cpu) {
	/* scas (AE/AF) b1010111W */
	if (W) {
		uint16_t* mem = GET_MEM_PTR(EXTRA_ADDR(DI));
		alu_sub16(cpu, &AX, *mem);
	}
	else {
		uint8_t* mem = GET_MEM_PTR(EXTRA_ADDR(DI));
		alu_sub8(cpu, &AL, *mem);
	}

	if (DF) {
		DI -= (1 << W);

	}
	else {
		DI += (1 << W);
	}

	if (cpu->rep_prefix != 0xFF) {
		CX -= 1;
		if (CX > 0 && ZF == cpu->rep_prefix)
			return DECODE_REQ_CYCLE;
		else
			return DECODE_OK;
	}
	return DECODE_OK;
}

static void les(I8086* cpu) {
	/* les (C4) b11000100 */
	cpu->modrm.byte = FETCH_BYTE();
	uint16_t* reg = GET_REG16(cpu->modrm.reg);
	uint16_t* rm = modrm_get_ptr16(cpu);
	*reg = *rm;
	ES = *(rm + 1);
}
static void lds(I8086* cpu) {
	/* lds (C5) b11000101 */
	cpu->modrm.byte = FETCH_BYTE();
	uint16_t* reg = GET_REG16(cpu->modrm.reg);
	uint16_t* rm = modrm_get_ptr16(cpu);
	*reg = *rm;
	DS = *(rm + 1);
}

static void xlat(I8086* cpu) {
	/* Get data pointed by BX + AL (D7) b11010111 */
	uint8_t* mem = GET_MEM_PTR(DATA_ADDR(BX + AL));
	AL = *mem;
}

static void esc(I8086* cpu) {
	/* esc (D8-DF R/M reg = XXX) b11010REG */
	cpu->modrm.byte = FETCH_BYTE();
	if (cpu->modrm.mod != 0b11) {
		uint8_t esc_opcode = ((cpu->opcode & 7) << 3) | cpu->modrm.reg;
		uint16_t* reg = GET_REG16(cpu->opcode);
		uint16_t* rm = modrm_get_ptr16(cpu);
		(void)esc_opcode;
		(void)reg;
		(void)rm;
	}
}

static void loopnz(I8086* cpu) {
	/* loop while not zero (E0) b1110000Z */
	int8_t disp = FETCH_BYTE();
	CX -= 1;
	if (CX && !ZF) {
		IP += disp;
	}
}
static void loopz(I8086* cpu) {
	/* loop while zero (E1) b1110000Z */
	int8_t disp = FETCH_BYTE();
	CX -= 1;
	if (CX && ZF) {
		IP += disp;
	}
}
static void loop(I8086* cpu) {
	/* loop if CX not zero (E2) b11100010 */
	int8_t disp = FETCH_BYTE();
	CX -= 1;
	if (CX) {
		IP += disp;
	}
}
static void jcxz(I8086* cpu) {
	/* jump if CX zero (E3) b11100011 */
	int8_t disp = FETCH_BYTE();
	if (CX == 0) {
		IP += disp;
	}
}

static void in_accum_imm(I8086* cpu) {
	/* in AL/AX, imm */
	uint8_t imm = FETCH_BYTE();
	if (W) {
		AX = READ_IO_WORD(imm);
	}
	else {
		AL = READ_IO(imm);
	}
}
static void out_accum_imm(I8086* cpu) {
	/* out imm, AL/AX */
	uint8_t imm = FETCH_BYTE();
	if (W) {
		WRITE_IO_WORD(imm, AX);
	}
	else {
		WRITE_IO(imm, AL);
	}
}
static void in_accum_dx(I8086* cpu) {
	/* in AL/AX, DX */
	if (W) {
		AX = READ_IO_WORD(DX);
	}
	else {
		AL = READ_IO(DX);
	}
}
static void out_accum_dx(I8086* cpu) {
	/* out DX, AL/AX */
	if (W) {
		WRITE_IO_WORD(DX, AX);
	}
	else {
		WRITE_IO(DX, AL);
	}
}

static void int_(I8086* cpu) {
	/* interrupt (CC/CD) b1100110V */
	uint8_t type = 3;
 	if (cpu->opcode & 0x1) {
		type = FETCH_BYTE();
	}
	i8086_int(cpu, type);
}
static void into(I8086* cpu) {
	/* interrupt on overflow (CE) b11001110 */
	if (OF) {
		i8086_int(cpu, 4);
	}
}
static void iret(I8086* cpu) {
	/* interrupt on return (CF) b11001111 */
	i8086_pop_word(cpu, &IP);
	i8086_pop_word(cpu, &CS);
	i8086_pop_word(cpu, &cpu->status.word);
}

/* prefix byte */
static int rep(I8086* cpu) {
	/* rep/repz/repnz (F2/F3) b1111001Z */
	cpu->rep_prefix = cpu->opcode & 0x1;
	cpu->opcode = FETCH_BYTE();
	return DECODE_REQ_CYCLE;
}
static int segment_override(I8086* cpu) {
	/* (26/2E/36/3E) b001SR110 */
	cpu->segment_prefix = SR;
	cpu->opcode = FETCH_BYTE();
	return DECODE_REQ_CYCLE;
}
static int lock(I8086* cpu) {
	/* lock the bus (F0) b11110000 */
	cpu->opcode = FETCH_BYTE();
	return DECODE_REQ_CYCLE;
}

/* Fetch next opcode */
static void i8086_fetch(I8086* cpu) {
	cpu->modrm.byte = 0;
	cpu->segment_prefix = 0xFF;
	cpu->rep_prefix = 0xFF;
	cpu->opcode = FETCH_BYTE();
}

/* decode opcode */
static void i8086_decode_opcode_80(I8086* cpu) {
	/* 0x80 - 0x83 b100000SW */
	cpu->modrm.byte = FETCH_BYTE();
	switch (cpu->modrm.reg) {
		case 0b000: // ADD
			add_rm_imm(cpu);
			break;
		case 0b001: // OR
			or_rm_imm(cpu);
			break;
		case 0b010: // ADC
			adc_rm_imm(cpu);
			break;
		case 0b011: // SBB
			sbb_rm_imm(cpu);
			break;
		case 0b100: // AND
			and_rm_imm(cpu);
			break;
		case 0b101: // SUB
			sub_rm_imm(cpu);
			break;
		case 0b110: // XOR
			xor_rm_imm(cpu);
			break;
		case 0b111: // CMP
			cmp_rm_imm(cpu);
			break;
	}
}
static void i8086_decode_opcode_d0(I8086* cpu) {
	/* 0xD0 - 0xD3 b110100VW */
	cpu->modrm.byte = FETCH_BYTE();
	switch (cpu->modrm.reg) {
		case 0b000:
			rol(cpu);
			break;
		case 0b001:
			ror(cpu);
			break;
		case 0b010:
			rcl(cpu);
			break;
		case 0b011:
			rcr(cpu);
			break;
		case 0b100:
			shl(cpu);
			break;
		case 0b101:
			shr(cpu);
			break;
		case 0b111:
			sar(cpu);
			break;
	}
}
static void i8086_decode_opcode_f6(I8086* cpu) {
	/* F6/F7 b1111011W */
	cpu->modrm.byte = FETCH_BYTE();
	switch (cpu->modrm.reg) {
		case 0b000:
			test_rm_imm(cpu);
			break;
		case 0b010:
			not(cpu);
			break;
		case 0b011:
			neg(cpu);
			break;
		case 0b100:
			mul(cpu);
			break;
		case 0b101:
			imul(cpu);
			break;
		case 0b110:
			div(cpu);
			break;
		case 0b111:
			idiv(cpu);
			break;
	}
}
static void i8086_decode_opcode_fe(I8086* cpu) {
	/* FE/FF b1111111W */
	cpu->modrm.byte = FETCH_BYTE();
	switch (cpu->modrm.reg) {
		case 0b000:
			inc_rm(cpu);
			break;
		case 0b001:
			dec_rm(cpu);
			break;
		case 0b010:
			call_intra_indirect(cpu);
			break;
		case 0b011:
			call_inter_indirect(cpu);
			break;
		case 0b100:
			jmp_intra_indirect(cpu);
			break;
		case 0b101:
			jmp_inter_indirect(cpu);
			break;
		case 0b110:
			push_rm(cpu);
			break;
	}
}

static int i8086_decode_opcode(I8086 * cpu) {
	switch (cpu->opcode) {
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
			add_rm_reg(cpu);
			break;
		case 0x04:
		case 0x05:
			add_accum_imm(cpu);
			break;
		case 0x06:
			push_seg(cpu);
			break;
		case 0x07:
			pop_seg(cpu);
			break;
		case 0x08:
		case 0x09:
		case 0x0A:
		case 0x0B:
			or_rm_reg(cpu);
			break;
		case 0x0C:
		case 0x0D:
			or_accum_imm(cpu);
			break;
		case 0x0E:
			push_seg(cpu);
			break;
		case 0x0F: // pop cs; 8086 undocumented
			pop_seg(cpu);
			break;
		
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
			adc_rm_reg(cpu);
			break;
		case 0x14:
		case 0x15:
			adc_accum_imm(cpu);
			break;
		case 0x16:
			push_seg(cpu);
			break;
		case 0x17:
			pop_seg(cpu);
			break;
		case 0x18:
		case 0x19:
		case 0x1A:
		case 0x1B:
			sbb_rm_reg(cpu);
			break;
		case 0x1C:
		case 0x1D:
			sbb_accum_imm(cpu);
			break;
		case 0x1E:
			push_seg(cpu);
			break;
		case 0x1F:
			pop_seg(cpu);
			break;
		
		case 0x20:
		case 0x21:
		case 0x22:
		case 0x23:
			and_rm_reg(cpu);
			break;
		case 0x24:
		case 0x25:
			and_accum_imm(cpu);
			break;
		case 0x26:
			return segment_override(cpu);
		case 0x27:
			daa(cpu);
			break;
		case 0x28:
		case 0x29:
		case 0x2A:
		case 0x2B:
			sub_rm_reg(cpu);
			break;
		case 0x2C:
		case 0x2D:
			sub_accum_imm(cpu);
			break;
		case 0x2E:
			return segment_override(cpu);
		case 0x2F:
			das(cpu);
			break;
		
		case 0x30:
		case 0x31:
		case 0x32:
		case 0x33:
			xor_rm_reg(cpu);
			break;
		case 0x34:
		case 0x35:
			xor_accum_imm(cpu);
			break;
		case 0x36:
			return segment_override(cpu);
		case 0x37:
			aaa(cpu);
			break;
		case 0x38:
		case 0x39:
		case 0x3A:
		case 0x3B:
			cmp_rm_reg(cpu);
			break;
		case 0x3C:
		case 0x3D:
			cmp_accum_imm(cpu);
			break;
		case 0x3E:
			return segment_override(cpu);
		case 0x3F:
			aas(cpu);
			break;

		case 0x40:
		case 0x41:
		case 0x42:
		case 0x43:
		case 0x44:
		case 0x45:
		case 0x46:
		case 0x47:
			inc_reg(cpu);
			break;

		case 0x48:
		case 0x49:
		case 0x4A:
		case 0x4B:
		case 0x4C:
		case 0x4D:
		case 0x4E:
		case 0x4F:
			dec_reg(cpu);
			break;

		case 0x50:
		case 0x51:
		case 0x52:
		case 0x53:
		case 0x54:
		case 0x55:
		case 0x56:
		case 0x57:
			push_reg(cpu);
			break;

		case 0x58:
		case 0x59:
		case 0x5A:
		case 0x5B:
		case 0x5C:
		case 0x5D:
		case 0x5E:
		case 0x5F:
			pop_reg(cpu);
			break;

		// 8086 undocumented
		case 0x60:
		case 0x61:
		case 0x62:
		case 0x63:
		case 0x64:
		case 0x65:
		case 0x66:
		case 0x67:
		case 0x68:
		case 0x69:
		case 0x6A:
		case 0x6B:
		case 0x6C:
		case 0x6D:
		case 0x6E:
		case 0x6F:
			jcc(cpu);
			break;

		case 0x70:
		case 0x71:
		case 0x72:
		case 0x73:
		case 0x74:
		case 0x75:
		case 0x76:
		case 0x77:
		case 0x78:
		case 0x79:
		case 0x7A:
		case 0x7B:
		case 0x7C:
		case 0x7D:
		case 0x7E:
		case 0x7F:
			jcc(cpu);
			break;

		case 0x80:
		case 0x81:
		case 0x82:
		case 0x83:
			i8086_decode_opcode_80(cpu);
			break;
		case 0x84:
		case 0x85:
			test_rm_reg(cpu);
			break;
		case 0x86:
		case 0x87:
			xchg_rm_reg(cpu);
			break;
		case 0x88:
		case 0x89:
		case 0x8A:
		case 0x8B:
			mov_rm_reg(cpu);
			break;
		case 0x8C:
			mov_seg(cpu);
			break;
		case 0x8D:
			lea(cpu);
			break;
		case 0x8E:
			mov_seg(cpu);
			break;
		case 0x8F:
			pop_rm(cpu);
			break;

		case 0x90:
			nop(cpu);
			break;
		case 0x91:
		case 0x92:
		case 0x93:
		case 0x94:
		case 0x95:
		case 0x96:
		case 0x97:
			xchg_accum_reg(cpu);
			break;
		case 0x98:
			cbw(cpu);
			break;
		case 0x99:
			cwd(cpu);
			break; 
		case 0x9A:
			call_inter_direct(cpu);
			break;
		case 0x9B:
			wait(cpu);
			break;
		case 0x9C:
			pushf(cpu);
			break;
		case 0x9D:
			popf(cpu);
			break;
		case 0x9E:
			sahf(cpu);
			break;
		case 0x9F:
			lahf(cpu);
			break;

		case 0xA0:
		case 0xA1:
		case 0xA2:
		case 0xA3:
			mov_accum_mem(cpu);
			break;
		case 0xA4:
		case 0xA5:
			return movs(cpu);
		case 0xA6:
		case 0xA7:
			return cmps(cpu);
		case 0xA8:
		case 0xA9:
			test_accum_imm(cpu);
			break;
		case 0xAA:
		case 0xAB:
			return stos(cpu);
		case 0xAC:
		case 0xAD:
			return lods(cpu);
		case 0xAE:
		case 0xAF:
			return scas(cpu);

		case 0xB0:
		case 0xB1:
		case 0xB2:
		case 0xB3:
		case 0xB4:
		case 0xB5:
		case 0xB6:
		case 0xB7:
		case 0xB8:
		case 0xB9:
		case 0xBA:
		case 0xBB:
		case 0xBC:
		case 0xBD:
		case 0xBE:
		case 0xBF:
			mov_reg_imm(cpu);
			break;

		case 0xC0: // 8086 undocumented
		case 0xC2:
			ret_intra_add_imm(cpu);
			break;
		case 0xC1: // 8086 undocumented
		case 0xC3:
			ret_intra(cpu);
			break;
		case 0xC4:
			les(cpu);
			break;
		case 0xC5:
			lds(cpu);
			break;
		case 0xC6:
		case 0xC7:
			mov_rm_imm(cpu);
			break;
		case 0xC8: // 8086 undocumented
		case 0xCA:
			ret_inter_add_imm(cpu);
			break;
		case 0xC9: // 8086 undocumented
		case 0xCB:
			ret_inter(cpu);
			break;
		case 0xCC:
		case 0xCD:
			int_(cpu);
			break;
		case 0xCE:
			into(cpu);
			break;
		case 0xCF:
			iret(cpu);
			break;
			
		case 0xD0:
		case 0xD1:
		case 0xD2:
		case 0xD3:
			i8086_decode_opcode_d0(cpu);
			break;
		case 0xD4:
			aam(cpu);
			break;
		case 0xD5:
			aad(cpu);
			break;
		case 0xD6: // 8086 undocumented
			salc(cpu);
			break;
		case 0xD7:
			xlat(cpu);
			break;
		case 0xD8:
		case 0xD9:
		case 0xDA:
		case 0xDB:
		case 0xDC:
		case 0xDD:
		case 0xDE:
		case 0xDF:
			esc(cpu);
			break;

		case 0xE0:
			loopnz(cpu);
			break;
		case 0xE1:
			loopz(cpu);
			break;
		case 0xE2:
			loop(cpu);
			break;
		case 0xE3:
			jcxz(cpu);
			break;
		case 0xE4:
		case 0xE5:
			in_accum_imm(cpu);
			break;
		case 0xE6:
		case 0xE7:
			out_accum_imm(cpu);
			break;
		case 0xE8:
			call_intra_direct(cpu);
			break;
		case 0xE9:
			jmp_intra_direct(cpu);
			break;
		case 0xEA:
			jmp_inter_direct(cpu);
			break;
		case 0xEB:
			jmp_intra_direct_short(cpu);
			break;
		case 0xEC:
		case 0xED:
			in_accum_dx(cpu);
			break;
		case 0xEE:
		case 0xEF:
			out_accum_dx(cpu);
			break;

		case 0xF0:
			return lock(cpu);
		case 0xF2:
		case 0xF3:
			return rep(cpu);
		case 0xF4:
			hlt(cpu);
			break;
		case 0xF5:
			cmc(cpu);
			break;
		case 0xF6:
		case 0xF7:
			i8086_decode_opcode_f6(cpu);
			break;
		case 0xF8:
			clc(cpu);
			break;
		case 0xF9:
			stc(cpu);
			break;
		case 0xFA:
			cli(cpu);
			break;
		case 0xFB:
			sti(cpu);
			break;
		case 0xFC:
			cld(cpu);
			break;
		case 0xFD:
			std(cpu);
			break;
		case 0xFE:
		case 0xFF:
			i8086_decode_opcode_fe(cpu);
			break;
	}
	return DECODE_OK;
}

void i8086_init(I8086* cpu) {
	cpu->mm.mem = NULL;
	cpu->mm.funcs.read_mem_byte = NULL;
	cpu->mm.funcs.write_mem_byte = NULL;
	cpu->mm.funcs.read_mem_word = NULL;
	cpu->mm.funcs.write_mem_word = NULL;
	cpu->mm.funcs.get_mem_ptr = NULL;
	cpu->mm.funcs.read_io_byte = NULL;
	cpu->mm.funcs.write_io_byte = NULL;
	cpu->mm.funcs.read_io_word = NULL;
	cpu->mm.funcs.write_io_word = NULL;
}
void i8086_reset(I8086* cpu) {
	for (int i = 0; i < REGISTER_COUNT; ++i) {
		cpu->registers[i].r16 = 0;
	}

	for (int i = 0; i < SEGMENT_COUNT; ++i) {
		cpu->segments[i] = 0;
	}

	IP = 0;
	CS = 0xFFFF;

	cpu->status.word = 0;
	cpu->opcode = 0;
	cpu->modrm.byte = 0;
}

int i8086_execute(I8086* cpu) {
	i8086_fetch(cpu);
	 
	int r = 0;
	do {
		r = i8086_decode_opcode(cpu);
	} while (r == DECODE_REQ_CYCLE); 
	return r;
}
