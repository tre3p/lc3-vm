#ifndef LC3_VM__MM_REGISTERS_H_
#define LC3_VM__MM_REGISTERS_H_

/**
 * Memory Mapped Register to interact with special hardware devices
 */
enum  {
  MR_KBSR = 0xFE00, /* keyboard status */
  MR_KBDR = 0xFE02 /* keyboard data */
};

#endif //LC3_VM__MM_REGISTERS_H_
