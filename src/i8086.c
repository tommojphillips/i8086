/* i8086.c
 * Thomas J. Armytage 2025 ( https://github.com/tommojphillips/ )
 * Intel 8086 CPU
 */

/* Source Material
 * The 8086 Family Users Manual OCT79 by Intel
 * The 8086 Book by Russell Rector, George Alexy
 * Programming The 8086/8088 by James W. Coffron
 * Undocumented 8086 instructions, explained by the microcode - https://www.righto.com/2023/07/undocumented-8086-instructions.html
*/

#include <stdint.h>

#include "i8086.h"
#include "i8086_alu.h"

#define PSW cpu->status.word

#define DBZ cpu->dbz
#define NMI cpu->pins.nmi
#define INTR cpu->pins.intr

#define SF cpu->status.sf
#define CF cpu->status.cf
#define ZF cpu->status.zf
#define PF cpu->status.pf
#define OF cpu->status.of
#define AF cpu->status.af
#define DF cpu->status.df
#define TF cpu->status.tf
#define IF cpu->status.in

#define IP cpu->ip

#define AL cpu->registers[REG_AL].l   // accum low byte 8bit register
#define AH cpu->registers[REG_AL].h   // accum high byte 8bit register
#define AX cpu->registers[REG_AX].r16 // accum 16bit register

#define CL cpu->registers[REG_CL].l   // count low byte 8bit register
#define CH cpu->registers[REG_CL].h   // count high byte 8bit register
#define CX cpu->registers[REG_CX].r16 // count 16bit register

#define DL cpu->registers[REG_DL].l   // data low byte 8bit register
#define DH cpu->registers[REG_DL].h   // data high byte 8bit register
#define DX cpu->registers[REG_DX].r16 // data 16bit register

#define BL cpu->registers[REG_BL].l   // base low byte 8bit register
#define BH cpu->registers[REG_BL].h   // base high byte 8bit register
#define BX cpu->registers[REG_BX].r16 // base 16bit register

#define SP cpu->registers[REG_SP].r16 // stack pointer 16bit register
#define BP cpu->registers[REG_BP].r16 // base pointer 16bit register
#define SI cpu->registers[REG_SI].r16 // src index 16bit register
#define DI cpu->registers[REG_DI].r16 // dest index 16bit register

#define ES cpu->segments[SEG_ES] // extra segment register
#define CS cpu->segments[SEG_CS] // code segment register
#define SS cpu->segments[SEG_SS] // stack segment register
#define DS cpu->segments[SEG_DS] // data segment register

 // byte/word operation. 0 = byte; 1 = word
#define W (cpu->opcode & 0x1)

// byte/word operation. 0 = byte; 1 = word
#define WREG (cpu->opcode & 0x8) 

// byte/word operation. b00 = byte; b01 = word; b11 = byte sign extended to word
#define SW (cpu->opcode & 0x3)

// seg register
#define SR ((cpu->opcode >> 0x3) & 0x3)

// 0 = (count = 1); 1 = (count = CL)
#define VW (cpu->opcode & 0x2)

// register direction (reg -> r/m) or (r/m -> reg)
#define D (cpu->opcode & 0x2) 

// jump condition
#define CCCC (cpu->opcode & 0x0F)

/* Get segment override prefix */
#define GET_SEG_OVERRIDE(seg) ((cpu->segment_prefix != 0xFF) ? cpu->segment_prefix : seg)

/* Get 20bit address SEG:ADDR */
#define GET_ADDR(seg, addr) ((cpu->segments[seg] << 4) + addr)

/* Get 20bit code segment address CS:ADDR */
#define IP_ADDR            GET_ADDR(SEG_CS, IP)

/* Get 20bit stack segment address SS:ADDR */
#define STACK_ADDR(addr)      GET_ADDR(SEG_SS, addr)
#define STACK_ADDR_OR(addr)   GET_ADDR(GET_SEG_OVERRIDE(SEG_SS), addr)

/* Get 20bit data segment address DS:ADDR */
#define DATA_ADDR(addr)    GET_ADDR(GET_SEG_OVERRIDE(SEG_DS), addr)

/* Get 20bit extra segment address ES:ADDR */
#define EXTRA_ADDR(addr)   GET_ADDR(SEG_ES, addr)

/* Read byte from [addr] */
#define READ_BYTE(addr)        cpu->funcs.read_mem_byte(addr)

/* Write byte to [addr] */
#define WRITE_BYTE(addr,value) cpu->funcs.write_mem_byte(addr,value)

/* Fetch byte at CS:IP. Inc IP by 1 */
#define FETCH_BYTE()           cpu->funcs.read_mem_byte(IP_ADDR); IP += 1

/* read word from [addr] */
#define READ_WORD(addr)        cpu->funcs.read_mem_word(addr)

/* write word to [addr] */
#define WRITE_WORD(addr,value) cpu->funcs.write_mem_word(addr,value)

/* Fetch word at CS:IP. Inc IP by 2 */
#define FETCH_WORD()           cpu->funcs.read_mem_word(IP_ADDR); IP += 2

/* Read byte from IO port word */
#define READ_IO_WORD(port)     cpu->funcs.read_io_word(port)

/* Read byte from IO port */
#define READ_IO(port)          cpu->funcs.read_io_byte(port)

/* Write byte to IO port */
#define WRITE_IO_WORD(port,value) cpu->funcs.write_io_word(port, value)

/* Write byte to IO port */
#define WRITE_IO(port,value)   cpu->funcs.write_io_byte(port, value)

/* Get memory pointer */
#define GET_MEM_PTR(addr)      cpu->funcs.get_mem_ptr(addr)

/* Get 8bit register ptr 
	Registers are layed out in memory as: AX:{ LO, HI }, CX:{ LO, HI }, DX:{ LO, HI }, BX:{ LO, HI } etc
	The lower 4 16bit registers can be indexed as 8bit registers.
	Therefore bit1 and bit2 refer to the selected register pair (AX/CX/DX/BX).
	bit3 tells us if the LO byte (0) or the HI byte (1) of the register pair is selected
	if bit3 is clr. bits b00, b01, b10, b11 refer to the LO byte of the register. (AL, CL, DL, BL)
	if bit3 is set. bits b00, b01, b10, b11 refer to the HI byte of the register. (AH, CH, DH, BH) */
#define GET_REG8(reg)          (&cpu->registers[reg & 3].l + ((reg >> 2) & 1))

/* Get 16bit register ptr */
#define GET_REG16(reg)         (&cpu->registers[reg & 7].r16)

/* Get 16bit segment ptr */
#define GET_SEG(seg)         (&cpu->segments[seg & 3])

