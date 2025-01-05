#include <stdint.h>

uint16_t sign_extend(uint16_t x, int bits_count) {
  if ((x >> (bits_count) & 0x01)) { /* Check if MSB set to 1 - means that this number is negative */
    x |= (0xFFFF << bits_count); /* In that case - fill first bit_count bits with '1' */
  } /* If value is positive - just cast it to the 16 bit int, so bit_count bits will be filled with '0' by default*/

  return x;
}