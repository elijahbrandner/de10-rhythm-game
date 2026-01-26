#ifndef HAL_API_H
#define HAL_API_H

#include <stddef.h> // for size_t
#include <stdint.h> // for uint32_t

// --------------------------------------------------------------
// HAL STRUCTURE
// --------------------------------------------------------------

typedef struct {
    int fd;                 // File descriptor for /dev/mem
    void *virtual_base;     // Base virtual address (mmap pointer)
    unsigned int span;      // Memory span for the mapping
} hal_map_t;

// --------------------------------------------------------------
// FUNCTION PROTOTYPES
// --------------------------------------------------------------

// Open and map /dev/mem to access physical FPGA addresses
// Returns 0 on success, -1 on failure
int hal_open(hal_map_t *map);

// Close mapping and cleanup
int hal_close(hal_map_t *map);

// Get virutual address of register at given offset
void* hal_get_virtual_addr(hal_map_t *map, unsigned int offset);

// 32-bit MMIO read/write
void     hal_write32(hal_map_t *map, unsigned int offset, uint32_t data);
uint32_t hal_read32(hal_map_t *map, unsigned int offset);


#endif // HAL_API_H