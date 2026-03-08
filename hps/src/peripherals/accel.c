#include "../../includes/peripherals/accel.h"

#include <string.h>

// -----------------------------------------------------------------------------
// Stub implementation for now
// -----------------------------------------------------------------------------

int accel_init(accel_handle_t *acc, hal_map_t *hal) {
    if (!acc || !hal) return -1;

    memset(acc, 0, sizeof(*acc));
    acc->hal = hal;
    acc->initialized = 1;

    // Default threshold for future shake detection tuning
    acc->shake_threshold = 500;

    return 0;
}

int accel_cleanup(accel_handle_t *acc) {
    if (!acc) return -1;

    acc->initialized = 0;
    acc->hal = NULL;

    acc->x = 0;
    acc->y = 0;
    acc->z = 0;

    acc->prev_x = 0;
    acc->prev_y = 0;
    acc->prev_z = 0;

    acc->shake_threshold = 0;

    return 0;
}

int accel_read_xyz(accel_handle_t *acc, int16_t *x, int16_t *y, int16_t *z) {
    if (!acc || !acc->initialized || !x || !y || !z) return -1;

    // Stub values for now
    acc->prev_x = acc->x;
    acc->prev_y = acc->y;
    acc->prev_z = acc->z;

    acc->x = 0;
    acc->y = 0;
    acc->z = 0;

    *x = acc->x;
    *y = acc->y;
    *z = acc->z;

    return 0;
}

int accel_poll_shake(accel_handle_t *acc, int *shake_detected) {
    if (!acc || !acc->initialized || !shake_detected) return -1;

    int16_t x = 0, y = 0, z = 0;
    if (accel_read_xyz(acc, &x, &y, &z) != 0) {
        return -1;
    }

    // Stub behavior: no shake detected yet
    *shake_detected = 0;
    return 0;
}