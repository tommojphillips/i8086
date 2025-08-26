/* i8086_mnem.c
 * Thomas J. Armytage 2025 ( https://github.com/tommojphillips/ )
 * Intel 8086 Mnemonics
 */

#define I8086_ENABLE_MNEM
#ifdef I8086_ENABLE_MNEM

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "i8086.h"
#include "i8086_mnem.h"

 //#define CONVERT_REL_TO_ABS
#ifdef CONVERT_REL_TO_ABS
#include "sign_extend.h"
/* Relative offset to Absolute offset*/
#define TO_LABEL_OFFSET(x) (IP+x)
#endif

#define CS mnem->state->segments[SEG_CS] // code segment register

 /* define IP as counter */
#define IP  (mnem->offset + mnem->counter)

/* Get 20bit address SEG:ADDR */
#define GET_ADDR(addr) (((mnem->segment << 4) + (addr)))

 /* Get 20bit code segment address CS:ADDR */
#define IP_ADDR            GET_ADDR(IP)

/* Fetch word at CS:IP. Inc IP by 2 */
#define FETCH_BYTE()       fetch_byte(mnem)//cpu->state->funcs.read_mem_byte(IP_ADDR); cpu->counter += 1

/* Fetch word at CS:IP. Inc IP by 2 */
#define FETCH_WORD()       fetch_word(mnem)//cpu->state->funcs.read_mem_word(IP_ADDR); cpu->counter += 2

/* Get 8bit register mnemonic */
#define GET_REG8(reg)      reg8_mnem[reg & 7]

/* Get 16bit register mnemonic */
#define GET_REG16(reg)     reg16_mnem[reg & 7]

 // byte/word operation. 0 = byte; 1 = word
#define W (mnem->opcode & 0x1)

// byte/word operation. 0 = byte; 1 = word
#define WREG (mnem->opcode & 0x8) 

// byte/word operation. b00 = byte; b01 = word; b11 = byte sign extended to word
#define SW (mnem->opcode & 0x3)

// seg register
#define SR ((mnem->opcode >> 0x3) & 0x3)

// 0 = (count = 1); 1 = (count = CL)
#define VW (mnem->opcode & 0x2)

// register direction (reg -> r/m) or (r/m -> reg)
#define D (mnem->opcode & 0x2) 

// jump condition
#define CCCC (mnem->opcode & 0x0F)

#define MNEM0_F(mem, x) sprintf(mem+strlen(mem), x)
#define MNEM0(x)        MNEM0_F(mnem->str, x)

#define MNEM_F(mem, x, ...) sprintf(mem+strlen(mem), x, __VA_ARGS__)
#define MNEM(x, ...) MNEM_F(mnem->str, x, __VA_ARGS__)

static const char* reg8_mnem[] = {
	"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"
};
static const char* reg16_mnem[] = {
	"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"
};

static const char* seg_mnem[] = {
	"es", "cs", "ss", "ds"
};

/* Get 20bit address SEG:ADDR */
#define PHYS_ADDRESS(seg, offset) ((((uint20_t)seg << 4) + ((offset) & 0xFFFF)) & 0xFFFFF)

static uint8_t read_byte(I8086_MNEM* mnem, uint16_t segment, uint16_t offset) {
	return mnem->state->funcs.read_mem_byte(PHYS_ADDRESS(segment, offset));
}
static void write_byte(I8086_MNEM* mnem, uint16_t segment, uint16_t offset, uint8_t value) {
	mnem->state->funcs.write_mem_byte(PHYS_ADDRESS(segment, offset), value);
}
static uint8_t fetch_byte(I8086_MNEM* mnem) {
	uint8_t v = read_byte(mnem, CS, IP);
	mnem->counter += 1;
	return v;
}

static uint16_t read_word(I8086_MNEM* mnem, uint16_t segment, uint16_t offset) {
	return (((uint16_t)mnem->state->funcs.read_mem_byte(PHYS_ADDRESS(segment, offset + 1)) << 8) | mnem->state->funcs.read_mem_byte(PHYS_ADDRESS(segment, offset)));
}
static void write_word(I8086_MNEM* mnem, uint16_t segment, uint16_t offset, uint16_t value) {
	mnem->state->funcs.write_mem_byte(PHYS_ADDRESS(segment, offset), value & 0xFF);
	mnem->state->funcs.write_mem_byte(PHYS_ADDRESS(segment, offset + 1), (value >> 8) & 0xFF);
}
static uint16_t fetch_word(I8086_MNEM* mnem) {
	uint16_t v = read_word(mnem, CS, IP);
	mnem->counter += 2;
	return v;
}

/* Swap pointers if 'D' is set bXXXXXXDX */
static void get_direction(I8086_MNEM* mnem, void** ptr1, void** ptr2) {
	if (D) {
		void* tmp = *ptr1;
		*ptr1 = *ptr2;
		*ptr2 = tmp;
	}
}

static void get_base_mnem(I8086_MNEM* mnem, char* t) {
	switch (mnem->modrm.rm) {
		case 0b000: // base rel indexed - BX + SI
			MNEM_F(t, "%s + %s", GET_REG16(REG_BX), GET_REG16(REG_SI));
			break;
		case 0b001: // base rel indexed - BX + DI
			MNEM_F(t, "%s + %s", GET_REG16(REG_BX), GET_REG16(REG_DI));
			break;
		case 0b010: // base rel indexed stack - BP + SI
			MNEM_F(t, "%s + %s", GET_REG16(REG_BP), GET_REG16(REG_SI));
			break;
		case 0b011: // base rel indexed stack - BP + DI
			MNEM_F(t, "%s + %s", GET_REG16(REG_BP), GET_REG16(REG_DI));
			break;
		case 0b100: // implied SI
			MNEM_F(t, "%s", GET_REG16(REG_SI));
			break;
		case 0b101: // implied DI
			MNEM_F(t, "%s", GET_REG16(REG_DI));
			break;
		case 0b110: // implied BP
			MNEM_F(t, "%s", GET_REG16(REG_BP));
			break;
		case 0b111: // implied BX
			MNEM_F(t, "%s", GET_REG16(REG_BX));
			break;
	}
}
static void modrm_get_mnem(I8086_MNEM* mnem) {
	// Get R/M pointer
	char t[32] = { 0 };
	switch (mnem->modrm.mod) {

		case 0b00:
			if (mnem->modrm.rm == 0b110) {
				// displacement mode - [ disp16 ]
				uint16_t imm = FETCH_WORD();
				MNEM_F(mnem->addressing_str, "[%04X]", imm);
			}
			else {
				// register mode - [ base16 ]
				get_base_mnem(mnem, t);
				MNEM_F(mnem->addressing_str, "[%s]", t);
			}
			break;

		case 0b01: {
			// memory mode; 8bit displacement - [ base16 + disp8 ]
			uint8_t imm = FETCH_BYTE();
			get_base_mnem(mnem, t);
			MNEM_F(mnem->addressing_str, "[%s + %02X]", t, imm);
		} break;

		case 0b10: {
			// memory mode; 16bit displacement - [ base16 + disp16 ]
			uint16_t imm = FETCH_WORD();
			get_base_mnem(mnem, t);
			MNEM_F(mnem->addressing_str, "[%s + %04X]", t, imm);
		} break;
	}
}

const char* modrm_get_mnem16(I8086_MNEM* mnem) {
	// Get R/M pointer
	if (mnem->modrm.mod == 0b11) {
		return GET_REG16(mnem->modrm.rm);
	}
	else {
		modrm_get_mnem(mnem);
		return mnem->addressing_str;
	}
}
const char* modrm_get_mnem8(I8086_MNEM* mnem) {
	// Get R/M pointer
	if (mnem->modrm.mod == 0b11) {
		return GET_REG8(mnem->modrm.rm);
	}
	else {
		modrm_get_mnem(mnem);
		return mnem->addressing_str;
	}
}

/* Opcodes */

