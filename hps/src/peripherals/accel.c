// Include accelerometer interface header.
// This provides accel_handle_t and the public accel API.
#include "../../includes/peripherals/accel.h"

// Include the ADXL345 driver header.
// This provides the low-level accelerometer/I2C helper functions.
#include "../../includes/peripherals/ADXL345.h"

#include "../../lib/address_map_arm.h"

// Needed for memset().
#include <string.h>

// Needed for printf().
#include <stdio.h>

// Needed for open().
#include <fcntl.h>

// Needed for mmap()/munmap().
#include <sys/mman.h>

// Needed for close().
#include <unistd.h>

// -----------------------------------------------------------------------------
// Internal globals used by the ADXL345 driver
// -----------------------------------------------------------------------------
//
// The ADXL345 helper code expects these globals to exist.
// They point to the memory-mapped I2C0 and SYSMGR regions.
volatile int *I2C0_ptr = NULL;
volatile int *SYSMGR_ptr = NULL;

// Local file descriptor used for the accelerometer's own /dev/mem mapping.
// This is separate from your HAL mapping.
static int accel_fd = -1;

// Track whether these regions were successfully mapped by this module.
static int accel_i2c_mapped = 0;
static int accel_sysmgr_mapped = 0;

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

int accel_init(accel_handle_t *acc, hal_map_t *hal) {
    if (!acc || !hal) return -1;

    // Clear the handle first so all fields start clean.
    memset(acc, 0, sizeof(*acc));

    // Store HAL pointer for consistency with the rest of the project.
    // This module does not directly use the LW bridge HAL mapping for ADXL345,
    // but we keep the pointer because accel_handle_t expects it.
    acc->hal = hal;

    // Default shake threshold for possible future delta-based detection.
    // This is not strictly required when using ADXL345 activity detection,
    // but it is still useful to keep in the handle.
    acc->shake_threshold = 500;

    // Open /dev/mem so we can map the HPS-side I2C0 and SYSMGR regions.
    accel_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (accel_fd == -1) {
        perror("[ACCEL] ERROR: could not open /dev/mem");
        return -1;
    }

    // Map I2C0 controller registers.
    I2C0_ptr = (volatile int *)mmap(
        NULL,
        I2C0_SPAN,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        accel_fd,
        I2C0_BASE
    );

    if (I2C0_ptr == MAP_FAILED) {
        perror("[ACCEL] ERROR: mmap() failed for I2C0");
        I2C0_ptr = NULL;
        close(accel_fd);
        accel_fd = -1;
        return -1;
    }
    accel_i2c_mapped = 1;

    // Map system manager registers.
    SYSMGR_ptr = (volatile int *)mmap(
        NULL,
        SYSMGR_SPAN,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        accel_fd,
        SYSMGR_BASE
    );

    if (SYSMGR_ptr == MAP_FAILED) {
        perror("[ACCEL] ERROR: mmap() failed for SYSMGR");
        SYSMGR_ptr = NULL;

        if (accel_i2c_mapped) {
            munmap((void *)I2C0_ptr, I2C0_SPAN);
            accel_i2c_mapped = 0;
            I2C0_ptr = NULL;
        }

        close(accel_fd);
        accel_fd = -1;
        return -1;
    }
    accel_sysmgr_mapped = 1;

    // Configure the pin mux so the ADXL345 lines are routed to I2C0.
    Pinmux_Config();

    // Initialize the I2C0 controller.
    I2C0_Init();

    // Verify the ADXL345 device ID.
    // The expected device ID is 0xE5.
    uint8_t devid = 0;
    ADXL345_REG_READ(ADXL345_REG_DEVID, &devid);

    if (devid != 0xE5) {
        fprintf(stderr, "[ACCEL] ERROR: incorrect ADXL345 device ID: 0x%02X\n", devid);

        if (accel_sysmgr_mapped) {
            munmap((void *)SYSMGR_ptr, SYSMGR_SPAN);
            accel_sysmgr_mapped = 0;
            SYSMGR_ptr = NULL;
        }

        if (accel_i2c_mapped) {
            munmap((void *)I2C0_ptr, I2C0_SPAN);
            accel_i2c_mapped = 0;
            I2C0_ptr = NULL;
        }

        close(accel_fd);
        accel_fd = -1;
        return -1;
    }

    // Initialize the ADXL345 itself.
    ADXL345_Init();

    // Optional:
    // You can enable this later if you want calibration at startup.
    // Be aware that calibration expects the board to be still/flat.
    // ADXL345_Calibrate();

    // Mark accelerometer handle as initialized only after all setup succeeds.
    acc->initialized = 1;

    printf("[ACCEL] ADXL345 initialized successfully.\n");
    return 0;
}

int accel_cleanup(accel_handle_t *acc) {
    if (!acc) return -1;

    // Reset handle state.
    acc->initialized = 0;
    acc->hal = NULL;

    acc->x = 0;
    acc->y = 0;
    acc->z = 0;

    acc->prev_x = 0;
    acc->prev_y = 0;
    acc->prev_z = 0;

    acc->shake_threshold = 0;

    // Unmap SYSMGR if this module mapped it.
    if (accel_sysmgr_mapped && SYSMGR_ptr) {
        munmap((void *)SYSMGR_ptr, SYSMGR_SPAN);
        accel_sysmgr_mapped = 0;
        SYSMGR_ptr = NULL;
    }

    // Unmap I2C0 if this module mapped it.
    if (accel_i2c_mapped && I2C0_ptr) {
        munmap((void *)I2C0_ptr, I2C0_SPAN);
        accel_i2c_mapped = 0;
        I2C0_ptr = NULL;
    }

    // Close /dev/mem if it was opened here.
    if (accel_fd >= 0) {
        close(accel_fd);
        accel_fd = -1;
    }

    return 0;
}

int accel_read_xyz(accel_handle_t *acc, int16_t *x, int16_t *y, int16_t *z) {
    if (!acc || !acc->initialized || !x || !y || !z) return -1;

    int16_t XYZ[3] = {0, 0, 0};

    // Save previous readings before updating current ones.
    acc->prev_x = acc->x;
    acc->prev_y = acc->y;
    acc->prev_z = acc->z;

    // Read fresh X/Y/Z values from the ADXL345.
    ADXL345_XYZ_Read(XYZ);

    // Store them in the handle.
    acc->x = XYZ[0];
    acc->y = XYZ[1];
    acc->z = XYZ[2];

    // Return them to the caller.
    *x = acc->x;
    *y = acc->y;
    *z = acc->z;

    return 0;
}

int accel_poll_shake(accel_handle_t *acc, int *shake_detected) {
    if (!acc || !acc->initialized || !shake_detected) return -1;

    int16_t x = 0, y = 0, z = 0;

    // Refresh current XYZ readings so the handle always has up-to-date values.
    if (accel_read_xyz(acc, &x, &y, &z) != 0) {
        return -1;
    }

    // Use the ADXL345 activity interrupt/status as the shake indicator.
    // This is the simplest and most reliable first implementation
    // based on your earlier working activity.
    if (ADXL345_WasActivityUpdated()) {
        *shake_detected = 1;
    } else {
        *shake_detected = 0;
    }

    return 0;
}