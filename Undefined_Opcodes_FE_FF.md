## 8086 Undefined Opcodes FE/FF
 This document covers the undefined instructions in the Group 2 opcodes; FE and FF.
 The instructions are variations of `CALL`, `JMP`, and `PUSH` with register or memory operands.
 Their behaviour depends on operand size (byte vs word), addressing mode (register vs memory), 
 and segment overrides. All these findings are based on the V2 JSON undefined tests that are 
 hardware generated for the 8086/8088 CPU.

### FE.2 - CALL NEAR byte R/M
 If the operand is a register (mod = b11), IP is set to the register pair but reversed. In other 
 words, the instruction interprets the 8-bit register as part of a 16-bit register pair and 
 reverses the bytes. EG: `BX=D8E1` becomes `IP=E1D8`. The SP is decremented by 2 even though 
 only a single byte is written.
 ```c
/* Get the 8-bit register selected by the modrm byte. */
op8_t rm = modrm_get_op8(cpu);

/* Bit3 of the rm field selects the low or high 8-bit register. */
cpu->modrm.rm ^= 0x4;

/* Get the other 8-bit register in the register pair selected by the modrm byte. */
op8_t rm2 = modrm_get_op8(cpu);

/* Read the first 8-bit register into the low byte. */
ip = op8_read(cpu, rm);

/* Read the second 8-bit register into the high byte. */
ip |= ((uint16_t)op8_read(cpu, rm2) << 8);

/* Push return address; Only the low byte is pushed to the stack. */
push_byte(cpu, IP & 0xFF);

/* Set jump address */
IP = ip;
 ```
 
 If the operand is memory (mod != b11), IP is set to the value of the memory byte with the high 
 byte forced to 0xFF. The SP is decremented by 2 even though only a single byte is written.
```c
/* Get the 8-bit memory address selected by the modrm byte. */
op8_t rm = modrm_get_op8(cpu);

/* Read the memory value into the low byte. Set the high byte to 0xFF. */
ip = 0xFF00 | op8_read(cpu, rm);

/* Push return address; Only the low byte is pushed to the stack. */
push_byte(cpu, IP & 0xFF);

/* Set jump address */
IP = ip;
```

 ### FE.3 - CALL FAR byte R/M
  This instruction cannot be traditionally emulated when register operands are specified. Due to 
  assumptions made by the microcode, this routine uses the internal register tmpb which is
  stale/uninitialized.

  If the operand is memory (mod != b11), The low byte of IP is set to the memory byte, respecting 
  any segment override in use. The high byte of IP is set to 0xFF. The low byte of CS is set to 
  the memory byte, ignoring any segment override, but still respecting default segments 
  (SS for BP variants). The high byte of CS is set to 0xFF. The SP is decremented by 2 per push 
  even though only a single byte is written.
```c
/* Get Mod R/M offset; fetch displacement (if applicable); incrementing IP. */
uint16_t offset = modrm_get_offset(cpu);

/* IP is read into the low byte. Set the high byte to 0xFF. */
uint16_t segment = modrm_get_segment(cpu);
ip = 0xFF00 | read_byte(cpu, segment, offset);

/* CS is read disregarding any segment override. Set the high byte to FF. */
cpu->segment_prefix = 0xFF;
segment = modrm_get_segment(cpu);
cs = 0xFF00 | read_byte(cpu, segment, offset);

/* Push return address; Only the low byte of IP,CS are pushed to the stack. */
push_byte(cpu, CS & 0xFF);
push_byte(cpu, IP & 0xFF);

/* Set jump address */
CS = cs;
IP = ip;
```

 ### FE.4 - JMP NEAR byte R/M
  Behaves like FE.2. (without the push)
  
 ### FE.5 - JMP FAR byte R/M
  Behaves like FE.3. (without the push)
 
 ### FE.6/FE.7 - PUSH byte R/M
  Both registers and memory behave the same; The 8-bit value is written to the stack. The SP is 
  decremented by 2 even though only a single byte is written.
```c
/* Get the memory or register. */
op8_t op8 = modrm_get_op8(cpu);

/* Read the 8-bit memory or register. */
uint8_t tmp = op8_read(cpu, op8);

/* Write the 8-bit value to the stack. */
push_byte(cpu, tmp);
```
 ### FF.3 - CALL FAR word R/M
  This instruction cannot be traditionally emulated when register operands are specified. Due to 
  assumptions made by the microcode, this routine uses the internal register tmpb which is 
  stale/uninitialized. If the operand is memory then this is the normal, defined operation of 
  the instruction.

 ### FF.5 - JMP FAR word R/M
  Behaves like FF.3. (without the push)
