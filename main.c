#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/termios.h>
#include <sys/mman.h>
#include "registers.h"
#include <signal.h>
#include "mm_registers.h"
#include "opcodes.h"
#include "trapcodes.h"
#include "cond_flags.h"
#include "utils.h"

#define MEMORY_MAX (1 << 16)
#define DEFAULT_PC_POS 0x3000

uint16_t memory[MEMORY_MAX]; // 128 KB
uint16_t reg[R_COUNT];

void update_cond_flags(uint16_t);
void mem_write(uint16_t, uint16_t);
uint16_t mem_read(uint16_t);
int read_image(const char*);

/* OS specific functions start */
uint16_t check_key();
void restore_input_buffering();
void disable_input_buffering();
void handle_interrupt(int);
/* OS specific functions end */

int main(int argc, const char* argv[]) {
  if (argc < 2) {
    printf("Usage: lc3 [image-file1] ...\n");
    return 2;
  }
  for (int j = 1; j < argc; ++j) {
    if (!read_image(argv[j])) {
      printf("failed to load image: %s\n", argv[j]);
      exit(1);
    }
  }

  signal(SIGINT, handle_interrupt);
  disable_input_buffering();

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
          reg[dr] = reg[r0] & sign_extend(instr & 0x1F, 5);
        } else {
          reg[dr] = reg[r0] & reg[instr & 0x7];
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
      case OP_JMP: {
        uint16_t dest = (instr >> 6) & 0x7;
        reg[R_PC] = reg[dest];

        break;
      }
      case OP_JSR: {
        reg[R_R7] = reg[R_PC];
        uint16_t cond = (instr >> 11) & 0x1;

        if (cond) {
          reg[R_PC] += sign_extend((instr & 0x7FF), 11);
        } else {
          uint16_t base_r = (instr >> 6) & 0x7;
          reg[R_PC] = reg[base_r];
        }

        break;
      }
      case OP_LD: {
        uint16_t dr = (instr >> 9) & 0x7;
        uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
        reg[dr] = mem_read(reg[R_PC] + pc_offset);

        update_cond_flags(dr);
        break;
      }
      case OP_LDR: {
        uint16_t dr = (instr >> 9) & 0x7;
        uint16_t base_r = (instr >> 6) & 0x7;
        uint16_t offset = sign_extend(instr & 0x3F, 6);

        reg[dr] = mem_read(reg[base_r] + offset);

        update_cond_flags(dr);
        break;
      }
      case OP_LEA: {
        uint16_t dr = (instr >> 9) & 0x7;
        uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);

        reg[dr] = reg[R_PC] + pc_offset;

        update_cond_flags(dr);
        break;
      }
      case OP_ST: {
        uint16_t sr = (instr >> 9) & 0x7;
        uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);

        mem_write(reg[R_PC] + pc_offset, reg[sr]);

        break;
      }
      case OP_STI: {
        uint16_t sr = (instr >> 9) & 0x7;
        uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);

        mem_write(mem_read(reg[R_PC] + pc_offset), reg[sr]);

        break;
      }
      case OP_STR: {
        uint16_t sr = (instr >> 9) & 0x7;
        uint16_t base_r = (instr >> 6) & 0x7;
        uint16_t offset = sign_extend(instr & 0x3F, 6);

        mem_write(reg[base_r] + offset, reg[sr]);

        break;
      }
      case OP_TRAP: {
        reg[R_R7] = reg[R_PC]; // store current value of PC

        switch (instr & 0xFF) {
          case TRAP_GETC: {
            reg[R_R0] = (uint16_t) getchar();

            update_cond_flags(R_R0);
            break;
          }
          case TRAP_OUT: {
            putc((char) reg[R_R0], stdout);

            fflush(stdout);
            break;
          }
          case TRAP_PUTS: {
            uint16_t* c = memory + reg[R_R0];
            while (*c) {
              putc((char)*c, stdout);
              ++c;
            }

            fflush(stdout);
            break;
          }
          case TRAP_IN: {
            printf("Enter a character: ");
            char c = getchar();
            putc(c, stdout);
            fflush(stdout);
            reg[R_R0] = (uint16_t) c;

            update_cond_flags(R_R0);
            break;
          }
          case TRAP_PUTSP: {
            uint16_t* c = memory + reg[R_R0];

            while (*c) {
              char c1 = (*c) & 0xFF;
              char c2 = (*c) >> 8;
              putc(c1, stdout);
              if (c2) putc(c2, stdout);
              ++c;
            }

            fflush(stdout);
            break;
          }
          case TRAP_HALT: {
            puts("HALT");
            fflush(stdout);

            running = 0;
            break;
          }
        }
        break;
      }
      case OP_RES:
      case OP_RTI:
      default: {
        printf("bad op_code detected");
        exit(1);
      }
    }
  }

  restore_input_buffering();
  return 0;
}

void read_image_file(FILE* f) {
  uint16_t origin;
  fread(&origin, sizeof(origin), 1, f);
  origin = swap16(origin);

  uint16_t max_read = MEMORY_MAX - origin;
  uint16_t* p = memory + origin;
  size_t read = fread(p, sizeof(uint16_t), max_read, f);

  while (read-- > 0) {
    *p = swap16(*p);
    ++p;
  }
}

int read_image(const char* image_path) {
  FILE* file = fopen(image_path, "rb");
  if (!file) { return 0; }
  read_image_file(file);
  fclose(file);
  return 1;
}

void mem_write(uint16_t addr, uint16_t val) {
  memory[addr] = val;
}

uint16_t mem_read(uint16_t addr) {
  /* handle read from memory mapped register */
  if (addr == MR_KBSR) {
    if (check_key()) {
      memory[MR_KBSR] = (1 << 15);
      memory[MR_KBDR] = getchar();
    } else {
      memory[MR_KBSR] = 0;
    }
  }

  return memory[addr];
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

struct termios original_tio;

void disable_input_buffering() {
  tcgetattr(STDIN_FILENO, &original_tio);
  struct termios new_tio = original_tio;
  new_tio.c_lflag &= ~ICANON & ~ECHO;
  tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

void restore_input_buffering() {
  tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
}

uint16_t check_key() {
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(STDIN_FILENO, &readfds);

  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  return select(1, &readfds, NULL, NULL, &timeout) != 0;
}

void handle_interrupt(int signal) {
  restore_input_buffering();
  printf("\n");
  exit(-2);
}