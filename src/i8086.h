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
#define GET_SEG(seg) ((cpu->segment_prefix != 0xFF) ? cpu->segment_prefix : seg)

/* Get 20bit address SEG:ADDR */
#define GET_ADDR(seg, addr) (((cpu->segments[seg] << 4) + addr) & 0xFFFFF)

/* Get 20bit code segment address CS:ADDR */
#define IP_ADDR            GET_ADDR(SEG_CS, IP)

/* Get 20bit stack segment address SS:ADDR */
#define STACK_ADDR(addr)   GET_ADDR(GET_SEG(SEG_SS), addr)

/* Get 20bit data segment address DS:ADDR */
#define DATA_ADDR(addr)    GET_ADDR(GET_SEG(SEG_DS), addr)

/* Get 20bit extra segment address ES:ADDR */
#define EXTRA_ADDR(addr)   GET_ADDR(GET_SEG(SEG_ES), addr)

/* Read byte from [addr] */
#define READ_BYTE(addr)        cpu->mm.funcs.read_mem_byte(cpu->mm.mem, addr)

/* Write byte to [addr] */
#define WRITE_BYTE(addr,value) cpu->mm.funcs.write_mem_byte(cpu->mm.mem, addr,value)

/* Fetch byte at CS:IP. Inc IP by 1 */
#define FETCH_BYTE()           cpu->mm.funcs.read_mem_byte(cpu->mm.mem, IP_ADDR); IP += 1

/* read word from [addr] */
#define READ_WORD(addr)        cpu->mm.funcs.read_mem_word(cpu->mm.mem, addr)

/* write word to [addr] */
#define WRITE_WORD(addr,value) cpu->mm.funcs.write_mem_word(cpu->mm.mem, addr,value)

/* Fetch word at CS:IP. Inc IP by 2 */
#define FETCH_WORD()           cpu->mm.funcs.read_mem_word(cpu->mm.mem, IP_ADDR); IP += 2

/* Read byte from IO port word */
#define READ_IO_WORD(port)     cpu->mm.funcs.read_io_word(port)

/* Read byte from IO port */
#define READ_IO(port)          cpu->mm.funcs.read_io_byte(port)

/* Write byte to IO port */
#define WRITE_IO_WORD(port,value) cpu->mm.funcs.write_io_word(port, value)

/* Write byte to IO port */
#define WRITE_IO(port,value)   cpu->mm.funcs.write_io_byte(port, value)

/* Get memory pointer */
#define GET_MEM_PTR(addr)      cpu->mm.funcs.get_mem_ptr(cpu->mm.mem, addr)

/* Get 8bit register ptr */
#define GET_REG8(reg)          (&cpu->registers[reg & 3].l + ((reg & 7) >> 2))

/* Get 16bit register ptr */
#define GET_REG16(reg)         (&cpu->registers[reg & 7].r16)

#define REGISTER_COUNT 8
#define SEGMENT_COUNT  4

#define DECODE_OK        0
#define DECODE_REQ_CYCLE 1
#define DECODE_UNDEFINED 2

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
	uint8_t(*read_mem_byte)(uint8_t*, uint20_t);         // write mem byte
	void(*write_mem_byte)(uint8_t*, uint20_t, uint8_t);  // write mem byte

	uint16_t(*read_mem_word)(uint8_t*, uint20_t);        // read mem word
	void(*write_mem_word)(uint8_t*, uint20_t, uint16_t); // write mem word

	void* (*get_mem_ptr)(uint8_t*, uint20_t);            // Get mem pointer

	uint8_t(*read_io_byte)(uint16_t);          // read io byte
	void(*write_io_byte)(uint16_t, uint8_t);   // write io byte

	uint16_t(*read_io_word)(uint16_t);         // read io byte
	void(*write_io_word)(uint16_t, uint16_t);  // write io byte	
} I8086_FUNCS;

/* I8086 Memory */
typedef struct I8086_MM {
	uint8_t* mem;      // cpu memory pointer
	I8086_FUNCS funcs; // cpu memory functions
} I8086_MM;

/* I8086 CPU State */
typedef struct I8086 {
	uint16_t ip;
	I8086_REG16 registers[REGISTER_COUNT];
	uint16_t segments[SEGMENT_COUNT];
	I8086_PROGRAM_STATUS_WORD status;

	uint8_t opcode;
	I8086_MOD_RM modrm;
	uint8_t segment_prefix;
	uint8_t rep_prefix;
	uint8_t test_line; // pin 23

	I8086_MM mm;
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
