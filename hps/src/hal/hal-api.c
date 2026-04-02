// Standard I/O for printf() and perror()
#include <stdio.h>

// UNIX/POSIX functions like close()
#include <unistd.h>

// File control (open flags)
#include <fcntl.h>

// Memory mapping functions (mmap, munmap)
#include <sys/mman.h>

// Address map for FPGA lightweight bridge base and span
#include "../../lib/address_map_arm.h"

// HAL interface header (defines hal_map_t and function prototypes)
#include "../../includes/hal/hal-api.h"

// --------------------------------------------------------------
// HAL IMPLEMENTATION
// --------------------------------------------------------------

// Open and initialize the hardware abstraction layer.
//
// Parameters:
// - map: pointer to HAL mapping structure
//
// Returns:
// - 0 on success
// - -1 on failure
//
// What this does:
// 1. Opens /dev/mem to gain access to physical memory
// 2. Maps the FPGA lightweight bridge into user-space virtual memory
// 3. Stores mapping info in hal_map_t
//
// This allows our C code to directly read/write FPGA registers.
int hal_open(hal_map_t *map) {
    // Validate input pointer
    if (!map) return -1;

    // Open /dev/mem for raw physical memory access
    //
    // O_RDWR = read/write access
    // O_SYNC = synchronous access (important for hardware consistency)
    map->fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (map->fd == -1) {
        perror("[HAL] ERROR: could not open /dev/mem");
        return -1;
    }

    // Map the lightweight bridge into virtual memory
    //
    // LW_BRIDGE_BASE = physical base address of FPGA bridge
    // LW_BRIDGE_SPAN = size of region to map
    //
    // mmap returns a virtual address that corresponds to that physical region.
    map->virtual_base = mmap(
        NULL,                       // let OS choose virtual address
        LW_BRIDGE_SPAN,             // size of mapping
        PROT_READ | PROT_WRITE,     // allow read/write
        MAP_SHARED,                 // shared mapping
        map->fd,                    // file descriptor (/dev/mem)
        LW_BRIDGE_BASE              // physical base address
    );

    // If mapping fails, clean up and return error
    if (map->virtual_base == MAP_FAILED) {
        perror("[HAL] ERROR: mmap() failed");
        close(map->fd);
        return -1;
    }

    // Store the mapped span for later unmapping
    map->span = LW_BRIDGE_SPAN;

    // Print success message for debugging/confirmation
    printf("[HAL] Hardware mapping successful (base: 0x%X)\n", LW_BRIDGE_BASE);
    return 0;
}

// Close and clean up the HAL mapping.
//
// Parameters:
// - map: pointer to HAL structure
//
// Returns:
// - 0 on success
// - -1 on failure
//
// What this does:
// 1. Unmaps the virtual memory region
// 2. Closes the /dev/mem file descriptor
int hal_close(hal_map_t *map) {
    // Validate pointer
    if (!map) return -1;

    // If memory was mapped, unmap it
    if (map->virtual_base) {
        munmap(map->virtual_base, map->span);
        map->virtual_base = NULL;
    }

    // If file descriptor is valid, close it
    if (map->fd >= 0) {
        close(map->fd);
        map->fd = -1;
    }

    return 0;
}

// Get a virtual address pointer for a specific hardware register.
//
// Parameters:
// - map: HAL mapping structure
// - offset: offset from LW bridge base (e.g., KEY_BASE, SW_BASE, JP1_BASE)
//
// Returns:
// - pointer to the mapped register
// - NULL on failure
//
// This function converts a hardware offset into a usable pointer.
void* hal_get_virtual_addr(hal_map_t *map, unsigned int offset) {
    // Validate mapping
    if (!map || !map->virtual_base) return NULL;

    // Return pointer to the requested offset within the mapped region
    return (void*)((uint8_t *)map->virtual_base + offset);
}

// -----------------------------------------------------------------------------
// 32-bit MMIO Write
// -----------------------------------------------------------------------------
// Write a 32-bit value to a memory-mapped hardware register.
//
// Parameters:
// - map: HAL mapping
// - offset: register offset from base
// - data: value to write
//
// This is how our C code sends data to the FPGA.
void hal_write32(hal_map_t *map, unsigned int offset, uint32_t data) {
    // Validate mapping
    if (!map || !map->virtual_base) return;

    // Compute the address of the target register
    uint32_t *addr = (uint32_t *)((uint8_t *)map->virtual_base + offset);

    // Write the data directly to hardware
    *addr = data;
}

// -----------------------------------------------------------------------------
// 32-bit MMIO Read
// -----------------------------------------------------------------------------
// Read a 32-bit value from a memory-mapped hardware register.
//
// Parameters:
// - map: HAL mapping
// - offset: register offset from base
//
// Returns:
// - 32-bit value read from hardware
//
// This is how your C code reads FPGA inputs like buttons/switches.
uint32_t hal_read32(hal_map_t *map, unsigned int offset) {
    // Validate mapping
    if (!map || !map->virtual_base) return 0;

    // Compute the address of the target register
    uint32_t *addr = (uint32_t *)((uint8_t *)map->virtual_base + offset);

    // Return the value stored in that hardware register
    return *addr;
}