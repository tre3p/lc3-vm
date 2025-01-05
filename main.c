#include <stdint.h>
#include <stdio.h>
#include "registers.h"
#include "opcodes.h"
#include "cond_flags.h"
#include "utils.c"

#define MEMORY_MAX (1 << 16)
#define DEFAULT_PC_POS 0x3000

uint16_t mem[MEMORY_MAX]; // 128 KB
uint16_t reg[R_COUNT];

void update_cond_flags(uint16_t);

int main(int argc, const char* argv[]) {
  if (argc < 2) {
    printf("lc3 [image-file1] ...\n");
    return 2;
  }

  reg[R_COND] = FL_ZRO;
  reg[R_PC] = DEFAULT_PC_POS;

  int running = 1;
  while(running) {
    uint16_t instr = mem_read(reg[R_PC]++);
    uint16_t op = instr >> 12;

    switch (op) {
      case OP_ADD: {
        uint16_t dr = (instr >> 9) & 0x7;
        uint16_t r0 = (instr >> 6) & 0x7;

        if ((instr >> 5) & 0x1) { /* if immediate flag is set */
          uint16_t imm_val = sign_extend(instr & 0x1F, 5);
          reg[dr] = reg[r0] + imm_val;
        } else {
          uint16_t r1 = instr & 0x7;
          reg[dr] = reg[r0] + reg[r1];
        }

        update_cond_flags(dr);
        break;
      }
      case OP_AND: {
        uint16_t dr = (instr >> 9) & 0x7;
        uint16_t r0 = (instr >> 6) & 0x7;

        if ((instr >> 5) & 0x1) {
          reg[dr] = r0 & sign_extend(instr & 0x1F, 5);
        } else {
          reg[dr] = r0 & reg[instr & 0x7];
        }

        update_cond_flags(dr);
        break;
      }
      case OP_NOT: {
        uint16_t dr = (instr >> 9) & 0x7;
        uint16_t sr = (instr >> 6) & 0x7;
        reg[dr] = ~reg[sr];

        update_cond_flags(dr);
        break;
      }
      case OP_BR: {
        uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
        uint16_t cond_flags = (instr >> 9) & 0x7;

        if (cond_flags & reg[R_COND]) {
          reg[R_PC] += pc_offset;
        }

        break;
      }
      case OP_LDI: {
        uint16_t dr = (instr >> 9) & 0x7;
        uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
        reg[dr] = mem_read(mem_read(reg[R_PC] + pc_offset));

        update_cond_flags(dr);
        break;
      }
      // etc
      default:
        // BAD OPCode
        break;
    }
  }
  return 0;
}

/**
 * When value is written to the register - condition registers should be updated based on value written.
 */
void update_cond_flags(uint16_t r_num) {
  if (reg[r_num] == 0) {
    reg[R_COND] = FL_ZRO;
  } else if (reg[r_num] >> 15) { /* since register is 16-bit - MSB is sign-bit */
    reg[R_COND] = FL_NEG;
  } else {
    reg[R_COND] = FL_POS;
  }
}