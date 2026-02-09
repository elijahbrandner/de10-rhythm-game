#ifndef SEQUENCE_H
#define SEQUENCE_H

#include <stdint.h>
#include <stdbool.h>

#include "game_types.h"
#include "../fpga/fpga_if.h"

// Returns recommended bpm and seq_len for a mode
void sequence_mode_params(game_mode_t mode, uint32_t *bpm_out, uint32_t *len_out);

// Generates a sequence into out_steps (count = seq_len)
// seed can be derived from time, switches, etc.
int sequence_generate(game_mode_t mode, uint32_t seed, fpga_step_t *out_steps, uint32_t out_cap, uint32_t *out_len);

#endif // SEQUENCE_H
