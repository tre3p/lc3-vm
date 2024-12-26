#ifndef LC3_VM__COND_FLAGS_H_
#define LC3_VM__COND_FLAGS_H_

enum {
  FL_POS = 1 << 0, /* Positive */
  FL_ZRO = 1 << 1, /* Zero */
  FL_NEG = 1 << 2, /* Negative */
};

#endif //LC3_VM__COND_FLAGS_H_
