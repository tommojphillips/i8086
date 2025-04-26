/* I8086.h
 * Thomas J. Armytage 2025 ( https://github.com/tommojphillips/ )
 * Intel 8086 CPU
 */

#ifndef I8086_H
#define I8086_H

#include <stdint.h>

#define REG_AL 0
#define REG_CL 1
#define REG_DL 2
#define REG_BL 3
#define REG_AH 4
#define REG_CH 5
#define REG_DH 6
#define REG_BH 7

#define REG_AX 0
#define REG_CX 1
#define REG_DX 2
#define REG_BX 3
#define REG_SP 4
#define REG_BP 5
#define REG_SI 6
#define REG_DI 7

#define SEG_ES 0
#define SEG_CS 1
#define SEG_SS 2
#define SEG_DS 3

#define I8086_REGISTER_COUNT 8
#define I8086_SEGMENT_COUNT  4

#define I8086_DECODE_OK        0
#define I8086_DECODE_REQ_CYCLE 1
#define I8086_DECODE_UNDEFINED 2

 /* Jump condition */
enum {
	X86_JCC_JO = 0b0000,
	X86_JCC_JNO = 0b0001,
	X86_JCC_JC = 0b0010,
	X86_JCC_JNC = 0b0011,
	X86_JCC_JZ = 0b0100,
	X86_JCC_JNZ = 0b0101,
	X86_JCC_JBE = 0b0110,
	X86_JCC_JA = 0b0111,
	X86_JCC_JS = 0b1000,
	X86_JCC_JNS = 0b1001,
	X86_JCC_JPE = 0b1010,
	X86_JCC_JPO = 0b1011,
	X86_JCC_JL = 0b1100,
	X86_JCC_JGE = 0b1101,
	X86_JCC_JLE = 0b1110,
	X86_JCC_JG = 0b1111,
};

/* 20bit address */
typedef uint32_t uint20_t;

#pragma warning( push )
/* ignore unnamed_structure warning */
#pragma warning( disable : 4201)

#define PSW_BIT_STRUCT

/* 16bit Program Status Word */
#ifdef PSW_BIT_STRUCT
typedef struct I8086_PROGRAM_STATUS_WORD {
	union {
		uint16_t word;
		struct {
			uint8_t cf : 1; // carry flag
			uint8_t r0 : 1; // rev 0
			uint8_t pf : 1; // parity flag
			uint8_t r1 : 1; // rev 1
			uint8_t af : 1; // aux carry flag
			uint8_t r2 : 1; // rev 2
			uint8_t zf : 1; // zero flag
			uint8_t sf : 1; // sign flag
			uint8_t tf : 1; // trap flag
			uint8_t in : 1; // interrupt flag
			uint8_t df : 1; // direction flag
			uint8_t of : 1; // overflow flag
			uint8_t r3 : 1; // rev 3
			uint8_t r4 : 1; // rev 4
			uint8_t r5 : 1; // rev 5
			uint8_t r6 : 1; // rev 6
		};
	};
} I8086_PROGRAM_STATUS_WORD;
#else
typedef uint16_t I8086_PROGRAM_STATUS_WORD;
#endif

/* 16bit register */
typedef struct I8086_REG16 {
	union {
		uint16_t r16;
		struct {
			uint8_t l;
			uint8_t h;
		};
	};
} I8086_REG16;

/* 8bit Mod R/M byte */
typedef struct I8086_MOD_RM {
	union {
		uint8_t byte;
		struct {
			uint8_t rm  : 3; // r/m
			uint8_t reg : 3; // register
			uint8_t mod : 2; // mode
		};
	};
} I8086_MOD_RM;

#pragma warning( pop ) 

/* I8086 Function pointers */
typedef struct I8086_FUNCS {	
	void* (*get_mem_ptr)(uint20_t); // Get mem pointer

	uint8_t(*read_mem_byte)(uint20_t);  // read mem byte
	uint16_t(*read_mem_word)(uint20_t); // read mem word
	uint8_t(*read_io_byte)(uint16_t);   // read io byte
	uint16_t(*read_io_word)(uint16_t);  // read io byte

	void(*write_mem_byte)(uint20_t, uint8_t);  // write mem byte
	void(*write_mem_word)(uint20_t, uint16_t); // write mem word
	void(*write_io_byte)(uint16_t, uint8_t);   // write io byte
	void(*write_io_word)(uint16_t, uint16_t);  // write io byte	

} I8086_FUNCS;

/* I8086 CPU State */
typedef struct I8086 {
	uint16_t ip;
	I8086_REG16 registers[I8086_REGISTER_COUNT];
	uint16_t segments[I8086_SEGMENT_COUNT];
	I8086_PROGRAM_STATUS_WORD status;

	uint8_t opcode;
	I8086_MOD_RM modrm;
	uint8_t segment_prefix;
	uint8_t rep_prefix;
	uint8_t test_line; // pin 23
	I8086_FUNCS funcs; // cpu memory functions
} I8086;

#ifdef __cplusplus
extern "C" {
#endif

void i8086_init(I8086* cpu);
void i8086_reset(I8086* cpu);
int i8086_execute(I8086* cpu);

#ifdef __cplusplus
};
#endif
#endif
