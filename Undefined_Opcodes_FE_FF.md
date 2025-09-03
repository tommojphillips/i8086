## 8086 Undefined Opcodes FE/FF - The rules
 This document covers the undefined instructions in the Group 2 opcodes; FE and FF.
 The instructions are variations of CALL, JMP, and PUSH with register or memory operands.
 Their behaviour depends on operand size (byte vs word), addressing mode (register vs memory), and segment overrides. All these findings are based on the V2 JSON undefined tests that are hardware generated for the 8086/8088 CPU.

### FE.2 - CALL NEAR byte R/M
 If the operand is a register (mod = b11), IP is set to the register pair but reversed. In other words, the instruction interprets the 8-bit register as part of a 16-bit register pair and reverses the bytes. EG: `BX=D8E1` becomes `IP=E1D8`.
 ```c
/* Get the 8-bit register selected by the rm field. */
op8_t rm = modrm_get_op8(cpu);

/* Bit3 of the rm field determines if the LOW or the HIGH 8-bit register is selected. */
cpu->modrm.rm ^= 0x4;

/* Get the other 8-bit register in the register pair selected by the rm field. */
op8_t rm2 = modrm_get_op8(cpu);

/* Read the first 8-bit register as the low byte of IP. */
ip = op8_read(cpu, rm);

/* Read the second 8-bit register as the high byte of IP. */
ip |= ((uint16_t)op8_read(cpu, rm2) << 8);

/* Push return address */
push_word(cpu, IP);

/* Set jump address */
IP = ip;
 ```
 
 If the operand is memory (mod != b11), IP is set to the value of the memory byte with the high byte forced to 0xFF. Segment overrides are respected for memory operands.
```c
/* Get the 8-bit memory address directed by the mod field and rm field. */
op8_t rm = modrm_get_op8(cpu);

/* Read the memory value as the low byte of IP. Set the high byte of IP to 0xFF. */
ip = 0xFF00 | op8_read(cpu, rm);

/* Push return address */
push_word(cpu, IP);

/* Set jump address */
IP = ip;
```

 ### FE.3 - CALL FAR byte R/M
  If the operand is a register (mod = b11), IP is set to IP minus the instruction length minus 4 (`OLD_IP - 4`). CS is read from the address `[DS + 4]`, ignoring any segment override and default segment. Only the low byte of CS and IP are pushed to the stack.
 ```c
/* IP is set to OLD_IP - 4. */
ip = IP - cpu->instruction_len - 4;

/* Read the memory value at [DS+4] (disregarding segement overrides) as the low byte of CS. 
    Set the high byte of CS to 0xFF. */
cs = 0xFF00 | read_byte(cpu, cpu->segments[SEG_DS], 4);

/* Push return address */
push_word(cpu, CS & 0xFF);
push_word(cpu, IP & 0xFF);

/* Set jump address */
CS = cs;
IP = ip;
 ```

  If the operand is memory (mod != b11), The low byte of IP is set to the memory byte, respecting any segment override in use. The high byte of IP is set to 0xFF. The low byte of CS is set to the memory byte, ignoring any segment override, but still respecting default segments (SS for BP variants). The high byte of CS is set to 0xFF.
```c
/* Get Mod R/M offset; fetch displacement (if applicable); incrementing IP. */
uint16_t offset = modrm_get_offset(cpu);

/* Read the memory value (respecting segment overrides and defaults) as the low byte of IP. 
    Set the high byte of IP to 0xFF. */
uint16_t segment = modrm_get_segment(cpu);
ip = 0xFF00 | read_byte(cpu, segment, offset);

/* CS is read disregarding segment override. Set the hi byte to FF. */
cpu->segment_prefix = 0xFF;
segment = modrm_get_segment(cpu);
cs = 0xFF00 | read_byte(cpu, segment, offset);

/* Push return address */
push_word(cpu, CS);
push_word(cpu, IP);

/* Set jump address */
CS = cs;
IP = ip;
```

 ### FE.4 - JMP NEAR byte R/M
  Behaves like FE.2, without pushing the return address to the stack.
  
 ### FE.5 - JMP FAR byte R/M
  Behaves like FE.3, without pushing the return address to the stack.
 
 ### FE.6/FE.7 - PUSH byte R/M
  SP is decremented by two before reading the operand, ensuring that if SP itself is the operand, the new SP is used. Both registers and memory behave the same; The 8-bit value is extended to a word with the high byte set to 0xFF and written to the stack.