#define CYCLES(x) cpu->cycles += x
#define CYCLES_RM(reg_cyc, mem_cyc) cpu->cycles += (cpu->modrm.mod == 0b11 ? reg_cyc : mem_cyc)
#define CYCLES_RM_D(reg_cyc, mr_cyc, rm_cyc) cpu->cycles += (cpu->modrm.mod == 0b11 ? reg_cyc : !D ? mr_cyc : rm_cyc)

#define INT_DBZ      0 // ITC 0
#define INT_TRAP     1 // ITC 1
#define INT_NMI      2 // ITC 2
#define INT_3        3 // ITC 3
#define INT_OVERFLOW 4 // ITC 4

/* Internal flag F1. Signals that a rep prefix is in use for this decode cycle */
#define F1  (cpu->internal_flags & INTERNAL_FLAG_F1)

/* Internal flag F1Z. Signals which rep (repz/repnz) is in use for this decode cycle */
#define F1Z (cpu->internal_flags & INTERNAL_FLAG_F1Z)

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

void i8086_request_int(I8086* cpu, uint8_t type) {
	INTR = 1;
	cpu->intr_type = type;
}
void i8086_int(I8086* cpu, uint8_t type) {
	/* 3 things can delay an interrupt:
		an interrupt delay micro-instruction.
		an instruction that modifies a segment register.
		or an instruction prefix. */
	if (cpu->opcode == 0x8E || /* mov seg, rm */
		cpu->opcode == 0x07 || /* pop seg */
		cpu->opcode == 0x0F || /* pop seg */
		cpu->opcode == 0x17 || /* pop seg */
		cpu->opcode == 0x1F || /* pop seg */
		cpu->opcode == 0xC4 || /* les */
		cpu->opcode == 0xC5)   /* lds */ {
		return;
	}

	i8086_push_word(cpu, PSW);
	i8086_push_word(cpu, CS);
	i8086_push_word(cpu, IP);
	uint20_t addr = type * 4;
	IP = READ_WORD(addr);
	CS = READ_WORD(addr + 2);
	IF = 0;
	TF = 0;
}
void i8086_enable_int(I8086* cpu, int enable) {
	if (enable) {
		IF = 1;
	}
	else {
		IF = 0;
	}
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

/* Use the mod r/m byte to calculate a 16bit indirect address eg (BX+SI) */
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

/* Use the mod r/m byte to calculate a 16bit address eg (offset) */
static uint16_t modrm_get_address(I8086* cpu) {
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

/* Use the mod r/m byte to calculate a 20bit indirect address eg (segment:BX+SI) */
static uint20_t modrm_get_effective_base_address(I8086* cpu) {
	// Get 20bit effective base address
	switch (cpu->modrm.rm) {
		case 0b000: // base rel indexed - BX + SI
			CYCLES(7);
			return DATA_ADDR(BX + SI);

		case 0b001: // base rel indexed - BX + DI
			CYCLES(8);
			return DATA_ADDR(BX + DI);

		case 0b010: // base rel indexed stack - BP + SI
			CYCLES(8);
			return STACK_ADDR_OR(BP + SI);

		case 0b011: // base rel indexed stack - BP + DI
			CYCLES(7);
			return STACK_ADDR_OR(BP + DI);

		case 0b100: // implied SI
			CYCLES(5);
			return DATA_ADDR(SI);

		case 0b101: // implied DI
			CYCLES(5);
			return DATA_ADDR(DI);

		case 0b110: // implied BP
			CYCLES(5);
			return STACK_ADDR_OR(BP);

		case 0b111: // implied BX
			CYCLES(5);
			return DATA_ADDR(BX);
	}
	return 0;
}

/* Use the mod r/m byte to calculate a 20bit address eg (segment:offset) */
static uint20_t modrm_get_effective_address(I8086* cpu) {
	// Get 20bit effective address
	uint20_t addr = 0;
	switch (cpu->modrm.mod) {

		case 0b00:
			if (cpu->modrm.rm == 0b110) {
				// displacement mode - [ disp16 ]
				uint16_t imm = FETCH_WORD();
				addr = DATA_ADDR(imm); 
				CYCLES(6);
			}
			else {
				// register mode - [ base16 ]
				addr = modrm_get_effective_base_address(cpu);
			}
			break;

		case 0b01:
			// memory mode; 8bit displacement - [ base16 + disp8 ]
			addr = modrm_get_effective_base_address(cpu) + FETCH_BYTE();
			CYCLES(4);
			break;

		case 0b10:
			// memory mode; 16bit displacement - [ base16 + disp16 ]
			addr = modrm_get_effective_base_address(cpu) + FETCH_WORD(); 
			CYCLES(4);
			break;
	}
	return addr;
}

/* Use the mod r/m byte to calculate a 16bit pointer to a register or memory location */
static uint16_t* modrm_get_ptr16(I8086* cpu) {
	// Get R/M pointer to 16bit value
	if (cpu->modrm.mod == 0b11) {
		return GET_REG16(cpu->modrm.rm);
	}
	else {
		return GET_MEM_PTR(modrm_get_effective_address(cpu));
	}
}

/* Use the mod r/m byte to calculate a 8bit pointer to a register or memory location */
static uint8_t* modrm_get_ptr8(I8086* cpu) {
	// Get R/M pointer to 8bit value
	if (cpu->modrm.mod == 0b11) {
		return GET_REG8(cpu->modrm.rm);
	}
	else {
		return GET_MEM_PTR(modrm_get_effective_address(cpu));
	}
}

static int16_t sign_extend8_16(uint8_t value) {
	int16_t s = 0;
	if (value > 0x7F) {
		s |= 0xFF00;
	}
	s |= value;
	return s;
}

static void fetch_modrm(I8086* cpu) {
	cpu->modrm.byte = FETCH_BYTE();
}

/* Opcodes */

static void add_rm_imm(I8086* cpu) {
	/* add r/m, imm (80/81/82/83, R/M reg = b000) b100000SW */
	if (W) {
		if (SW == 0b01) {
			/* reg16, disp16 
			0x81 */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint16_t imm = FETCH_WORD();
			alu_add16(cpu, rm, imm);
		}
		else {
			/* reg16, disp8 
			0x83 is 8bit sign extended to 16bit */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint8_t imm = FETCH_BYTE();
			int16_t se = sign_extend8_16(imm);
			alu_add16(cpu, rm, se);
		}
	}
	else {
		/* reg8, disp8 
		0x80, 0x82 */
		uint8_t* rm = modrm_get_ptr8(cpu);
		uint8_t imm = FETCH_BYTE();
		alu_add8(cpu, rm, imm);
	}
	CYCLES_RM(4, 23);
}
static void add_rm_reg(I8086* cpu) {
	/* add r/m, reg (00/01/02/03) b000000DW */
	fetch_modrm(cpu);
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
	CYCLES_RM_D(3, 24, 13);
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
	CYCLES(4);
}

static void or_rm_imm(I8086* cpu) {
	/* or r/m, imm (80/81/82/83, R/M reg = b001) b100000SW */
	if (W) {
		if (SW == 0b01) {
			/* reg16, disp16 
			0x81 */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint16_t imm = FETCH_WORD();
			alu_or16(cpu, rm, imm);
		}
		else {
			/* reg16, disp8 
			0x83 is 8bit sign extended to 16bit */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint8_t imm = FETCH_BYTE();
			int16_t se = sign_extend8_16(imm);
			alu_or16(cpu, rm, se);
		}
	}
	else {
		/* reg8, disp8 
		0x80, 0x82 */
		uint8_t* rm = modrm_get_ptr8(cpu);
		uint8_t imm = FETCH_BYTE();
		alu_or8(cpu, rm, imm);
	}
	CYCLES_RM(4, 23);
}
static void or_rm_reg(I8086* cpu) {
	/* or r/m, reg (08/0A/09/0B) b000010DW */
	fetch_modrm(cpu);
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
	CYCLES_RM_D(3, 24, 13);
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
	CYCLES(4);
}

static void adc_rm_imm(I8086* cpu) {
	/* adc r/m, imm (80/81/82/83, R/M reg = b010) b100000SW */
	if (W) {
		if (SW == 0b01) {
			/* reg16, disp16 
			0x81 */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint16_t imm = FETCH_WORD();
			alu_adc16(cpu, rm, imm);
		}
		else {
			/* reg16, disp8 
			0x83 is 8bit sign extended to 16bit */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint8_t imm = FETCH_BYTE();
			int16_t se = sign_extend8_16(imm);
			alu_adc16(cpu, rm, se);
		}
	}
	else {
		/* reg8, disp8 
		0x80, 0x82 */
		uint8_t* rm = modrm_get_ptr8(cpu);
		uint8_t imm = FETCH_BYTE();
		alu_adc8(cpu, rm, imm);
	}
	CYCLES_RM(4, 23);
}
static void adc_rm_reg(I8086* cpu) {
	/* adc r/m, reg (10/12/11/13) b000100DW */
	fetch_modrm(cpu);
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
	CYCLES_RM_D(3, 24, 13);
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
	CYCLES(4);
}

static void sbb_rm_imm(I8086* cpu) {
	/* sbb r/m, imm (80/81/82/83, R/M reg = b011)  b100000SW */
	if (W) {
		if (SW == 0b01) {
			/* reg16 + disp16 
			0x81 */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint16_t imm = FETCH_WORD();
			alu_sbb16(cpu, rm, imm);
		}
		else {
			/* reg16 + disp8 
			0x83 is 8bit sign extended to 16bit */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint8_t imm = FETCH_BYTE();
			int16_t se = sign_extend8_16(imm);
			alu_sbb16(cpu, rm, se);
		}
	}
	else {
		/* reg8, disp8 
		0x80, 0x82 */
		uint8_t* rm = modrm_get_ptr8(cpu);
		uint8_t imm = FETCH_BYTE();
		alu_sbb8(cpu, rm, imm);
	}
	CYCLES_RM(4, 23);
}
static void sbb_rm_reg(I8086* cpu) {
	/* sbb r/m, reg (18/1A/19/1B) b000110DW */
	fetch_modrm(cpu);
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
	CYCLES_RM_D(3, 24, 13);
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
	CYCLES(4);
}

static void and_rm_imm(I8086* cpu) {
	/* and r/m, imm (80/81/82/83, R/M reg = b100) b100000SW */
	if (W) {
		if (SW == 0b01) {
			/* reg16, disp16 
			0x81 */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint16_t imm = FETCH_WORD();
			alu_and16(cpu, rm, imm);
		}
		else {
			/* reg16, disp8 
			0x83 is 8bit sign extended to 16bit */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint8_t imm = FETCH_BYTE();
			int16_t se = sign_extend8_16(imm);
			alu_and16(cpu, rm, se);
		}
	}
	else {
		/* reg8, disp8
		0x80, 0x82 */
		uint8_t* rm = modrm_get_ptr8(cpu);
		uint8_t imm = FETCH_BYTE();
		alu_and8(cpu, rm, imm);
	}
	CYCLES_RM(4, 23);
}
static void and_rm_reg(I8086* cpu) {
	/* and r/m, reg (20/22/21/23) b001000DW */
	fetch_modrm(cpu);
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
	CYCLES_RM_D(3, 24, 13);
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
	CYCLES(4);
}

static void sub_rm_imm(I8086* cpu) {
	/* sub r/m, imm (80/81, R/M reg = b101) b100000SW */
	if (W) {
		if (SW == 0b01) {
			/* reg16, disp16 
			0x81 */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint16_t imm = FETCH_WORD();
			alu_sub16(cpu, rm, imm);
		}
		else {
			/* reg16, disp8 
			0x83 is 8bit sign extended to 16bit */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint8_t imm = FETCH_BYTE();
			int16_t se = sign_extend8_16(imm);
			alu_sub16(cpu, rm, se);
		}
	}
	else {
		/* reg8, disp8 
		0x80, 0x82 */
		uint8_t* rm = modrm_get_ptr8(cpu);
		uint8_t imm = FETCH_BYTE();
		alu_sub8(cpu, rm, imm);
	}
	CYCLES_RM(4, 23);
}
static void sub_rm_reg(I8086* cpu) {
	/* sub r/m, reg (28/2A/29/2B) b001010DW */
	fetch_modrm(cpu);
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
	CYCLES_RM_D(3, 24, 13);
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
	CYCLES(4);
}

static void xor_rm_imm(I8086* cpu) {
	/* xor r/m, imm (80/81/82/83, R/M reg = b110) b100000SW */
	if (W) {
		if (SW == 0b01) {
			/* reg16, disp16 
			0x81 */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint16_t imm = FETCH_WORD();
			alu_xor16(cpu, rm, imm);
		}
		else {
			/* reg16, disp8 
			0x83 is 8bit sign extended to 16bit */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint8_t imm = FETCH_BYTE();
			int16_t se = sign_extend8_16(imm);
			alu_xor16(cpu, rm, se);
		}
	} 
	else {
		/* reg8, disp8 
		0x80, 0x82 */
		uint8_t* rm = modrm_get_ptr8(cpu);
		uint8_t imm = FETCH_BYTE();
		alu_xor8(cpu, rm, imm);
	}
	CYCLES_RM(4, 23);
}
static void xor_rm_reg(I8086* cpu) {
	/* xor r/m, reg (30/32/31/33) b001100DW */
	fetch_modrm(cpu);
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
	CYCLES_RM_D(3, 24, 13);
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
	CYCLES(4);
}

static void cmp_rm_imm(I8086* cpu) {
	/* cmp r/m, imm (80/81/82/83, R/M reg = b111)  b100000SW */
	if (W) {
		if (SW == 0b01) {
			/* reg16, disp16 
			0x81 */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint16_t imm = FETCH_WORD();
			alu_cmp16(cpu, *rm, imm);
		}
		else {
			/* reg16, disp8 
			0x83 is 8bit sign extended to 16bit */
			uint16_t* rm = modrm_get_ptr16(cpu);
			uint8_t imm = FETCH_BYTE();
			int16_t se = sign_extend8_16(imm);
			alu_cmp16(cpu, *rm, se);
		}
	}
	else {
		/* reg8, disp8 
		0x80, 0x82 */
		uint8_t* rm = modrm_get_ptr8(cpu);
		uint8_t imm = FETCH_BYTE();
		alu_cmp8(cpu, *rm, imm);
	}
	CYCLES_RM(4, 14);
}
static void cmp_rm_reg(I8086* cpu) {
	/* cmp r/m, reg (38/39/3A/3B) b001110DW */
	fetch_modrm(cpu);
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
	CYCLES_RM(3, 13);
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
	CYCLES(4);
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
	CYCLES_RM(5, 13);
}
static void test_rm_reg(I8086* cpu) {
	/* test r/m, reg (84/85) b1000010W */
	fetch_modrm(cpu);
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
	CYCLES_RM(3, 13);
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
	CYCLES(4);
}

static void daa(I8086* cpu) {
	/* Decimal Adjust for Addition (27) b00100111 */
	alu_daa(cpu, &AL);
	CYCLES(4);
}
static void das(I8086* cpu) {
	/* Decimal Adjust for Subtraction (2F) b00101111 */
	alu_das(cpu, &AL);
	CYCLES(4);
}
static void aaa(I8086* cpu) {
	/* ASCII Adjust for Addition (37) b00110111 */
	alu_aaa(cpu, &AL, &AH);
	CYCLES(8);
}
static void aas(I8086* cpu) {
	/* ASCII Adjust for Subtraction (3F) b00111111 */
	alu_aas(cpu, &AL, &AH);
	CYCLES(8);
}
static void aam(I8086* cpu) {
	/* ASCII Adjust for Multiply (D4 0A) b11010100 00001010 */
	uint8_t divisor = FETCH_BYTE(); // undocumented operand; normally 0x0A
	alu_aam(cpu, &AL, &AH, divisor);
	CYCLES(83);
}
static void aad(I8086* cpu) {
	/* ASCII Adjust for Division (D5 0A) b11010101 00001010 */
	uint8_t divisor = FETCH_BYTE(); // undocumented operand; normally 0x0A
	alu_aad(cpu, &AL, &AH, divisor);
	CYCLES(60);
}
static void salc(I8086* cpu) {
	/* set carry in AL (D6) b11010110 undocumented opcode */
	if (CF) {
		AL = 0xFF;
	}
	else {
		AL = 0;
	}
	CYCLES(2); // ????
}

static void push_seg(I8086* cpu) {
	/* Push seg16 (06/0E/16/1E) b000SR110 */
	i8086_push_word(cpu, cpu->segments[SR]);
	CYCLES(14);
}
static void pop_seg(I8086* cpu) {
	/* Pop seg16 (07/0F/17/1F) b000SR111 */
	i8086_pop_word(cpu, &cpu->segments[SR]);
	CYCLES(12);
}
static void push_reg(I8086* cpu) {
	/* Push reg16 (50-57) b01010REG */
	i8086_push_word(cpu, cpu->registers[cpu->opcode & 7].r16);
	CYCLES(15);
}
static void pop_reg(I8086* cpu) {
	/* Pop reg16 (58-5F) b01011REG */
	i8086_pop_word(cpu, GET_REG16(cpu->opcode));
	CYCLES(12);
}
static void push_rm(I8086* cpu) {
	/* Push R/M (FF, R/M reg = 110) b11111111 */
	uint16_t* rm = modrm_get_ptr16(cpu);
	i8086_push_word(cpu, *rm);
	CYCLES(24);
}
static void pop_rm(I8086* cpu) {
	/* Pop R/M (8F) b10001111 */
	uint16_t* rm = modrm_get_ptr16(cpu);
	i8086_pop_word(cpu, rm);
	CYCLES(25);
}
static void pushf(I8086* cpu) {
	/* push psw (9C) b10011100 */
	PSW &= 0x7D5;
	i8086_push_word(cpu, PSW);
	CYCLES(14);
}
static void popf(I8086* cpu) {
	/* pop psw (9D) b10011101 */
	i8086_pop_word(cpu, &PSW);
	PSW &= 0x7D5;
	CYCLES(12);
}

static void nop(I8086* cpu) {
	/* nop (90) b10010000 */
	(void)cpu;
	CYCLES(3);
}
static void xchg_accum_reg(I8086* cpu) {
	/* xchg AX, reg16 (91 - 97) b10010REG */	
	uint16_t tmp = AX;
	uint16_t* reg = GET_REG16(cpu->opcode);
	AX = *reg;
	*reg = tmp;
	CYCLES(3);
}
static void xchg_rm_reg(I8086* cpu) {
	/* xchg R/M, reg16 (86/87) b1000011W */
	fetch_modrm(cpu);
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
	CYCLES_RM(4, 25);
}

static void cbw(I8086* cpu) {
	/* Convert byte to word (98) b10011000 */
	if (AL & 0x80) {
		AH = 0xFF;
	}
	else {
		AH = 0;
	}
	CYCLES(2);
}
static void cwd(I8086* cpu) {
	/* Convert word to dword (99) b10011001 */
	if (AX & 0x8000) {
		DX = 0xFFFF;
	}
	else {
		DX = 0;
	}
	CYCLES(5);
}

static void wait(I8086* cpu) {
	/* wait (9B) b10011011 */
	if (!cpu->pins.test) {
		IP -= 1;
	}
	CYCLES(4);
}

static void sahf(I8086* cpu) {
	/* Store AH into flags (9E) b10011110 */
	PSW &= 0xFF00;
	PSW |= AH & 0xD5;
	CYCLES(4);
}
static void lahf(I8086* cpu) {
	/* Load flags into AH (9F) b10011111 */
	AH = PSW & 0xD5;
	CYCLES(4);
}

static void hlt(I8086* cpu) {
	/* Halt CPU (F4) b11110100 */
	IP -= 1;
	CYCLES(2);
}
static void cmc(I8086* cpu) {
	// Complement carry flag (F5) b11110101
	CF = ~CF;
	CYCLES(2);
}
static void clc(I8086* cpu) {
	// clear carry flag (F8) b11111000
	CF = 0;
	CYCLES(2);
}
static void stc(I8086* cpu) {
	// set carry flag (F9) b11111001
	CF = 1;
	CYCLES(2);
}
static void cli(I8086* cpu) {
	// clear interrupt flag (FA) b11111010
	IF = 0;
	CYCLES(2);
}
static void sti(I8086* cpu) {
	// set interrupt flag (FB) b1111011
	IF = 1;
	CYCLES(2);
}
static void cld(I8086* cpu) {
	// clear direction flag (FC) b11111100
	DF = 0;
	CYCLES(2);
}
static void std(I8086* cpu) {
	// set direction flag (FD) b11111101
	DF = 1;
	CYCLES(2);
}

static void inc_reg(I8086* cpu) {
	/* Inc reg16 (40-47) b01000REG */
	alu_inc16(cpu, GET_REG16(cpu->opcode));
	CYCLES(3);
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
	CYCLES_RM(3, 23);
}

static void dec_reg(I8086* cpu) {
	/* Dec reg16 (48-4F) b01001REG */
	alu_dec16(cpu, GET_REG16(cpu->opcode));
	CYCLES(3);
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
	CYCLES_RM(3, 23);
}

static void rol(I8086* cpu) {
	/* Rotate left (D0/D1/D2/D3, R/M reg = 000) b110100VW */
	uint8_t count = 1;
	if (VW) {
		count = CL;
		CYCLES(5 + (4 * count));
	}
	if (W) {
		uint16_t* rm = modrm_get_ptr16(cpu);
		alu_rol16(cpu, rm, count);
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		alu_rol8(cpu, rm, count);
	}
	CYCLES_RM(2, 23);
}
static void ror(I8086* cpu) {
	/* Rotate left (D0/D1/D2/D3, R/M reg = 001) b110100VW */
	uint8_t count = 1;
	if (VW) {
		count = CL;
		CYCLES(5 + (4 * count));
	}
	if (W) {
		uint16_t* rm = modrm_get_ptr16(cpu);
		alu_ror16(cpu, rm, count);
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		alu_ror8(cpu, rm, count);
	}
	CYCLES_RM(2, 23);
}
static void rcl(I8086* cpu) {
	/* Rotate through carry left (D0/D1/D2/D3, R/M reg = 010) b110100VW */
	uint8_t count = 1;
	if (VW) {
		count = CL;
		CYCLES(5 + (4 * count));
	}
	if (W) {
		uint16_t* rm = modrm_get_ptr16(cpu);
		alu_rcl16(cpu, rm, count);
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		alu_rcl8(cpu, rm, count);
	}
	CYCLES_RM(2, 23);
}
static void rcr(I8086* cpu) {
	/* Rotate through carry right (D0/D1/D2/D3, R/M reg = 011) b110100VW */
	uint8_t count = 1;
	if (VW) {
		count = CL;
		CYCLES(5 + (4 * count));
	}
	if (W) {
		uint16_t* rm = modrm_get_ptr16(cpu);
		alu_rcr16(cpu, rm, count);
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		alu_rcr8(cpu, rm, count);
	}
	CYCLES_RM(2, 23);
}
static void shl(I8086* cpu) {
	/* Shift left (D0/D1/D2/D3, R/M reg = 100) b110100VW */
	uint8_t count = 1;
	if (VW) {
		count = CL;
		CYCLES(5 + (4 * count));
	}
	if (W) {
		uint16_t* rm = modrm_get_ptr16(cpu);
		alu_shl16(cpu, rm, count);
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		alu_shl8(cpu, rm, count);
	}
	CYCLES_RM(2, 23);
}
static void shr(I8086* cpu) {
	/* Shift Logical right (D0/D1/D2/D3, R/M reg = 101) b110100VW */
	uint8_t count = 1;
	if (cpu->opcode & 0x2) {
		count = CL;
		CYCLES(5 + (4 * count));
	}
	if (W) {
		uint16_t* rm = modrm_get_ptr16(cpu);
		alu_shr16(cpu, rm, count);
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		alu_shr8(cpu, rm, count);
	}
	CYCLES_RM(2, 23);
}
static void sar(I8086* cpu) {
	/* Shift Arithmetic right (D0/D1/D2/D3, R/M reg = 111) b110100VW */
	uint8_t count = 1;
	if (cpu->opcode & 0x2) {
		count = CL;
		CYCLES(5 + (4 * count));
	}
	if (W) {
		uint16_t* rm = modrm_get_ptr16(cpu);
		alu_sar16(cpu, rm, count);
	}
	else {
		uint8_t* rm = modrm_get_ptr8(cpu);
		alu_sar8(cpu, rm, count);
	}
	CYCLES_RM(2, 23);
}

static int jump_condition(I8086* cpu) {
	switch (CCCC) {
		case I8086_JCC_JO:
			if (OF) return 1;
			break;
		case I8086_JCC_JNO:
			if (!OF) return 1;
			break;
		case I8086_JCC_JC:
			if (CF) return 1;
			break;
		case I8086_JCC_JNC:
			if (!CF) return 1;
			break;
		case I8086_JCC_JZ:
			if (ZF) return 1;
			break;
		case I8086_JCC_JNZ:
			if (!ZF) return 1;
			break;
		case I8086_JCC_JBE:
			if (CF || ZF) return 1;
			break;
		case I8086_JCC_JA:
			if (!CF && !ZF) return 1;
			break;
		case I8086_JCC_JS:
			if (SF) return 1;
			break;
		case I8086_JCC_JNS:
			if (!SF) return 1;
			break;
		case I8086_JCC_JPE:
			if (PF) return 1;
			break;
		case I8086_JCC_JPO:
			if (!PF) return 1;
			break;
		case I8086_JCC_JL:
			if (SF != OF) return 1;
			break;
		case I8086_JCC_JGE:
			if (SF == OF) return 1;
			break;
		case I8086_JCC_JLE:
			if (ZF || SF != OF) return 1;
			break;
		case I8086_JCC_JG:
			if (!ZF && SF == OF) return 1;
			break;
	}
	return 0;
}
static void jcc(I8086* cpu) {
	/* conditional jump(70-7F) b011XCCCC
	   8086 cpu decode 60-6F the same as 70-7F */
	uint8_t imm = FETCH_BYTE();
	if (jump_condition(cpu)) {
		uint16_t offset = sign_extend8_16(imm);
		IP += offset;
		CYCLES(16);
	}
	else {
		CYCLES(4);
	}
}
static void jcxz(I8086* cpu) {
	/* jump if CX zero (E3) b11100011 */
	uint8_t imm = FETCH_BYTE();
	if (CX == 0) {
		uint16_t offset = sign_extend8_16(imm);
		IP += offset;
		CYCLES(18);
	}
	else {
		CYCLES(6);
	}
}

static void jmp_intra_direct_short(I8086* cpu) {
	/* Jump short imm8 (EB) b11101011 */
	uint8_t imm = FETCH_BYTE();
	uint16_t se = sign_extend8_16(imm);
	IP += se;
	CYCLES(15);
}
static void jmp_intra_direct(I8086* cpu) {
	/* Jump near  imm16 (E9) b11101001 */
	uint16_t imm = FETCH_WORD();
	IP += imm;
	CYCLES(15);
}
static void jmp_intra_indirect(I8086* cpu) {
	/* Jump near indirect (FF, R/M reg = 100) b11111111 */	
	uint16_t* rm = modrm_get_ptr16(cpu);
	IP = *rm;
	CYCLES_RM(11, 18);
}

static void jmp_inter_direct(I8086* cpu) {
	/* Jump far addr:seg (EA) b11101010 */
	uint16_t imm = FETCH_WORD();
	uint16_t imm2 = FETCH_WORD();
	IP = imm;
	CS = imm2;
	CYCLES(15);
}
static void jmp_inter_indirect(I8086* cpu) {
	/* Jump inter indirect (FF, R/M reg = 101) b11111111 */
	uint20_t ea = modrm_get_effective_address(cpu);
	IP = READ_WORD(ea);
	CS = READ_WORD(ea + 2);
	CYCLES(24);
}

static void call_intra_direct(I8086* cpu) {
	/* Call disp (E8) b11101000 */
	uint16_t imm = FETCH_WORD();
	i8086_push_word(cpu, IP);
	IP += imm;
	CYCLES(23);
}
static void call_intra_indirect(I8086* cpu) {
	/* Call mem/reg (FF, R/M reg = 010) b11111111 */
	uint16_t* rm = modrm_get_ptr16(cpu);
	i8086_push_word(cpu, IP);
	IP = *rm;
	CYCLES_RM(20, 29);
}

static void call_inter_direct(I8086* cpu) {
	/* Call addr:seg (9A) b10011010 */
	uint16_t imm = FETCH_WORD();
	uint16_t imm2 = FETCH_WORD();
	i8086_push_word(cpu, CS);
	i8086_push_word(cpu, IP);
	IP = imm;
	CS = imm2;
	CYCLES(36);
}
static void call_inter_indirect(I8086* cpu) {
	/* Call mem (FF, R/M reg = 011) b11111111 */	
	uint20_t ea = modrm_get_effective_address(cpu);
	i8086_push_word(cpu, CS);
	i8086_push_word(cpu, IP);
	IP = READ_WORD(ea);
	CS = READ_WORD(ea + 2);
	CYCLES(53);
}

static void ret_intra_add_imm(I8086* cpu) {
	/* Ret imm16 (C2) b110000X0 
	   undocumented* C0 decodes identically to C2 */
	uint16_t imm = FETCH_WORD();
	i8086_pop_word(cpu, &IP);
	SP += imm;
	CYCLES(24);
}
static void ret_intra(I8086* cpu) {
	/* Ret (C3) b110000X1 
	undocumented* C1 decodes identically to C3 */
	i8086_pop_word(cpu, &IP);
	CYCLES(20);
}
static void ret_inter_add_imm(I8086* cpu) {
	/* Ret imm16 (CA) b110010X0 
	undocumented* C8 decodes identically to CA */
	uint16_t imm = FETCH_WORD();
	i8086_pop_word(cpu, &IP);
	i8086_pop_word(cpu, &CS);
	SP += imm;
	CYCLES(33);
}
static void ret_inter(I8086* cpu) {
	/* Ret (CB) b110010X1 
	undocumented* C9 decodes identically to CB */
	i8086_pop_word(cpu, &IP);
	i8086_pop_word(cpu, &CS);
	CYCLES(34);
}

static void mov_rm_imm(I8086* cpu) {
	/* mov r/m, imm (C6/C7) b1100011W */
	fetch_modrm(cpu);
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
	CYCLES(14);
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
	CYCLES(4);
}
static void mov_rm_reg(I8086* cpu) {
	/* mov r/m, reg (88/89/8A/8B) b100010DW */
	fetch_modrm(cpu);
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
	CYCLES_RM_D(2, 13, 12);
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
	CYCLES(14);
}
static void mov_seg(I8086* cpu) {
	/* mov r/m, seg (8C/8E) b100011D0 */
	fetch_modrm(cpu);
	uint16_t* rm = modrm_get_ptr16(cpu);
	uint16_t* seg = GET_SEG(cpu->modrm.reg);
	get_direction(cpu, &seg, &rm);
	*rm = *seg;
	CYCLES_RM_D(2, 13, 12);
}

static void lea(I8086* cpu) {
	/* lea reg16, [r/m] (8D) b10001101 */
	fetch_modrm(cpu);
	uint16_t* reg = GET_REG16(cpu->modrm.reg);
	uint16_t addr = modrm_get_address(cpu);
	*reg = addr;
	CYCLES(2);
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
	CYCLES_RM(3, 24);
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
	CYCLES_RM(3, 24);
}
static void mul_rm(I8086* cpu) {
	/* mul r/m (F6/F7, R/M reg = b100) b1111011W */
	if (W) {
		uint16_t* mul = modrm_get_ptr16(cpu);
		uint32_t r = AX * *mul;
		AX = ((r >> 16) & 0xFFFF);
		DX = (r & 0xFFFF);
		CYCLES_RM(118, 224);
	}
	else {
		uint8_t* mul = modrm_get_ptr8(cpu);
		uint8_t al = AL;
		AX = al * *mul;
		CYCLES_RM(70, 76);
	}
}
static void imul_rm(I8086* cpu) {
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
static void div_rm(I8086* cpu) {
	/* div r/m (F6/F7, R/M reg = b110) b1111011W */
	if (W) {
		uint16_t* divider = modrm_get_ptr16(cpu);
		uint32_t dividend = (AX << 16) | DX;
		AX = (dividend / *divider) & 0xFFFF; // quotient
		DX = dividend % *divider; // remainder
		CYCLES_RM(144, 150);
	}
	else {
		uint8_t* divider = modrm_get_ptr8(cpu);
		uint16_t dividend = AX;
		AH = (dividend / *divider) & 0xFF; // quotient
		AL = dividend % *divider; // remainder
		CYCLES_RM(80, 86);
	}
}
static void idiv_rm(I8086* cpu) {
	/* idiv r/m (F6/F7, R/M reg = b111) b1111011W */
	if (W) {
		int16_t* divider = (int16_t*)modrm_get_ptr16(cpu);
		int32_t dividend = (AX << 16) | DX;
		AX = (dividend / *divider) & 0xFFFF; // quotient
		DX = dividend % *divider; // remainder
		CYCLES_RM(128, 134);
	}
	else {
		int8_t* divider = (int8_t*)modrm_get_ptr8(cpu);
		int16_t dividend = AX;
		AL = (dividend / *divider) & 0xFF; // quotient
		AH = dividend % *divider; // remainder
		CYCLES_RM(80, 86);
	}
}

static int movs(I8086* cpu) {
	/* movs (A4/A5) b1010010W */

	/* Rep prefix check */
	if (F1) {
		if (CX == 0) {
			return I8086_DECODE_OK;
		}
		CX -= 1;
	}

	/* Do string operation */
	if (W) {
		uint16_t* src = GET_MEM_PTR(DATA_ADDR(SI));
		uint16_t* dest = GET_MEM_PTR(EXTRA_ADDR(DI));
		*dest = *src;
		CYCLES(26);
	}
	else {
		uint8_t* src = GET_MEM_PTR(DATA_ADDR(SI));
		uint8_t* dest = GET_MEM_PTR(EXTRA_ADDR(DI));
		*dest = *src;
		CYCLES(18);
	}

	/* Adjust si/di delta */
	if (DF) {
		SI -= (1 << W);
		DI -= (1 << W);
	}
	else {
		SI += (1 << W);
		DI += (1 << W);
	}

	/* Rep prefix check */
	if (F1) {
		//return I8086_DECODE_REQ_CYCLE;
		IP -= 2; /* Allow interrupts */
	}
	return I8086_DECODE_OK;
}
static int stos(I8086* cpu) {
	/* stos (AA/AB) b1010101W */

	/* Rep prefix check */
	if (F1) {
		if (CX == 0) {
			return I8086_DECODE_OK;
		}
		CX -= 1;
	}

	/* Do string operation */
	if (W) {
		WRITE_WORD(EXTRA_ADDR(DI), AX);
		CYCLES(15);
	}
	else {
		WRITE_BYTE(EXTRA_ADDR(DI), AL);
		CYCLES(11);
	}

	/* Adjust si/di delta */
	if (DF) {
		DI -= (1 << W);
	}
	else {
		DI += (1 << W);
	}

	/* Rep prefix check */
	if (F1) {
		//return I8086_DECODE_REQ_CYCLE;
		IP -= 2; /* Allow interrupts */
		}
	return I8086_DECODE_OK;
}
static int lods(I8086* cpu) {
	/* lods (AC/AD) b1010110W */

	/* Rep prefix check */
	if (F1) {
		if (CX == 0) {
			return I8086_DECODE_OK;
		}
		CX -= 1;
	}

	/* Do string operation */
	if (W) {
		AX = READ_WORD(DATA_ADDR(SI));
	}
	else {
		AL = READ_BYTE(DATA_ADDR(SI));		
	}
	CYCLES(16);

	/* Adjust si/di delta */
	if (DF) {
		SI -= (1 << W);
	}
	else {
		SI += (1 << W);
	}

	/* Rep prefix check */
	if (F1) {
		//return I8086_DECODE_REQ_CYCLE;
		IP -= 2; /* Allow interrupts */
	}
	return I8086_DECODE_OK;
}
static int cmps(I8086* cpu) {
	/* cmps (A6/A7) b1010011W */

	/* Rep prefix check */
	if (F1) {
		if (CX == 0) {
			return I8086_DECODE_OK;
		}
		CX -= 1;
	}

	/* Do string operation */
	if (W) {
		uint16_t* src = GET_MEM_PTR(DATA_ADDR(SI));
		uint16_t* dest = GET_MEM_PTR(EXTRA_ADDR(DI));
		alu_cmp16(cpu, *dest, *src);
	}
	else {
		uint8_t* src = GET_MEM_PTR(DATA_ADDR(SI));
		uint8_t* dest = GET_MEM_PTR(EXTRA_ADDR(DI));
		alu_cmp8(cpu, *dest, *src);
	}
	CYCLES(30);

	/* Adjust si/di delta */
	if (DF) {
		SI -= (1 << W);
		DI -= (1 << W);
	}
	else {
		SI += (1 << W);
		DI += (1 << W);
	}

	/* Rep prefix check */
	if (F1 && ZF == F1Z) {
		//return I8086_DECODE_REQ_CYCLE;
		IP -= 2; /* Allow interrupts */
	}
	return I8086_DECODE_OK;
}
static int scas(I8086* cpu) {
	/* scas (AE/AF) b1010111W */

	/* Rep prefix check */
	if (F1) {
		if (CX == 0) {
			return I8086_DECODE_OK;
		}
		CX -= 1;
	}

	/* Do string operation */
	if (W) {
		uint16_t* mem = GET_MEM_PTR(EXTRA_ADDR(DI));
		alu_cmp16(cpu, AX, *mem);
	}
	else {
		uint8_t* mem = GET_MEM_PTR(EXTRA_ADDR(DI));
		alu_cmp8(cpu, AL, *mem);
	}
	CYCLES(19);

	/* Adjust si/di delta */
	if (DF) {
		DI -= (1 << W);
	}
	else {
		DI += (1 << W);
	}

	/* Rep prefix check */
	if (F1 && ZF == F1Z) {
		//return I8086_DECODE_REQ_CYCLE;
		IP -= 2; /* Allow interrupts */
	}
	return I8086_DECODE_OK;
}

static void les(I8086* cpu) {
	/* les (C4) b11000100 */
	fetch_modrm(cpu);
	uint16_t* reg = GET_REG16(cpu->modrm.reg);
	uint20_t ea = modrm_get_effective_address(cpu);
	*reg = READ_WORD(ea);
	ES = READ_WORD(ea + 2);
	CYCLES(24);
}
static void lds(I8086* cpu) {
	/* lds (C5) b11000101 */
	fetch_modrm(cpu);
	uint16_t* reg = GET_REG16(cpu->modrm.reg);
	uint20_t ea = modrm_get_effective_address(cpu);
	*reg = READ_WORD(ea);
	DS = READ_WORD(ea + 2);
	CYCLES(24);
}

static void xlat(I8086* cpu) {
	/* Get data pointed by BX + AL (D7) b11010111 */
	uint8_t mem = READ_BYTE(DATA_ADDR(BX + AL));
	AL = mem;
	CYCLES(11);
}

static void esc(I8086* cpu) {
	/* esc (D8-DF R/M reg = XXX) b11010REG */
	fetch_modrm(cpu);
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
		CYCLES(19);
	}
	else {
		CYCLES(5);
	}
}
static void loopz(I8086* cpu) {
	/* loop while zero (E1) b1110000Z */
	int8_t disp = FETCH_BYTE();
	CX -= 1;
	if (CX && ZF) {
		IP += disp;
		CYCLES(18);
	}
	else {
		CYCLES(6);
	}
}
static void loop(I8086* cpu) {
	/* loop if CX not zero (E2) b11100010 */
	int8_t disp = FETCH_BYTE();
	CX -= 1;
	if (CX) {
		IP += disp;
		CYCLES(17);
	}
	else {
		CYCLES(5);
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
	CYCLES(14);
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
	CYCLES(14);
}
static void in_accum_dx(I8086* cpu) {
	/* in AL/AX, DX */
	if (W) {
		AX = READ_IO_WORD(DX);
	}
	else {
		AL = READ_IO(DX);
	}
	CYCLES(12);
}
static void out_accum_dx(I8086* cpu) {
	/* out DX, AL/AX */
	if (W) {
		WRITE_IO_WORD(DX, AX);
	}
	else {
		WRITE_IO(DX, AL);
	}
	CYCLES(12);
}

static void int_(I8086* cpu) {
	/* interrupt CD b11001101 */	
	uint8_t type = FETCH_BYTE();
	i8086_int(cpu, type);
		CYCLES(71);
	}
static void int3(I8086* cpu) {
	/* interrupt CC b11001100 */
	i8086_int(cpu, INT_3);
		CYCLES(72);
	}
static void into(I8086* cpu) {
	/* interrupt on overflow (CE) b11001110 */
	if (OF) {
		i8086_int(cpu, INT_OVERFLOW);
		CYCLES(73);
	}
	else {
		CYCLES(4);
	}
}
static void iret(I8086* cpu) {
	/* return from interrupt (CF) b11001111 */
	i8086_pop_word(cpu, &IP);
	i8086_pop_word(cpu, &CS);
	i8086_pop_word(cpu, &PSW);
	CYCLES(44);
}

/* prefix byte */
static int rep(I8086* cpu) {
	/* rep/repz/repnz (F2/F3) b1111001Z */
	cpu->internal_flags |= INTERNAL_FLAG_F1;    /* Set F1 */
	cpu->internal_flags &= ~INTERNAL_FLAG_F1Z;  /* Clr F1Z */
	cpu->internal_flags |= (cpu->opcode & 0x1); /* Set F1Z */
	
	cpu->opcode = FETCH_BYTE();
	CYCLES(9);
	return I8086_DECODE_REQ_CYCLE;
}
static int segment_override(I8086* cpu) {
	/* (26/2E/36/3E) b001SR110 */
	cpu->segment_prefix = SR;
	cpu->opcode = FETCH_BYTE();
	CYCLES(2);
	return I8086_DECODE_REQ_CYCLE;
}
static int lock(I8086* cpu) {
	/* lock the bus (F0) b11110000 */
	cpu->opcode = FETCH_BYTE();
	CYCLES(2);
	return I8086_DECODE_REQ_CYCLE;
}

static void i8086_check_interrupts(I8086* cpu) {
	if (NMI) {
		NMI = 0;
		i8086_int(cpu, INT_NMI);
	}
	else {
		uint8_t tf = TF;

		if (DBZ) {
			DBZ = 0;
			i8086_int(cpu, INT_DBZ);
		}

		if (tf) {
			i8086_int(cpu, INT_TRAP);
		}
		else if (IF && INTR) {
			INTR = 0;
			i8086_int(cpu, cpu->intr_type);
		}
	}
}

/* Fetch next opcode */
static void i8086_fetch(I8086* cpu) {
	cpu->internal_flags = 0;
	cpu->modrm.byte = 0;
	cpu->segment_prefix = 0xFF;
	cpu->opcode = FETCH_BYTE();
}

/* decode opcode */
static void i8086_decode_opcode_80(I8086* cpu) {
	/* 0x80 - 0x83 b100000SW (Immed) */
	fetch_modrm(cpu);
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
static int i8086_decode_opcode_d0(I8086* cpu) {
	/* 0xD0 - 0xD3 b110100VW (Shift) */
	fetch_modrm(cpu);
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
		case 0b110:
			return I8086_DECODE_UNDEFINED;
		case 0b111:
			sar(cpu);
			break;
	}
	return I8086_DECODE_OK;
}
static int i8086_decode_opcode_f6(I8086* cpu) {
	/* F6/F7 b1111011W (Group 1) */
	fetch_modrm(cpu);
	switch (cpu->modrm.reg) {
		case 0b000:
			test_rm_imm(cpu);
			break;
		case 0b001:
			return I8086_DECODE_UNDEFINED;
		case 0b010:
			not(cpu);
			break;
		case 0b011:
			neg(cpu);
			break;
		case 0b100:
			mul_rm(cpu);
			break;
		case 0b101:
			imul_rm(cpu);
			break;
		case 0b110:
			div_rm(cpu);
			break;
		case 0b111:
			idiv_rm(cpu);
			break;
	}
	return I8086_DECODE_OK;
}
static int i8086_decode_opcode_fe(I8086* cpu) {
	/* FE/FF b1111111W (Group 2) */
	fetch_modrm(cpu);
	if (W) {
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
			
			case 0b111:
				return I8086_DECODE_UNDEFINED;
		}
	}
	else {
		switch (cpu->modrm.reg) {
			case 0b000:
				inc_rm(cpu);
				break;
			case 0b001:
				dec_rm(cpu);
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
			int3(cpu);
			break;
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
			return i8086_decode_opcode_d0(cpu);
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
			return i8086_decode_opcode_f6(cpu);
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
			return i8086_decode_opcode_fe(cpu);
		default:
			return I8086_DECODE_UNDEFINED;
	}
	return I8086_DECODE_OK;
}

static int i8086_decode_instruction(I8086* cpu) {
	int r = 0;
	do {
		r = i8086_decode_opcode(cpu);
	} while (r == I8086_DECODE_REQ_CYCLE);
	return r;
}

void i8086_init(I8086* cpu) {
	cpu->funcs.read_mem_byte = NULL;
	cpu->funcs.write_mem_byte = NULL;
	cpu->funcs.read_mem_word = NULL;
	cpu->funcs.write_mem_word = NULL;
	cpu->funcs.get_mem_ptr = NULL;
	cpu->funcs.read_io_byte = NULL;
	cpu->funcs.write_io_byte = NULL;
	cpu->funcs.read_io_word = NULL;
	cpu->funcs.write_io_word = NULL;
}
void i8086_reset(I8086* cpu) {
	for (int i = 0; i < I8086_REGISTER_COUNT; ++i) {
		cpu->registers[i].r16 = 0;
	}

	for (int i = 0; i < I8086_SEGMENT_COUNT; ++i) {
		cpu->segments[i] = 0;
	}

	IP = 0;
	CS = 0xFFFF;

	PSW = 0;
	cpu->opcode = 0;
	cpu->modrm.byte = 0;
	cpu->cycles = 0;
	cpu->internal_flags = 0;
	cpu->dbz = 0;
	cpu->segment_prefix = 0;

	cpu->pins.intr = 0;
	cpu->pins.mode = 0;
	cpu->pins.nmi = 0;
	cpu->pins.test = 0;
}

int i8086_execute(I8086* cpu) {
	i8086_check_interrupts(cpu);
	i8086_fetch(cpu);
	return i8086_decode_instruction(cpu);
}
