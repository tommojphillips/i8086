/* main.c
 * tommojphillips 2025 ( https://github.com/tommojphillips/ )
 * Intel 8086 CPU test main
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "i8086.h"
#include "i8086_mnem.h"

#define MEM_SIZE 0x100000
#define BLDR_OFFSET 0x200

void* get_mem_ptr(uint8_t* mem, uint20_t addr) {
	return &mem[addr];
}

uint8_t read_mem_byte(uint8_t* mem, uint20_t addr) {
	return mem[addr];
}
void write_mem_byte(uint8_t* mem, uint20_t addr, uint8_t value) {
	mem[addr] = value;
}

uint16_t read_mem_word(uint8_t* mem, uint20_t addr) {
	return *(uint16_t*)&mem[addr];
}
void write_mem_word(uint8_t* mem, uint20_t addr, uint16_t value) {
	*(uint16_t*)&mem[addr] = value;
}

void rpad(char* buffer, const int buffer_size, const char pad) {
	size_t len = strlen(buffer);
	if (len < buffer_size) {
		for (int i = 0; i < buffer_size; ++i) {
			buffer[len + i] = pad;
		}
		buffer[buffer_size - 1] = '\0';
	}
}

void print_mnem(I8086* cpu, I8086_MNEM* mnem) {
	char buffer[64] = { 0 };

	i8086_mnem(mnem);

	rpad(mnem->mnem, 25, ' ');
	printf("%04X.%04X: %s", CS, IP, mnem->mnem);

	for (int i = cpu->ip; i < mnem->counter; ++i) {
		size_t len = strlen(buffer);
		if (len < 64) {
			snprintf(buffer + len, 64 - len, "%02X ", cpu->mm.mem[GET_ADDR(SEG_CS, i)]);
		}
	}
	rpad(buffer, 20, ' ');
	printf(buffer);
}

void print_state(I8086* cpu) {

	printf("AX %04X BX %04X CX %04X DX %04X SI %04X DI %04X ES %04X DS %04X SS %04X ",
		AX, BX, CX, DX, SI, DI, ES, DS, SS);

	if (CF) printf("C"); else printf(" ");
	if (PF) printf("P"); else printf(" ");
	if (AF) printf("A"); else printf(" ");
	if (ZF) printf("Z"); else printf(" ");
	if (SF) printf("S"); else printf(" ");
	if (TF) printf("T"); else printf(" ");
	if (IF) printf("I"); else printf(" ");
	if (DF) printf("D"); else printf(" ");
	if (OF) printf("O"); else printf(" ");
	printf("\n");
}
int main(int argc, char** argv) {
	(void)argc;
	(void)argv;

	I8086* cpu = NULL;
	uint8_t* mem = NULL;

	cpu = (I8086*)malloc(sizeof(I8086));
	if (cpu == NULL) {
		printf("Error: failed to allocate cpu struct\n");
		exit(1);
	}
	memset(cpu, 0, sizeof(I8086));

	mem = (uint8_t*)malloc(0x100000);
	if (mem == NULL) {
		printf("Error: failed to allocate cpu memory\n");
		exit(1);
	}
	memset(mem, 0, 0x100000);

	i8086_init(cpu);
	i8086_reset(cpu);

	/* setup cpu pointers */
	cpu->mm.mem = mem;
	cpu->mm.funcs.get_mem_ptr = get_mem_ptr;
	cpu->mm.funcs.read_mem_byte = read_mem_byte;
	cpu->mm.funcs.write_mem_byte = write_mem_byte;
	cpu->mm.funcs.read_mem_word = read_mem_word;
	cpu->mm.funcs.write_mem_word = write_mem_word;

	I8086_MNEM mnem = { 0 };
	mnem.state = cpu;

	FILE* file = NULL;
	fopen_s(&file, "bldr.bin", "rb");
	if (file != NULL) {
		size_t bytes_read = fread_s(mem + 0x8000, BLDR_OFFSET, 1, BLDR_OFFSET, file);
		printf("Read %zu bytes into memory at %04X from bldr.bin\n", bytes_read, 0x8000);
		fclose(file);
	}
	else {
		printf("Error: bldr.bin not found\n");
	}

	/* Load JMP FAR 0800:0000 at FFFF:0000 */	
	WRITE_BYTE(0xFFFF0, 0xEA);
	WRITE_WORD(0xFFFF3, 0x0800);
	WRITE_WORD(0xFFFF1, 0x0000);

	while (1) {
		print_mnem(cpu, &mnem);
		if (i8086_execute(cpu) == DECODE_UNDEFINED) {
			printf("error: undefined opcode ");
		}
		print_state(cpu);
	}

	if (mem != NULL) {
		free(mem);
		mem = NULL;
	}
	if (cpu != NULL) {
		free(cpu);
		cpu = NULL;
	}
}