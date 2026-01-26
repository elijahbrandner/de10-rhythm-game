#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "../../lib/address_map_arm.h"
#include "../../includes/hal/hal-api.h"

// --------------------------------------------------------------
// HAL IMPLEMENTATION
// --------------------------------------------------------------

// Open and initialize the hardware abstraction layer
int hal_open(hal_map_t *map) {
    if (!map) return -1;

    // Open /dev/mem for raw access
    map->fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (map->fd == -1) {
        perror("[HAL] ERROR: could not open /dev/mem");
        return -1;
    }

    // Map the lightweight bridge
    map->virtual_base = mmap(
        NULL, 
        LW_BRIDGE_SPAN,
        PROT_READ | PROT_WRITE, 
        MAP_SHARED,
        map->fd,
        LW_BRIDGE_BASE
    );

    if (map->virtual_base == MAP_FAILED) {
        perror("[HAL] ERROR: mmap() failed");
        close(map->fd);
        return -1;
    }

    map->span = LW_BRIDGE_SPAN;
    printf("[HAL] Hardware mapping successful (base: 0x%X)\n", LW_BRIDGE_BASE);
    return 0;
}

int hal_close(hal_map_t *map) {
    if (!map) return -1;

    if (map->virtual_base) {
        munmap(map->virtual_base, map->span);
        map->virtual_base = NULL;
    }

    if (map->fd >= 0) {
        close(map->fd);
        map->fd = -1;
    }

    return 0;
}

void* hal_get_virtual_addr(hal_map_t *map, unsigned int offset) {
    if (!map || !map->virtual_base) return NULL;
    return (void*)((uint8_t *)map->virtual_base + offset);
}

// -----------------------------------------------------------------------------
// 32-bit MMIO Write
// -----------------------------------------------------------------------------
void hal_write32(hal_map_t *map, unsigned int offset, uint32_t data) {
    if (!map || !map->virtual_base) return;

    uint32_t *addr = (uint32_t *)((uint8_t *)map->virtual_base + offset);
    *addr = data;
}

// -----------------------------------------------------------------------------
// 32-bit MMIO Read
// -----------------------------------------------------------------------------
uint32_t hal_read32(hal_map_t *map, unsigned int offset) {
    if (!map || !map->virtual_base) return 0;

    uint32_t *addr = (uint32_t *)((uint8_t *)map->virtual_base + offset);
    return *addr;
}