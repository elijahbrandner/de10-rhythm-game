#ifndef ACCEL_H
#define accel_H

#include <stdint.h>
#include "../hal/hal-api.h"

typedef struct {
    hal_map_t *hal;
    int initialized;

    // last raw samples
    int16_t x;
    int16_t y;
    int16_t z;

    // optional previous samples for shake detection
    int16_t prev_x;
    int16_t prev_y;
    int16_t prev_z;

    // threshold
    uint16_t shake_threshold;
} accel_handle_t;

int accel_init(accel_handle_t *acc, hal_map_t *hal);
int accel_cleanup(accel_handle_t *acc);

int accel_read_xyz(accel_handle_t *acc, int16_t *x, int16_t *y, int16_t *z);
int accel_poll_shake(accel_handle_t *acc, int *shake_detected);

#endif