```c
/* Decrement SP prior to reading the operand. */
SP -= 2;

/* Get the memory or register. */
op8_t op8 = modrm_get_op8(cpu);

/* Read the 8-bit memory or register into the low byte. Set the high byte to 0xFF */
uint16_t tmp = 0xFF00 | op8_read(cpu, op8);

/* Write the 16-bit word to the stack. */
write_word(cpu, SS, SP, tmp);
```
 ### FF.3 - CALL FAR word R/M
  If the operand is a register (mod = b11), IP is set to the old IP minus four (`OLD_IP - 4`). CS is read from `[SEG + 4]`, ignoring default segments (SS for BP variants), but still respecting any segment overrides ... defaults to DS if no segment override is used. Both IP and CS are pushed as 16-bit words.
```c
/* IP is set to OLD_IP - 4. */
ip = IP - cpu->instruction_len - 4;

/* CS is read respecting segment override [SR + 4].	defaults to DS. */
if (cpu->segment_prefix == 0xFF) {
	cpu->segment_prefix = SEG_DS;
}
uint16_t segment = modrm_get_segment(cpu);
cs = read_word(cpu, segment, 4);

/* Push return address. */
push_word(cpu, CS);
push_word(cpu, IP);

/* Set jump address. */
CS = cs;
IP = ip;
 ```

  If the operand is memory (mod != b11), IP and CS are read from memory. Both are pushed as 16-bit words. Note this is the normal, defined behaviour of CALL FAR word R/M
 ```c
/* Get the segment and offset from the modrm byte. */
uint16_t segment = modrm_get_segment(cpu);
uint16_t offset = modrm_get_offset(cpu);

/* Read the new IP and CS from memory. */
ip = read_word(cpu, segment, offset);
cs = read_word(cpu, segment, offset + 2);

/* Push return address. */
push_word(cpu, CS);
push_word(cpu, IP);

/* Set jump address. */
CS = cs;
IP = ip;
 ```
 ### FF.5 - JMP FAR word R/M
  Behaves like FF.3, without pushing the return address to the stack.

  ### FF.6/FF.7 - PUSH word R/M
  SP is decrement by 2 before reading the operand, ensuring that if SP itself is the operand, the new SP is used. Both registers and memory behave the same; The 16-bit word is written to the stack. Note this is the normal, defined behaviour of PUSH word R/M
```c
/* Decrement SP prior to reading the operand. */
SP -= 2;

/* Get the memory or register. */
op16_t op16 = modrm_get_op16(cpu);

/* Read the 16-bit memory or register. */
uint16_t tmp = op16_read(cpu, op16);

/* Write the 16-bit word to the stack. */
write_word(cpu, SS, SP, tmp);
```

## Notes

The JSON v2 undefined tests only record the low bytes that are written to the stack. So i dont really know what it actually written to the hi byte of the word in the stack. The tests do record the high byte that is written to IP,CS in CALL, JMP. which is 0xFF so i have made assumptions that 0xFF is written as the high byte of the stack values. This may be incorrect. The stack is decremented by 2, that is verified by the tests. It's just that the tests haven't recorded the hi byte. As soon as the tests are updated to include the hi bytes, i will update this document. Opcodes that are affected by this error are:

| Opcode | Instruction        |
| ------ | ------------------ |
| `FE.2` | CALL near byte R/M |
| `FE.3` | CALL far byte R/M  |
| `FE.6` | PUSH byte R/M      |
| `FE.7` | PUSH byte R/M      |
