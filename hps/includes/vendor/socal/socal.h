#ifndef SOCAL_H
#define SOCAL_H
#include <stdint.h>
#define alt_read_word(addr) (*((volatile uint32_t *)(addr)))
#define alt_write_word(addr, val) (*((volatile uint32_t *)(addr)) = (val))
#define alt_setbits_word(addr, mask) (*((volatile uint32_t *)(addr)) |= (mask))
#define alt_clrbits_word(addr, mask) (*((volatile uint32_t *)(addr)) &= ~(mask))
#endif
