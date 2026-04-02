#ifndef ACCEL_H
#define ACCEL_H

#include <stdint.h>
#include "../hal/hal-api.h"

// Handle for the accelerometer peripheral.
//
// This structure stores:
// - HAL reference
// - init status
// - current XYZ readings
// - previous XYZ readings
// - shake threshold for optional software-based shake detection
typedef struct {
    hal_map_t *hal;
    int initialized;

    // Current raw samples
    int16_t x;
    int16_t y;
    int16_t z;

    // Previous raw samples
    int16_t prev_x;
    int16_t prev_y;
    int16_t prev_z;

    // Threshold used if software delta-based shake detection is added/tuned
    uint16_t shake_threshold;
} accel_handle_t;

// Initialize the accelerometer subsystem.
// Returns 0 on success, -1 on failure.
int accel_init(accel_handle_t *acc, hal_map_t *hal);

// Clean up the accelerometer subsystem.
// Returns 0 on success, -1 on failure.
int accel_cleanup(accel_handle_t *acc);

// Read current X/Y/Z accelerometer values.
// Returns 0 on success, -1 on failure.
int accel_read_xyz(accel_handle_t *acc, int16_t *x, int16_t *y, int16_t *z);

// Poll for shake/activity detection.
// Writes 1 to shake_detected if shake/activity is detected, else 0.
// Returns 0 on success, -1 on failure.
int accel_poll_shake(accel_handle_t *acc, int *shake_detected);

#endif // ACCEL_H