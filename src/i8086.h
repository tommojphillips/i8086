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
#define I8086_JCC_JO  0b0000
#define I8086_JCC_JNO 0b0001
#define I8086_JCC_JC  0b0010
#define I8086_JCC_JNC 0b0011
#define I8086_JCC_JZ  0b0100
#define I8086_JCC_JNZ 0b0101
#define I8086_JCC_JBE 0b0110
#define I8086_JCC_JA  0b0111
#define I8086_JCC_JS  0b1000
#define I8086_JCC_JNS 0b1001
#define I8086_JCC_JPE 0b1010
#define I8086_JCC_JPO 0b1011
#define I8086_JCC_JL  0b1100
#define I8086_JCC_JGE 0b1101
#define I8086_JCC_JLE 0b1110
#define I8086_JCC_JG  0b1111

/* 20bit address */
typedef uint32_t uint20_t;

#pragma warning( push )
/* ignore unnamed_structure warning */
#pragma warning( disable : 4201)

/* 16bit Program Status Word */
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

typedef struct I8086_PINS {
	uint8_t nmi;  // pin17 - non-maskable interrupt
	uint8_t inr;  // pin18 - interrupt request
	uint8_t test; // pin23 - test
	uint8_t lock; // pin29 - lock the bus
	uint8_t mode; // pin33 - minimum/maximum mode
} I8086_PINS;

/* I8086 CPU State */
typedef struct I8086 {
	uint16_t ip;                                 // instruction pointer
	I8086_REG16 registers[I8086_REGISTER_COUNT]; // general registers
	uint16_t segments[I8086_SEGMENT_COUNT];      // segment registers
	I8086_PROGRAM_STATUS_WORD status;            // program status word

	uint8_t opcode;                              // current opcode
	I8086_MOD_RM modrm;                          // current mod r/m byte (if applicable)
	uint8_t segment_prefix;                      // current segment override prefix byte (if applicable)
	uint8_t rep_prefix;                          // current rep prefix byte (if applicable)
	
	uint64_t cycles;

	I8086_PINS pins;                             // cpu pins
	I8086_FUNCS funcs;                           // cpu memory function pointers
} I8086;

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the CPU. 
	Sets all function pointers to NULL
	cpu: the cpu instance */
void i8086_init(I8086* cpu);

/* Reset The CPU to it's reset state.
	cpu: the cpu instance */
void i8086_reset(I8086* cpu);

/* Fetch, Execute the next instruction
	cpu: the cpu instance */
int i8086_execute(I8086* cpu);

#ifdef __cplusplus
};
#endif
#endif