static void add_rm_imm(I8086_MNEM* mnem) {
	/* add r/m, imm (80/81/82/83, R/M reg = b000) b100000SW */
	if (W) {
		if (SW == 0b01) {
			/* reg16, disp16 */
			const char* rm = modrm_get_mnem16(mnem);
			uint16_t imm = FETCH_WORD();
			MNEM("add %s, %04X", rm, imm);
		}
		else {
			/* reg16, disp8 */
			const char* rm = modrm_get_mnem16(mnem);
			uint8_t imm = FETCH_BYTE();
			MNEM("add %s, %02X", rm, imm);
		}
	}
	else {
		const char* rm = modrm_get_mnem8(mnem);
		uint8_t imm = FETCH_BYTE();
		MNEM("add %s, %02X", rm, imm);
	}
}
static void add_rm_reg(I8086_MNEM* mnem) {
	/* add r/m, reg (00/01/02/03) b000000DW */
	mnem->modrm.byte = FETCH_BYTE();
	if (W) {
		const char* rm = modrm_get_mnem16(mnem);
		const char* reg = GET_REG16(mnem->modrm.reg);
		get_direction(mnem, &reg, &rm);
		MNEM("add %s, %s", rm, reg);
	}
	else {
		const char* rm = modrm_get_mnem8(mnem);
		const char* reg = GET_REG8(mnem->modrm.reg);
		get_direction(mnem, &reg, &rm);
		MNEM("add %s, %s", rm, reg);
	}
}
static void add_accum_imm(I8086_MNEM* mnem) {
	/* add AL/AX, imm (04/05) b0000010W */
	if (W) {
		uint16_t imm = FETCH_WORD();
		MNEM("add ax, %04X", imm);
	}
	else {
		uint8_t imm = FETCH_BYTE();
		MNEM("add al, %02X", imm);
	}
}

static void or_rm_imm(I8086_MNEM* mnem) {
	/* or r/m, imm (80/81/82/83, R/M reg = b001) b100000SW */
	if (W) {
		if (SW == 0b01) {
			/* reg16, disp16 */
			const char* rm = modrm_get_mnem16(mnem);
			uint16_t imm = FETCH_WORD();
			MNEM("or %s, %04X", rm, imm);
		}
		else {
			/* reg16, disp8 */
			const char* rm = modrm_get_mnem16(mnem);
			uint8_t imm = FETCH_BYTE();
			MNEM("or %s, %02X", rm, imm);
		}
	}
	else {
		const char* rm = modrm_get_mnem8(mnem);
		uint8_t imm = FETCH_BYTE();
		MNEM("or %s, %02X", rm, imm);
	}
}
static void or_rm_reg(I8086_MNEM* mnem) {
	/* or r/m, reg (08/0A/09/0B) b000010DW */
	mnem->modrm.byte = FETCH_BYTE();
	if (W) {
		const char* rm = modrm_get_mnem16(mnem);
		const char* reg = GET_REG16(mnem->modrm.reg);
		get_direction(mnem, &reg, &rm);
		MNEM("or %s, %s", rm, reg);
	}
	else {
		const char* rm = modrm_get_mnem8(mnem);
		const char* reg = GET_REG8(mnem->modrm.reg);
		get_direction(mnem, &reg, &rm);
		MNEM("or %s, %s", rm, reg);
	}
}
static void or_accum_imm(I8086_MNEM* mnem) {
	/* or AL/AX, imm (0C/0D) b0000110W */
	if (W) {
		uint16_t imm = FETCH_WORD();
		MNEM("or ax, %04X", imm);
	}
	else {
		uint8_t imm = FETCH_BYTE();
		MNEM("or al, %02X", imm);
	}
}

static void adc_rm_imm(I8086_MNEM* mnem) {
	/* adc r/m, imm (80/81/82/83, R/M reg = b010) b100000SW */
	if (W) {
		if (SW == 0b01) {
			/* reg16, disp16 */
			const char* rm = modrm_get_mnem16(mnem);
			uint16_t imm = FETCH_WORD();
			MNEM("adc %s, %04X", rm, imm);
		}
		else {
			/* reg16, disp8 */
			const char* rm = modrm_get_mnem16(mnem);
			uint8_t imm = FETCH_BYTE();
			MNEM("adc %s, %02X", rm, imm);
		}
	}
	else {
		const char* rm = modrm_get_mnem8(mnem);
		uint8_t imm = FETCH_BYTE();
		MNEM("adc %s, %02X", rm, imm);
	}
}
static void adc_rm_reg(I8086_MNEM* mnem) {
	/* adc r/m, reg (10/12/11/13) b000100DW */
	mnem->modrm.byte = FETCH_BYTE();
	if (W) {
		const char* rm = modrm_get_mnem16(mnem);
		const char* reg = GET_REG16(mnem->modrm.reg);
		get_direction(mnem, &reg, &rm);
		MNEM("adc %s, %s", rm, reg);
	}
	else {
		const char* rm = modrm_get_mnem8(mnem);
		const char* reg = GET_REG8(mnem->modrm.reg);
		get_direction(mnem, &reg, &rm);
		MNEM("adc %s, %s", rm, reg);
	}
}
static void adc_accum_imm(I8086_MNEM* mnem) {
	/* adc AL/AX, imm (14/15) b0001010W */
	if (W) {
		uint16_t imm = FETCH_WORD();
		MNEM("adc ax, %04X", imm);
	}
	else {
		uint8_t imm = FETCH_BYTE();
		MNEM("adc al, %02X", imm);
	}
}

static void sbb_rm_imm(I8086_MNEM* mnem) {
	/* sbb r/m, imm (80/81/82/83, R/M reg = b011)  b100000SW */
	if (W) {
		if (SW == 0b01) {
			/* reg16, disp16 */
			const char* rm = modrm_get_mnem16(mnem);
			uint16_t imm = FETCH_WORD();
			MNEM("sbb %s, %04X", rm, imm);
		}
		else {
			/* reg16, disp8 */
			const char* rm = modrm_get_mnem16(mnem);
			uint8_t imm = FETCH_BYTE();
			MNEM("sbb %s, %02X", rm, imm);
		}
	}
	else {
		const char* rm = modrm_get_mnem8(mnem);
		uint8_t imm = FETCH_BYTE();
		MNEM("sbb %s, %02X", rm, imm);
	}
}
static void sbb_rm_reg(I8086_MNEM* mnem) {
	/* sbb r/m, reg (18/1A/19/1B) b000110DW */
	mnem->modrm.byte = FETCH_BYTE();
	if (W) {
		const char* rm = modrm_get_mnem16(mnem);
		const char* reg = GET_REG16(mnem->modrm.reg);
		get_direction(mnem, &reg, &rm);
		MNEM("sbb %s, %s", rm, reg);
	}
	else {
		const char* rm = modrm_get_mnem8(mnem);
		const char* reg = GET_REG8(mnem->modrm.reg);
		get_direction(mnem, &reg, &rm);
		MNEM("sbb %s, %s", rm, reg);
	}
}
static void sbb_accum_imm(I8086_MNEM* mnem) {
	/* sbb AL/AX, imm (1C/1D) b0001110W */
	if (W) {
		uint16_t imm = FETCH_WORD();
		MNEM("sbb ax, %04X", imm);
	}
	else {
		uint8_t imm = FETCH_BYTE();
		MNEM("sbb al, %02X", imm);
	}
}

static void and_rm_imm(I8086_MNEM* mnem) {
	/* and r/m, imm (80/81/82/83, R/M reg = b100) b100000SW */
	if (W) {
		if (SW == 0b01) {
			/* reg16, disp16 */
			const char* rm = modrm_get_mnem16(mnem);
			uint16_t imm = FETCH_WORD();
			MNEM("and %s, %04X", rm, imm);
		}
		else {
			/* reg16, disp8 */
			const char* rm = modrm_get_mnem16(mnem);
			uint8_t imm = FETCH_BYTE();
			MNEM("and %s, %02X", rm, imm);
		}
	}
	else {
		const char* rm = modrm_get_mnem8(mnem);
		uint8_t imm = FETCH_BYTE();
		MNEM("and %s, %02X", rm, imm);
	}
}
static void and_rm_reg(I8086_MNEM* mnem) {
	/* and r/m, reg (20/22/21/23) b001000DW */
	mnem->modrm.byte = FETCH_BYTE();
	if (W) {
		const char* rm = modrm_get_mnem16(mnem);
		const char* reg = GET_REG16(mnem->modrm.reg);
		get_direction(mnem, &reg, &rm);
		MNEM("and %s, %s", rm, reg);
	}
	else {
		const char* rm = modrm_get_mnem8(mnem);
		const char* reg = GET_REG8(mnem->modrm.reg);
		get_direction(mnem, &reg, &rm);
		MNEM("and %s, %s", rm, reg);
	}
}
static void and_accum_imm(I8086_MNEM* mnem) {
	/* and AL/AX, imm (24/25) b0010010W */
	if (W) {
		uint16_t imm = FETCH_WORD();
		MNEM("and ax, %04X", imm);
	}
	else {
		uint8_t imm = FETCH_BYTE();
		MNEM("and al, %02X", imm);
	}
}

