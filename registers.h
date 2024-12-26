#ifndef LC3_VM__REGISTERS_H_
#define LC3_VM__REGISTERS_H_

enum {
  R_R0 = 0, /* R0-R7 are GP registers */
  R_R1,
  R_R2,
  R_R3,
  R_R4,
  R_R5,
  R_R6,
  R_R7,
  R_PC, /* program counter */
  R_COND, /* condition flag */
  R_COUNT, /* total amount of registers */
};

#endif //LC3_VM__REGISTERS_H_