static void sub_rm_imm(I8086_MNEM* mnem) {
	/* sub r/m, imm (80/81, R/M reg = b101) b100000SW */
	if (W) {
		if (SW == 0b01) {
			/* reg16, disp16 */
			const char* rm = modrm_get_mnem16(mnem);
			uint16_t imm = FETCH_WORD();
			MNEM("sub %s, %04X", rm, imm);
		}
		else {
			/* reg16, disp8 */
			const char* rm = modrm_get_mnem16(mnem);
			uint8_t imm = FETCH_BYTE();
			MNEM("sub %s, %02X", rm, imm);
		}
	}
	else {
		const char* rm = modrm_get_mnem8(mnem);
		uint8_t imm = FETCH_BYTE();
		MNEM("sub %s, %02X", rm, imm);
	}
}
static void sub_rm_reg(I8086_MNEM* mnem) {
	/* sub r/m, reg (28/2A/29/2B) b001010DW */
	mnem->modrm.byte = FETCH_BYTE();
	if (W) {
		const char* rm = modrm_get_mnem16(mnem);
		const char* reg = GET_REG16(mnem->modrm.reg);
		get_direction(mnem, &reg, &rm);
		MNEM("sub %s, %s", rm, reg);
	}
	else {
		const char* rm = modrm_get_mnem8(mnem);
		const char* reg = GET_REG8(mnem->modrm.reg);
		get_direction(mnem, &reg, &rm);
		MNEM("sub %s, %s", rm, reg);
	}
}
static void sub_accum_imm(I8086_MNEM* mnem) {
	/* sub AL/AX, imm (2C/2D) b0010110W */
	if (W) {
		uint16_t imm = FETCH_WORD();
		MNEM("sub ax, %04X", imm);
	}
	else {
		uint8_t imm = FETCH_BYTE();
		MNEM("sub al, %02X", imm);
	}
}

static void xor_rm_imm(I8086_MNEM* mnem) {
	/* xor r/m, imm (80/81/82/83, R/M reg = b110) b100000SW */
	if (W) {
		if (SW == 0b01) {
			/* reg16, disp16 */
			const char* rm = modrm_get_mnem16(mnem);
			uint16_t imm = FETCH_WORD();
			MNEM("xor %s, %04X", rm, imm);
		}
		else {
			/* reg16, disp8 */
			const char* rm = modrm_get_mnem16(mnem);
			uint8_t imm = FETCH_BYTE();
			MNEM("xor %s, %02X", rm, imm);
		}
	}
	else {
		const char* rm = modrm_get_mnem8(mnem);
		uint8_t imm = FETCH_BYTE();
		MNEM("xor %s, %02X", rm, imm);
	}
}
static void xor_rm_reg(I8086_MNEM* mnem) {
	/* xor r/m, reg (30/32/31/33) b001100DW */
	mnem->modrm.byte = FETCH_BYTE();
	if (W) {
		const char* rm = modrm_get_mnem16(mnem);
		const char* reg = GET_REG16(mnem->modrm.reg);
		get_direction(mnem, &reg, &rm);
		MNEM("xor %s, %s", rm, reg);
	}
	else {
		const char* rm = modrm_get_mnem8(mnem);
		const char* reg = GET_REG8(mnem->modrm.reg);
		get_direction(mnem, &reg, &rm);
		MNEM("xor %s, %s", rm, reg);
	}
}
static void xor_accum_imm(I8086_MNEM* mnem) {
	/* xor AL/AX, imm (34/35) b0011010W */
	if (W) {
		uint16_t imm = FETCH_WORD();
		MNEM("xor ax, %04X", imm);
	}
	else {
		uint8_t imm = FETCH_BYTE();
		MNEM("xor al, %02X", imm);
	}
}

static void cmp_rm_imm(I8086_MNEM* mnem) {
	/* cmp r/m, imm (80/81/82/83, R/M reg = b111)  b100000SW */
	if (W) {
		if (SW == 0b01) {
			/* reg16, disp16 */
			const char* rm = modrm_get_mnem16(mnem);
			uint16_t imm = FETCH_WORD();
			MNEM("cmp %s, %04X", rm, imm);
		}
		else {
			/* reg16, disp8 */
			const char* rm = modrm_get_mnem16(mnem);
			uint8_t imm = FETCH_BYTE();
			MNEM("cmp %s, %02X", rm, imm);
		}
	}
	else {
		const char* rm = modrm_get_mnem8(mnem);
		uint8_t imm = FETCH_BYTE();
		MNEM("cmp %s, %02X", rm, imm);
	}
}
static void cmp_rm_reg(I8086_MNEM* mnem) {
	/* cmp r/m, reg (38/39/3A/3B) b001110DW */
	mnem->modrm.byte = FETCH_BYTE();
	if (W) {
		const char* rm = modrm_get_mnem16(mnem);
		const char* reg = GET_REG16(mnem->modrm.reg);
		get_direction(mnem, &reg, &rm);
		MNEM("cmp %s, %s", rm, reg);
	}
	else {
		const char* rm = modrm_get_mnem8(mnem);
		const char* reg = GET_REG8(mnem->modrm.reg);
		get_direction(mnem, &reg, &rm);
		MNEM("cmp %s, %s", rm, reg);
	}
}
static void cmp_accum_imm(I8086_MNEM* mnem) {
	/* cmp AL/AX, imm (3C/3D) b0011110W */
	if (W) {
		uint16_t imm = FETCH_WORD();
		MNEM("cmp ax, %04X", imm);
	}
	else {
		uint8_t imm = FETCH_BYTE();
		MNEM("cmp al, %02X", imm);
	}
}

static void test_rm_imm(I8086_MNEM* mnem) {
	/* test r/m, imm (F6/F7, R/M reg = b000) b1111011W */
	if (W) {
		/* reg16, disp16 */
		const char* rm = modrm_get_mnem16(mnem);
		uint16_t imm = FETCH_WORD();
		MNEM("test %s, %04X", rm, imm);		
	}
	else {
		const char* rm = modrm_get_mnem8(mnem);
		uint8_t imm = FETCH_BYTE();
		MNEM("test %s, %02X", rm, imm);
	}
}
static void test_rm_reg(I8086_MNEM* mnem) {
	/* test r/m, reg (84/85) b1000010W */
	mnem->modrm.byte = FETCH_BYTE();
	if (W) {
		const char* rm = modrm_get_mnem16(mnem);
		const char* reg = GET_REG16(mnem->modrm.reg);
		get_direction(mnem, &reg, &rm);
		MNEM("test %s, %s", rm, reg);
	}
	else {
		const char* rm = modrm_get_mnem8(mnem);
		const char* reg = GET_REG8(mnem->modrm.reg);
		get_direction(mnem, &reg, &rm);
		MNEM("test %s, %s", rm, reg);
	}
}
static void test_accum_imm(I8086_MNEM* mnem) {
	/* test AL/AX, imm (A8/A9) b1010100W */
	if (W) {
		uint16_t imm = FETCH_WORD();
		MNEM("test ax, %04X", imm);
	}
	else {
		uint8_t imm = FETCH_BYTE();
		MNEM("test al, %02X", imm);
	}
}

static void daa(I8086_MNEM* mnem) {
	/* Decimal Adjust for Addition (27) b00100111 */
	MNEM0("daa");
}
static void das(I8086_MNEM* mnem) {
	/* Decimal Adjust for Subtraction (2F) b00101111 */
	MNEM0("das");
}
static void aaa(I8086_MNEM* mnem) {
	/* ASCII Adjust for Addition (37) b00110111 */
	MNEM0("aaa");
}
static void aas(I8086_MNEM* mnem) {
	/* ASCII Adjust for Subtraction (3F) b00111111 */
	MNEM0("aas");
}
static void aam(I8086_MNEM* mnem) {
	/* ASCII Adjust for Multiply (D4 0A) b11010100 00001010 */
	uint8_t divisor = FETCH_BYTE(); // undocumented operand; normally 0x0A
	MNEM("aam %02X", divisor);
}
static void aad(I8086_MNEM* mnem) {
	/* ASCII Adjust for Division (D5 0A) b11010101 00001010 */
	uint8_t divisor = FETCH_BYTE(); // undocumented operand; normally 0x0A
	MNEM("aad %02X", divisor);
}
static void salc(I8086_MNEM* mnem) {
	/* set carry in AL (D6) b11010110 undocumented opcode */
	MNEM0("salc");
}

static void inc_reg(I8086_MNEM* mnem) {
	/* Inc reg16 (40-47) b01000REG */
	MNEM("inc %s", GET_REG16(mnem->opcode));
}
static void dec_reg(I8086_MNEM* mnem) {
	/* Dec reg16 (48-4F) b01001REG */
	MNEM("dec %s", GET_REG16(mnem->opcode));
}

static void push_seg(I8086_MNEM* mnem) {
	/* Push seg16 (06/0E/16/1E) b000SR110 */
	MNEM("push %s", seg_mnem[SR]);
}
static void pop_seg(I8086_MNEM* mnem) {
	/* Pop seg16 (07/0F/17/1F) b000SR111 */
	MNEM("pop %s", seg_mnem[SR]);
}
static void push_reg(I8086_MNEM* mnem) {
	/* Push reg16 (50-57) b01010REG */
	MNEM("push %s", GET_REG16(mnem->opcode));
}
static void pop_reg(I8086_MNEM* mnem) {
	/* Pop reg16 (58-5F) b01011REG */
	MNEM("pop %s", GET_REG16(mnem->opcode));
}
static void push_rm(I8086_MNEM* mnem) {
	/* Push R/M (FF, R/M reg = 110) b11111111 */
	const char* rm = modrm_get_mnem16(mnem);
	MNEM("push %s", rm);
}
static void pop_rm(I8086_MNEM* mnem) {
	/* Pop R/M (8F) b10001111 */
	const char* rm = modrm_get_mnem16(mnem);
	MNEM("pop %s", rm);
}
static void pushf(I8086_MNEM* mnem) {
	/* push psw (9C) b10011100 */
	MNEM0("pushf");
}
static void popf(I8086_MNEM* mnem) {
	/* pop psw (9D) b10011101 */
	MNEM0("popf");
}

static void nop(I8086_MNEM* mnem) {
	/* nop (90) b10010000 */
	MNEM0("nop");
}
static void xchg_accum_reg(I8086_MNEM* mnem) {
	/* xchg AX, reg16 (91 - 97) b10010REG */	
	const char* reg = GET_REG16(mnem->opcode);
	MNEM("xchg AX, %s", reg);
}
static void xchg_rm_reg(I8086_MNEM* mnem) {
	/* xchg R/M, reg16 (86/87) b1000011W */
	mnem->modrm.byte = FETCH_BYTE();
	if (W) {
		const char* rm = modrm_get_mnem16(mnem);
		const char* reg = GET_REG16(mnem->modrm.reg);
		MNEM("xchg %s, %s", rm, reg);
	}
	else {
		const char* rm = modrm_get_mnem8(mnem);
		const char* reg = GET_REG8(mnem->modrm.reg);
		MNEM("xchg %s, %s", rm, reg);
		
	}
}

static void cbw(I8086_MNEM* mnem) {
	/* Convert byte to word (98) b10011000 */
	MNEM0("cbw");
}
static void cwd(I8086_MNEM* mnem) {
	/* Convert word to dword (99) b10011001 */
	MNEM0("cwd");
}

static void wait(I8086_MNEM* mnem) {
	/* wait (9B) b10011011 */
	MNEM0("wait");
}

static void sahf(I8086_MNEM* mnem) {
	/* Store AH into flags (9E) b10011110 */
	MNEM0("sahf");
}
static void lahf(I8086_MNEM* mnem) {
	/* Load flags into AH (9F) b10011111 */
	MNEM0("lahf");
}

static void hlt(I8086_MNEM* mnem) {
	/* Halt mnem (F4) b11110100 */
	MNEM0("hlt");
}
static void cmc(I8086_MNEM* mnem) {
	// Complement carry flag (F5) b11110101
	MNEM0("cmc");
}
static void clc(I8086_MNEM* mnem) {
	// clear carry flag (F8) b11111000
	MNEM0("clc");
}
static void stc(I8086_MNEM* mnem) {
	// set carry flag (F9) b11111001
	MNEM0("stc");
}
static void cli(I8086_MNEM* mnem) {
	// clear interrupt flag (FA) b11111010
	MNEM0("cli");
}
static void sti(I8086_MNEM* mnem) {
	// set interrupt flag (FB) b1111011
	MNEM0("sti");
}
static void cld(I8086_MNEM* mnem) {
	// clear direction flag (FC) b11111100
	MNEM0("cld");
}
static void std(I8086_MNEM* mnem) {
	// set direction flag (FD) b11111101
	MNEM0("std");
}

static void inc_rm(I8086_MNEM* mnem) {
	/* Inc R/M (FE/FF, R/M reg = 000) b1111111W */
	if (W) {
		const char* rm = modrm_get_mnem16(mnem);
		MNEM("inc %s", rm);
	}
	else {
		const char* rm = modrm_get_mnem8(mnem);
		MNEM("inc %s", rm);
	}
}
static void dec_rm(I8086_MNEM* mnem) {
	/* Dec R/M (FE/FF, R/M reg = 001) b1111111W */
	if (W) {
		const char* rm = modrm_get_mnem16(mnem);
		MNEM("dec %s", rm);
	}
	else {
		const char* rm = modrm_get_mnem8(mnem);
		MNEM("dec %s", rm);
	}
}

static void rol(I8086_MNEM* mnem) {
	/* Rotate left (D0/D1/D2/D3, R/M reg = 000) b110100VW */
	char count[3] = { 0 };
	if (VW) {
		count[0] = 'c';
		count[1] = 'l';
	}
	else {
		count[0] = '1';
	}

	if (W) {
		const char* rm = modrm_get_mnem16(mnem);
		MNEM("rol %s, %s", rm, count);
	}
	else {
		const char* rm = modrm_get_mnem8(mnem);
		MNEM("rol %s, %s", rm, count);
	}
}
static void ror(I8086_MNEM* mnem) {
	/* Rotate left (D0/D1/D2/D3, R/M reg = 001) b110100VW */
	char count[3] = { 0 };
	if (VW) {
		count[0] = 'c';
		count[1] = 'l';
	}
	else {
		count[0] = '1';
	}

	if (W) {
		const char* rm = modrm_get_mnem16(mnem);
		MNEM("ror %s, %s", rm, count);
	}
	else {
		const char* rm = modrm_get_mnem8(mnem);
		MNEM("ror %s, %s", rm, count);
	}
}
static void rcl(I8086_MNEM* mnem) {
	/* Rotate through carry left (D0/D1/D2/D3, R/M reg = 010) b110100VW */
	char count[3] = { 0 };
	if (VW) {
		count[0] = 'c';
		count[1] = 'l';
	}
	else {
		count[0] = '1';
	}

	if (W) {
		const char* rm = modrm_get_mnem16(mnem);
		MNEM("rcl %s, %s", rm, count);
	}
	else {
		const char* rm = modrm_get_mnem8(mnem);
		MNEM("rcl %s, %s", rm, count);
	}
}
static void rcr(I8086_MNEM* mnem) {
	/* Rotate through carry right (D0/D1/D2/D3, R/M reg = 011) b110100VW */
	char count[3] = { 0 };
	if (VW) {
		count[0] = 'c';
		count[1] = 'l';
	}
	else {
		count[0] = '1';
	}

	if (W) {
		const char* rm = modrm_get_mnem16(mnem);
		MNEM("rcr %s, %s", rm, count);
	}
	else {
		const char* rm = modrm_get_mnem8(mnem);
		MNEM("rcr %s, %s", rm, count);
	}
}
static void shl(I8086_MNEM* mnem) {
	/* Shift left (D0/D1/D2/D3, R/M reg = 100) b110100VW */
	char count[3] = { 0 };
	if (VW) {
		count[0] = 'c';
		count[1] = 'l';
	}
	else {
		count[0] = '1';
	}

	if (W) {
		const char* rm = modrm_get_mnem16(mnem);
		MNEM("shl %s, %s", rm, count);
	}
	else {
		const char* rm = modrm_get_mnem8(mnem);
		MNEM("shl %s, %s", rm, count);
	}
}
static void shr(I8086_MNEM* mnem) {
	/* Shift Logical right (D0/D1/D2/D3, R/M reg = 101) b110100VW */
	char count[3] = { 0 };
	if (VW) {
		count[0] = 'c';
		count[1] = 'l';
	}
	else {
		count[0] = '1';
	}

	if (W) {
		const char* rm = modrm_get_mnem16(mnem);
		MNEM("shr %s, %s", rm, count);
	}
	else {
		const char* rm = modrm_get_mnem8(mnem);
		MNEM("shr %s, %s", rm, count);
	}
}
static void sar(I8086_MNEM* mnem) {
	/* Shift Arithmetic right (D0/D1/D2/D3, R/M reg = 111) b110100VW */
	char count[3] = { 0 };
	if (VW) {
		count[0] = 'c';
		count[1] = 'l';
	}
	else {
		count[0] = '1';
	}

	if (W) {
		const char* rm = modrm_get_mnem16(mnem);
		MNEM("sar %s, %s", rm, count);
	}
	else {
		const char* rm = modrm_get_mnem8(mnem);
		MNEM("sar %s, %s", rm, count);
	}
}

static void jcc(I8086_MNEM* mnem) {
	/* conditional jump(70-7F) b011XCCCC
	   8086 cpu decode 60-6F the same as 70-7F */
	uint8_t imm = FETCH_BYTE();

#ifdef CONVERT_REL_TO_ABS
	uint16_t se = sign_extend8_16(imm);
	uint16_t offset = TO_LABEL_OFFSET(se);
	switch (CCCC) {
		case I8086_JCC_JO:
			MNEM("jo %04X", offset);
			break;
		case I8086_JCC_JNO:
			MNEM("jno %04X", offset);
			break;
		case I8086_JCC_JC:
			MNEM("jc %04X", offset);
			break;
		case I8086_JCC_JNC:
			MNEM("jnc %04X", offset);
			break;
		case I8086_JCC_JZ:
			MNEM("jz %04X", offset);
			break;
		case I8086_JCC_JNZ:
			MNEM("jnz %04X", offset);
			break;
		case I8086_JCC_JBE:
			MNEM("jbe %04X", offset);
			break;
		case I8086_JCC_JA:
			MNEM("ja %04X", offset);
			break;
		case I8086_JCC_JS:
			MNEM("js %04X", offset);
			break;
		case I8086_JCC_JNS:
			MNEM("jns %04X", offset);
			break;
		case I8086_JCC_JPE:
			MNEM("jpe %04X", offset);
			break;
		case I8086_JCC_JPO:
			MNEM("jpo %04X", offset);
			break;
		case I8086_JCC_JL:
			MNEM("jl %04X", offset);
			break;
		case I8086_JCC_JGE:
			MNEM("jge %04X", offset);
			break;
		case I8086_JCC_JLE:
			MNEM("jle %04X", offset);
			break;
		case I8086_JCC_JG:
			MNEM("jg %04X", offset);
			break;
	}
#else
	switch (CCCC) {
		case I8086_JCC_JO:
			MNEM("jo %02X", imm);
			break;
		case I8086_JCC_JNO:
			MNEM("jno %02X", imm);
			break;
		case I8086_JCC_JC:
			MNEM("jc %02X", imm);
			break;
		case I8086_JCC_JNC:
			MNEM("jnc %02X", imm);
			break;
		case I8086_JCC_JZ:
			MNEM("jz %02X", imm);
			break;
		case I8086_JCC_JNZ:
			MNEM("jnz %02X", imm);
			break;
		case I8086_JCC_JBE:
			MNEM("jbe %02X", imm);
			break;
		case I8086_JCC_JA:
			MNEM("ja %04X", imm);
			break;
		case I8086_JCC_JS:
			MNEM("js %02X", imm);
			break;
		case I8086_JCC_JNS:
			MNEM("jns %02X", imm);
			break;
		case I8086_JCC_JPE:
			MNEM("jpe %02X", imm);
			break;
		case I8086_JCC_JPO:
			MNEM("jpo %02X", imm);
			break;
		case I8086_JCC_JL:
			MNEM("jl %02X", imm);
			break;
		case I8086_JCC_JGE:
			MNEM("jge %02X", imm);
			break;
		case I8086_JCC_JLE:
			MNEM("jle %02X", imm);
			break;
		case I8086_JCC_JG:
			MNEM("jg %02X", imm);
			break;
	}
#endif
}
static void jmp_intra_direct(I8086_MNEM* mnem) {
	/* Jump near (E9) b11101001 */
	uint16_t imm = FETCH_WORD();
#ifdef CONVERT_REL_TO_ABS
	MNEM("jmp near %04X", TO_LABEL_OFFSET(imm));
#else
	MNEM("jmp near %02X", imm);
#endif
}
static void jmp_inter_direct(I8086_MNEM* mnem) {
	/* Jump addr:seg (EA) b11101010 */
	uint16_t imm = FETCH_WORD();
	uint16_t imm2 = FETCH_WORD();
	MNEM("jmp far %04X:%04X", imm2, imm);
}
static void jmp_intra_direct_short(I8086_MNEM* mnem) {
	/* Jump near short (EB) b11101011 */
	uint8_t imm = FETCH_BYTE();
#ifdef CONVERT_REL_TO_ABS
	uint16_t se = sign_extend8_16(imm);
	MNEM("jmp short %04X", TO_LABEL_OFFSET(se)); 
#else
	MNEM("jmp short %02X", imm);
#endif
}
static void jmp_intra_indirect(I8086_MNEM* mnem) {
	/* Jump intra indirect (FF, R/M reg = 100) b11111111 */	
	const char* rm = modrm_get_mnem16(mnem);
	MNEM("jmp near %s", rm);
}
static void jmp_inter_indirect(I8086_MNEM* mnem) {
	/* Jump inter indirect (FF, R/M reg = 101) b11111111 */
	const char* rm = modrm_get_mnem16(mnem);
	MNEM("jmp far %s", rm);
}

static void call_intra_direct(I8086_MNEM* mnem) {
	/* Call disp (E8) b11101000 */
	uint16_t imm = FETCH_WORD();
#ifdef CONVERT_REL_TO_ABS
	MNEM("call near %04X", TO_LABEL_OFFSET(imm));
#else
	MNEM("call near %02X", imm);
#endif
}
static void call_inter_direct(I8086_MNEM* mnem) {
	/* Call addr:seg (9A) b10011010 */
	uint16_t imm = FETCH_WORD();
	uint16_t imm2 = FETCH_WORD();
	MNEM("call far %04X:%04X", imm2, imm);
}
static void call_intra_indirect(I8086_MNEM* mnem) {
	/* Call mem/reg (FF, R/M reg = 010) b11111111 */
	const char* rm = modrm_get_mnem16(mnem);
	MNEM("call near %s", rm);
}
static void call_inter_indirect(I8086_MNEM* mnem) {
	/* Call mem (FF, R/M reg = 011) b11111111 */
	const char* rm = modrm_get_mnem16(mnem);
	MNEM("call far %s", rm);
}

static void ret_intra_add_imm(I8086_MNEM* mnem) {
	/* Ret imm16 (C2) b11000010 
	   undocumented* C0 decodes identically to C2 */
	uint16_t imm = FETCH_WORD();
	MNEM("ret %04X", imm);
}
static void ret_intra(I8086_MNEM* mnem) {
	/* Ret (C3) b11000011 
	undocumented* C1 decodes identically to C3 */
	MNEM0("ret");
}
static void ret_inter_add_imm(I8086_MNEM* mnem) {
	/* Ret imm16 (CA) b11001010 
	undocumented* C8 decodes identically to CA */
	uint16_t imm = FETCH_WORD();
	MNEM("ret %04X", imm);
}
static void ret_inter(I8086_MNEM* mnem) {
	/* Ret (CB) b11001011 
	undocumented* C9 decodes identically to CB */
	MNEM0("ret");
}

static void mov_rm_imm(I8086_MNEM* mnem) {
	/* mov r/m, imm (C6/C7) b1100011W */
	mnem->modrm.byte = FETCH_BYTE();
	if (W) {
		const char* rm = modrm_get_mnem16(mnem);
		uint16_t imm = FETCH_WORD();
		MNEM("mov %s, %04X", rm, imm);
	}
	else {
		const char* rm = modrm_get_mnem8(mnem);
		uint8_t imm = FETCH_BYTE();
		MNEM("mov %s, %02X", rm, imm);
	}
}
static void mov_reg_imm(I8086_MNEM* mnem) {
	/* mov r/m, reg (B0-BF) b1011WREG */
	if (WREG) {
		uint16_t imm = FETCH_WORD();
		const char* reg = GET_REG16(mnem->opcode);
		MNEM("mov %s, %02X", reg, imm);
	}
	else {
		uint8_t imm = FETCH_BYTE();
		const char* reg = GET_REG8(mnem->opcode);
		MNEM("mov %s, %02X", reg, imm);
	}
}
static void mov_rm_reg(I8086_MNEM* mnem) {
	/* mov r/m, reg (88/89/8A/8B) b100010DW */
	mnem->modrm.byte = FETCH_BYTE();
	if (W) {
		const char* rm = modrm_get_mnem16(mnem);
		const char* reg = GET_REG16(mnem->modrm.reg);
		get_direction(mnem, &reg, &rm);
		MNEM("mov %s, %s", rm, reg);
	}
	else {
		const char* rm = modrm_get_mnem8(mnem);
		const char* reg = GET_REG8(mnem->modrm.reg);
		get_direction(mnem, &reg, &rm);
		MNEM("mov %s, %s", rm, reg);
	}
}
static void mov_accum_mem(I8086_MNEM* mnem) {
	/* mov AL/AX, [addr] (A0/A1/A2/A3) b101000DW */
	uint16_t addr = FETCH_WORD();
	MNEM_F(mnem->addressing_str, "[%04X]", addr);
	const char* mem = mnem->addressing_str;
	if (W) {
		const char* reg = GET_REG16(REG_AX);
		get_direction(mnem, &reg, &mem);
		MNEM("mov %s, %s", reg, mem);
	}
	else {
		const char* reg = GET_REG8(REG_AL);
		get_direction(mnem, &reg, &mem);
		MNEM("mov %s, %s", reg, mem);
	}
}
static void mov_seg(I8086_MNEM* mnem) {
	/* mov r/m, seg (8C/8E) b100011D0 */
	mnem->modrm.byte = FETCH_BYTE();
	const char* rm = modrm_get_mnem16(mnem);
	const char* reg = seg_mnem[mnem->modrm.reg & 3];
	get_direction(mnem, &reg, &rm);
	MNEM("mov %s, %s", rm, reg);
}

static void lea(I8086_MNEM* mnem) {
	/* lea reg16, mem (8D) b10001101 */
	mnem->modrm.byte = FETCH_BYTE();
	const char* reg = GET_REG16(mnem->modrm.reg);
	const char* mem = modrm_get_mnem16(mnem);
	MNEM("lea %s, %s", reg, mem);
}

static void not(I8086_MNEM* mnem) {
	/* not reg (F6/F7, R/M reg = b010) b1111011W */
	if (W) {
		const char* reg = GET_REG16(mnem->modrm.rm);
		MNEM("not %s", reg);
	}
	else {
		const char* reg = GET_REG8(mnem->modrm.rm);
		MNEM("not %s", reg);
	}
}
static void neg(I8086_MNEM* mnem) {
	/* neg reg (F6/F7, R/M reg = b011) b1111011W */
	if (W) {
		const char* reg = GET_REG16(mnem->modrm.rm);
		MNEM("neg %s", reg);
	}
	else {
		const char* reg = GET_REG8(mnem->modrm.rm);
		MNEM("neg %s", reg);
	}
}
static void mul_rm(I8086_MNEM* mnem) {
	/* mul r/m (F6/F7, R/M reg = b100) b1111011W */
	if (W) {
		const char* mul = modrm_get_mnem16(mnem);
		MNEM("mul %s", mul);
	}
	else {
		const char* mul = modrm_get_mnem8(mnem);
		MNEM("mul %s", mul);
	}
}
static void imul_rm(I8086_MNEM* mnem) {
	/* imul r/m (F6/F7, R/M reg = b101) b1111011W */
	if (W) {
		const char* mul = modrm_get_mnem16(mnem);
		MNEM("imul %s", mul);
	}
	else {
		const char* mul = modrm_get_mnem8(mnem);
		MNEM("imul %s", mul);
	}
}
static void div_rm(I8086_MNEM* mnem) {
	/* div r/m (F6/F7, R/M reg = b110) b1111011W */
	if (W) {
		const char* div = modrm_get_mnem16(mnem);
		MNEM("div %s", div);
	}
	else {
		const char* div = modrm_get_mnem8(mnem);
		MNEM("div %s", div);
	}
}
static void idiv_rm(I8086_MNEM* mnem) {
	/* idiv r/m (F6/F7, R/M reg = b111) b1111011W */
	if (W) {
		const char* div = modrm_get_mnem16(mnem);
		MNEM("idiv %s", div);
	}
	else {
		const char* div = modrm_get_mnem8(mnem);
		MNEM("idiv %s", div);
	}
}

#define REP_NONE     0
#define REP_NOT_ZERO 1
#define REP_ZERO     2

static void movs(I8086_MNEM* mnem) {
	/* movs (A4/A5) b1010010W */
	if (W) {
		MNEM0("movsw");
	}
	else {
		MNEM0("movsb");
	}
}
static void stos(I8086_MNEM* mnem) {
	/* stos (AA/AB) b1010101W */
	if (W) {
		MNEM0("stosw");
	}
	else {
		MNEM0("stosb");
	}
}
static void lods(I8086_MNEM* mnem) {
	/* lods (AC/AD) b1010110W */
	if (W) {
		MNEM0("lodsw");
	}
	else {
		MNEM0("lodsb");
	}
}
static void cmps(I8086_MNEM* mnem) {
	/* cmps (A6/A7) b1010011W */
	if (W) {
		MNEM0("cmpsw");
	}
	else {
		MNEM0("cmpsb");
	}
}
static void scas(I8086_MNEM* mnem) {
	/* scas (AE/AF) b1010111W */
	if (W) {
		MNEM0("scasw");
	}
	else {
		MNEM0("scasb");
	}
}

static void les(I8086_MNEM* mnem) {
	/* les (C4) b11000100 */
	mnem->modrm.byte = FETCH_BYTE();
	const char* rm = modrm_get_mnem16(mnem);
	MNEM("les %s", rm);
}
static void lds(I8086_MNEM* mnem) {
	/* lds (C5) b11000101 */
	mnem->modrm.byte = FETCH_BYTE();
	const char* rm = modrm_get_mnem16(mnem);
	MNEM("lds %s", rm);
}

static void xlat(I8086_MNEM* mnem) {
	/* Get data pointed by BX + AL (D7) b11010111 */
	MNEM0("xlat");
}

static void esc(I8086_MNEM* mnem) {
	/* esc (D8-DF R/M reg = XXX) b11010REG */
	mnem->modrm.byte = FETCH_BYTE();
	uint8_t esc_opcode = ((mnem->opcode & 7) << 3) | mnem->modrm.reg;
	const char* rm = modrm_get_mnem16(mnem);
	printf("esc %02X, %s", esc_opcode, rm);
}

static void loopnz(I8086_MNEM* mnem) {
	/* loop while not zero (E0) b1110000Z */
	uint8_t disp = FETCH_BYTE();
#ifdef CONVERT_REL_TO_ABS
	uint16_t se = sign_extend8_16(disp);
	MNEM("loopnz %04X", TO_LABEL_OFFSET(se));
#else
	MNEM("loopnz %02X", disp);
#endif
}
static void loopz(I8086_MNEM* mnem) {
	/* loop while zero (E1) b1110000Z */
	uint8_t disp = FETCH_BYTE();
#ifdef CONVERT_REL_TO_ABS
	uint16_t se = sign_extend8_16(disp);
	MNEM("loopz %04X", TO_LABEL_OFFSET(se));
#else
	MNEM("loopz %02X", disp);
#endif
}
static void loop(I8086_MNEM* mnem) {
	/* loop if CX not zero (E2) b11100010 */
	uint8_t disp = FETCH_BYTE();
#ifdef CONVERT_REL_TO_ABS
	uint16_t se = sign_extend8_16(disp);
	MNEM("loop %04X", TO_LABEL_OFFSET(se));
#else
	MNEM("loop %02X", disp);
#endif
}
static void jcxz(I8086_MNEM* mnem) {
	/* jump if CX zero (E3) b11100011 */
	uint8_t disp = FETCH_BYTE();
#ifdef CONVERT_REL_TO_ABS
	uint16_t se = sign_extend8_16(disp);
	MNEM("jcxz %04X", TO_LABEL_OFFSET(se));
#else
	MNEM("jcxz %02X", disp);
#endif
}

static void in_accum_imm(I8086_MNEM* mnem) {
	/* in AL/AX, imm */
	uint8_t imm = FETCH_BYTE();
	if (W) {
		MNEM("in ax, %02X", imm);
	}
	else {
		MNEM("in al, %02X", imm);
	}
}
static void out_accum_imm(I8086_MNEM* mnem) {
	/* out imm, AL/AX */
	uint8_t imm = FETCH_BYTE();
	if (W) {
		MNEM("out %02X, ax", imm);
	}
	else {
		MNEM("out %02X, al", imm);
	}
}
static void in_accum_dx(I8086_MNEM* mnem) {
	/* in AL/AX, DX */
	if (W) {
		MNEM0("in ax, dx");
	}
	else {
		MNEM0("in al, dx");
	}
}
static void out_accum_dx(I8086_MNEM* mnem) {
	/* out DX, AL/AX */
	if (W) {
		MNEM0("out dx, ax");
	}
	else {
		MNEM0("out dx, al");
	}
}

static void int3(I8086_MNEM* mnem) {
	/* interrupt CC b11001100 */
	MNEM0("int 3");
}

static void int_(I8086_MNEM* mnem) {
	/* interrupt CD b11001101 */
	uint8_t type = 0;
 	if (mnem->opcode & 0x1) {
		type = FETCH_BYTE();
	}
	MNEM("int %X", type);
}
static void into(I8086_MNEM* mnem) {
	/* interrupt on overflow (CE) b11001110 */
	MNEM0("into");
}
static void iret(I8086_MNEM* mnem) {
	/* interrupt on return (CF) b11001111 */
	MNEM0("iret");
}

static int rep(I8086_MNEM* mnem) {
	/* rep/repz/repnz (F2/F3) b1111001Z */
	if (mnem->opcode & 0x1) {
		MNEM0("repz ");
	}
	else {
		MNEM0("repnz ");
	}
	mnem->opcode = FETCH_BYTE();
	return I8086_DECODE_REQ_CYCLE;
}
static int segment_override(I8086_MNEM* mnem) {
	/* (26/2E/36/3E) b001SR110 */
	MNEM("%s: ", seg_mnem[SR]);
	mnem->opcode = FETCH_BYTE();
	return I8086_DECODE_REQ_CYCLE;
}
static int lock(I8086_MNEM* mnem) {
	/* lock the bus (F0) b11110000 */
	MNEM0("lock ");
	mnem->opcode = FETCH_BYTE();
	return I8086_DECODE_REQ_CYCLE;
}

static void i8086_next(I8086_MNEM* mnem, uint16_t seg, uint16_t offset) {
	
	mnem->segment = seg;
	mnem->offset = offset;
	mnem->counter = 0;

	mnem->modrm.byte = 0;
	mnem->segment_prefix = 0xFF;
	mnem->rep_prefix = 0;
	
	mnem->str[0] = '\0'; 
	mnem->addressing_str[0] = '\0';
}

static void i8086_fetch(I8086_MNEM* mnem, uint16_t seg, uint16_t offset) {
	i8086_next(mnem, seg, offset);
	mnem->opcode = FETCH_BYTE();
}

static void i8086_decode_opcode_80(I8086_MNEM* mnem) {
	/* 0x80 - 0x83 b100000SW */
	mnem->modrm.byte = FETCH_BYTE();
	switch (mnem->modrm.reg) {
		case 0b000: // ADD
			add_rm_imm(mnem);
			break;
		case 0b001: // OR
			or_rm_imm(mnem);
			break;
		case 0b010: // ADC
			adc_rm_imm(mnem);
			break;
		case 0b011: // SBB
			sbb_rm_imm(mnem);
			break;
		case 0b100: // AND
			and_rm_imm(mnem);
			break;
		case 0b101: // SUB
			sub_rm_imm(mnem);
			break;
		case 0b110: // XOR
			xor_rm_imm(mnem);
			break;
		case 0b111: // CMP
			cmp_rm_imm(mnem);
			break;
	}
}
static int i8086_decode_opcode_d0(I8086_MNEM* mnem) {
	/* 0xD0 - 0xD3 b110100VW */
	mnem->modrm.byte = FETCH_BYTE();
	switch (mnem->modrm.reg) {
		case 0b000:
			rol(mnem);
			break;
		case 0b001:
			ror(mnem);
			break;
		case 0b010:
			rcl(mnem);
			break;
		case 0b011:
			rcr(mnem);
			break;
		case 0b100:
			shl(mnem);
			break;
		case 0b101:
			shr(mnem);
			break;
		case 0b110:
			return I8086_DECODE_UNDEFINED;
		case 0b111:
			sar(mnem);
			break;
	}
	return I8086_DECODE_OK;
}
static int i8086_decode_opcode_f6(I8086_MNEM* mnem) {
	/* F6/F7 b1111011W */
	mnem->modrm.byte = FETCH_BYTE();
	switch (mnem->modrm.reg) {
		case 0b000:
			test_rm_imm(mnem);
			break;
		case 0b001:
			return I8086_DECODE_UNDEFINED;
		case 0b010:
			not(mnem);
			break;
		case 0b011:
			neg(mnem);
			break;
		case 0b100:
			mul_rm(mnem);
			break;
		case 0b101:
			imul_rm(mnem);
			break;
		case 0b110:
			div_rm(mnem);
			break;
		case 0b111:
			idiv_rm(mnem);
			break;
	}
	return I8086_DECODE_OK;
}
static int i8086_decode_opcode_fe(I8086_MNEM* mnem) {
	/* FE/FF b1111111W */
	mnem->modrm.byte = FETCH_BYTE();
	if (W) {
		switch (mnem->modrm.reg) {
			case 0b000:
				inc_rm(mnem);
				break;
			case 0b001:
				dec_rm(mnem);
				break;
			case 0b010:
				call_intra_indirect(mnem);
				break;
			case 0b011:
				call_inter_indirect(mnem);
				break;
			case 0b100:
				jmp_intra_indirect(mnem);
				break;
			case 0b101:
				jmp_inter_indirect(mnem);
				break;
			case 0b110:
				push_rm(mnem);
				break;
			case 0b111:
				return I8086_DECODE_UNDEFINED;
		}
	}
	else {
		switch (mnem->modrm.reg) {
			case 0b000:
				inc_rm(mnem);
				break;
			case 0b001:
				dec_rm(mnem);
				break;

			case 0b010:
			case 0b011:
			case 0b100:
			case 0b101:
			case 0b110:
			case 0b111:
				return I8086_DECODE_UNDEFINED;
		}
	}
	return I8086_DECODE_OK;
}

static int i8086_decode_opcode(I8086_MNEM* mnem) {
	switch (mnem->opcode) {
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
			add_rm_reg(mnem);
			break;
		case 0x04:
		case 0x05:
			add_accum_imm(mnem);
			break;
		case 0x06:
			push_seg(mnem);
			break;
		case 0x07:
			pop_seg(mnem);
			break;
		case 0x08:
		case 0x09:
		case 0x0A:
		case 0x0B:
			or_rm_reg(mnem);
			break;
		case 0x0C:
		case 0x0D:
			or_accum_imm(mnem);
			break;
		case 0x0E:
			push_seg(mnem);
			break;
		case 0x0F: // pop cs; 8086 undocumented
			pop_seg(mnem);
			break;
		
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
			adc_rm_reg(mnem);
			break;
		case 0x14:
		case 0x15:
			adc_accum_imm(mnem);
			break;
		case 0x16:
			push_seg(mnem);
			break;
		case 0x17:
			pop_seg(mnem);
			break;
		case 0x18:
		case 0x19:
		case 0x1A:
		case 0x1B:
			sbb_rm_reg(mnem);
			break;
		case 0x1C:
		case 0x1D:
			sbb_accum_imm(mnem);
			break;
		case 0x1E:
			push_seg(mnem);
			break;
		case 0x1F:
			pop_seg(mnem);
			break;
		
		case 0x20:
		case 0x21:
		case 0x22:
		case 0x23:
			and_rm_reg(mnem);
			break;
		case 0x24:
		case 0x25:
			and_accum_imm(mnem);
			break;
		case 0x26:
			return segment_override(mnem);
		case 0x27:
			daa(mnem);
			break;
		case 0x28:
		case 0x29:
		case 0x2A:
		case 0x2B:
			sub_rm_reg(mnem);
			break;
		case 0x2C:
		case 0x2D:
			sub_accum_imm(mnem);
			break;
		case 0x2E:
			return segment_override(mnem);
		case 0x2F:
			das(mnem);
			break;
		
		case 0x30:
		case 0x31:
		case 0x32:
		case 0x33:
			xor_rm_reg(mnem);
			break;
		case 0x34:
		case 0x35:
			xor_accum_imm(mnem);
			break;
		case 0x36:
			return segment_override(mnem);
		case 0x37:
			aaa(mnem);
			break;
		case 0x38:
		case 0x39:
		case 0x3A:
		case 0x3B:
			cmp_rm_reg(mnem);
			break;
		case 0x3C:
		case 0x3D:
			cmp_accum_imm(mnem);
			break;
		case 0x3E:
			return segment_override(mnem);
		case 0x3F:
			aas(mnem);
			break;

		case 0x40:
		case 0x41:
		case 0x42:
		case 0x43:
		case 0x44:
		case 0x45:
		case 0x46:
		case 0x47:
			inc_reg(mnem);
			break;

		case 0x48:
		case 0x49:
		case 0x4A:
		case 0x4B:
		case 0x4C:
		case 0x4D:
		case 0x4E:
		case 0x4F:
			dec_reg(mnem);
			break;

		case 0x50:
		case 0x51:
		case 0x52:
		case 0x53:
		case 0x54:
		case 0x55:
		case 0x56:
		case 0x57:
			push_reg(mnem);
			break;

		case 0x58:
		case 0x59:
		case 0x5A:
		case 0x5B:
		case 0x5C:
		case 0x5D:
		case 0x5E:
		case 0x5F:
			pop_reg(mnem);
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
			jcc(mnem);
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
			jcc(mnem);
			break;

		case 0x80:
		case 0x81:
		case 0x82:
		case 0x83:
			i8086_decode_opcode_80(mnem);
			break;
		case 0x84:
		case 0x85:
			test_rm_reg(mnem);
			break;
		case 0x86:
		case 0x87:
			xchg_rm_reg(mnem);
			break;
		case 0x88:
		case 0x89:
		case 0x8A:
		case 0x8B:
			mov_rm_reg(mnem);
			break;
		case 0x8C:
			mov_seg(mnem);
			break;
		case 0x8D:
			lea(mnem);
			break;
		case 0x8E:
			mov_seg(mnem);
			break;
		case 0x8F:
			pop_rm(mnem);
			break;

		case 0x90:
			nop(mnem);
			break;
		case 0x91:
		case 0x92:
		case 0x93:
		case 0x94:
		case 0x95:
		case 0x96:
		case 0x97:
			xchg_accum_reg(mnem);
			break;
		case 0x98:
			cbw(mnem);
			break;
		case 0x99:
			cwd(mnem);
			break; 
		case 0x9A:
			call_inter_direct(mnem);
			break;
		case 0x9B:
			wait(mnem);
			break;
		case 0x9C:
			pushf(mnem);
			break;
		case 0x9D:
			popf(mnem);
			break;
		case 0x9E:
			sahf(mnem);
			break;
		case 0x9F:
			lahf(mnem);
			break;

		case 0xA0:
		case 0xA1:
		case 0xA2:
		case 0xA3:
			mov_accum_mem(mnem);
			break;
		case 0xA4:
		case 0xA5:
			movs(mnem);
			break;
		case 0xA6:
		case 0xA7:
			cmps(mnem);
			break;
		case 0xA8:
		case 0xA9:
			test_accum_imm(mnem);
			break;
		case 0xAA:
		case 0xAB:
			stos(mnem);
			break;
		case 0xAC:
		case 0xAD:
			lods(mnem);
			break;
		case 0xAE:
		case 0xAF:
			scas(mnem);
			break;

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
			mov_reg_imm(mnem);
			break;

		case 0xC0: // 8086 undocumented
		case 0xC2:
			ret_intra_add_imm(mnem);
			break;
		case 0xC1: // 8086 undocumented
		case 0xC3:
			ret_intra(mnem);
			break;
		case 0xC4:
			les(mnem);
			break;
		case 0xC5:
			lds(mnem);
			break;
		case 0xC6:
		case 0xC7:
			mov_rm_imm(mnem);
			break;
		case 0xC8: // 8086 undocumented
		case 0xCA:
			ret_inter_add_imm(mnem);
			break;
		case 0xC9: // 8086 undocumented
		case 0xCB:
			ret_inter(mnem);
			break;
		case 0xCC:
			int3(mnem);
			break;
		case 0xCD:
			int_(mnem);
			break;
		case 0xCE:
			into(mnem);
			break;
		case 0xCF:
			iret(mnem);
			break;
			
		case 0xD0:
		case 0xD1:
		case 0xD2:
		case 0xD3:
			return i8086_decode_opcode_d0(mnem);
		case 0xD4:
			aam(mnem);
			break;
		case 0xD5:
			aad(mnem);
			break;
		case 0xD6: // 8086 undocumented
			salc(mnem);
			break;
		case 0xD7:
			xlat(mnem);
			break;
		case 0xD8:
		case 0xD9:
		case 0xDA:
		case 0xDB:
		case 0xDC:
		case 0xDD:
		case 0xDE:
		case 0xDF:
			esc(mnem);
			break;

		case 0xE0:
			loopnz(mnem);
			break;
		case 0xE1:
			loopz(mnem);
			break;
		case 0xE2:
			loop(mnem);
			break;
		case 0xE3:
			jcxz(mnem);
			break;
		case 0xE4:
		case 0xE5:
			in_accum_imm(mnem);
			break;
		case 0xE6:
		case 0xE7:
			out_accum_imm(mnem);
			break;
		case 0xE8:
			call_intra_direct(mnem);
			break;
		case 0xE9:
			jmp_intra_direct(mnem);
			break;
		case 0xEA:
			jmp_inter_direct(mnem);
			break;
		case 0xEB:
			jmp_intra_direct_short(mnem);
			break;
		case 0xEC:
		case 0xED:
			in_accum_dx(mnem);
			break;
		case 0xEE:
		case 0xEF:
			out_accum_dx(mnem);
			break;

		case 0xF0:
			return lock(mnem);
		case 0xF2:
		case 0xF3:
			return rep(mnem);
		case 0xF4:
			hlt(mnem);
			break;
		case 0xF5:
			cmc(mnem);
			break;
		case 0xF6:
		case 0xF7:
			return i8086_decode_opcode_f6(mnem);
		case 0xF8:
			clc(mnem);
			break;
		case 0xF9:
			stc(mnem);
			break;
		case 0xFA:
			cli(mnem);
			break;
		case 0xFB:
			sti(mnem);
			break;
		case 0xFC:
			cld(mnem);
			break;
		case 0xFD:
			std(mnem);
			break;
		case 0xFE:
		case 0xFF:
			return i8086_decode_opcode_fe(mnem);
		default:
			return I8086_DECODE_UNDEFINED;
	}
	return I8086_DECODE_OK;
}

static int i8086_decode_instruction(I8086_MNEM* mnem) {
	int r = 0;
	do {
		r = i8086_decode_opcode(mnem);
	} while (r == I8086_DECODE_REQ_CYCLE);

	if (r == I8086_DECODE_UNDEFINED) {
		sprintf(mnem->str, "undef op: %02X", mnem->opcode);
		if (mnem->modrm.byte != 0) {
			sprintf(mnem->str + strlen(mnem->str), " /%02X", mnem->modrm.reg);
		}
	}
	return r;
}

int i8086_mnem(I8086_MNEM* mnem) {
	i8086_fetch(mnem, CS, mnem->state->ip);
	return i8086_decode_instruction(mnem);
}

int i8086_mnem_at(I8086_MNEM* mnem, uint16_t seg, uint16_t offset) {
	i8086_fetch(mnem, seg, offset);
	return i8086_decode_instruction(mnem);
}

#endif
