// FPGA interface header
// This defines:
// - fpga_if_t structure
// - enums for lane, LED mode, tempo
// - function declarations
#include "fpga/fpga_if.h"

// Needed for memset() to clear structures
#include <string.h>

// Needed for usleep() to create small delays (used in reset pulse)
#include <unistd.h>

// ------------------------------------------------------------
// Control word bit fields
// ------------------------------------------------------------

// The FPGA is controlled using a single 32-bit "control word"
// Each field inside this word controls a different part of the FPGA

// Layout (bit positions):
// [1:0]    = lane
// [3:2]    = LED mode
// [7:4]    = tempo
// [8]      = enable
// [9]      = reset

// Lane field (2 bits starting at bit 0)
#define CTRL_LANE_SHIFT     0
#define CTRL_LANE_MASK      (0x3u << CTRL_LANE_SHIFT)

// LED mode field (2 bits starting at bit 2)
#define CTRL_LED_SHIFT      2
#define CTRL_LED_MASK       (0x3u << CTRL_LED_SHIFT)

// Tempo field (4 bits starting at bit 4)
#define CTRL_TEMPO_SHIFT    4
#define CTRL_TEMPO_MASK     (0xFu << CTRL_TEMPO_SHIFT)

// Enable bit (bit 8)
// When set, FPGA logic is active
#define CTRL_ENABLE_BIT     (1u << 8)

// Reset bit (bit 9)
// Used to reset FPGA-side state machines (like beat timing)
#define CTRL_RESET_BIT      (1u << 9)

// ------------------------------------------------------------
// Internal helper
// ------------------------------------------------------------

// Writes the current control word to the FPGA hardware
//
// This function is the ONLY place where actual hardware writes ocur
// Everything else just modifies fpga->ctrl_word in memory
//
// Safety checks:
// - fpga must exist
// - hal must exist
// - interface must be initialized
static inline void write_ctrl(fpga_if_t *fpga) {
    if (!fpga || !fpga->hal || !fpga->initialized) return;
    
    // Write the 32 bit control word to the JP1 memory-mapped register
    hal_write32(fpga->hal, JP1_BASE, fpga->ctrl_word);
}

// ------------------------------------------------------------
// Public API
// ------------------------------------------------------------

// Initializes the FPGA interface
//
// Parameters:
// - fpga: interface object to initialize
// - hal: hardware abstraction layer (provides memory-mapped access)
//
// Returns 0 on success, -1 on failure
int fpga_if_init(fpga_if_t *fpga, hal_map_t *hal) {
    // Validate inputs:
    // - fpga must exist
    // - hal must exist
    // - hal must already be mapped (virtual_base valid)
    if (!fpga || !hal || !hal->virtual_base) return -1;

    // Clear the fpga interface struct to a known state
    memset(fpga, 0, sizeof(*fpga));

    // Store pointer to HAL so we can write to hardware later
    fpga->hal = hal;

    // Initialize control word to 0 (all features off)
    fpga->ctrl_word = 0;

    // Mark interface as initialized
    fpga->initialized = 1;

    // Immediately clear hardware output so FPGA starts clean
    write_ctrl(fpga);
    return 0;
}

// Cleans up the FPGA interface
// This resets hardware state and clears internal pointers
int fpga_if_cleanup(fpga_if_t *fpga) {
    if (!fpga) return -1;

    // Clear FPGA output (turn everything off)
    fpga_if_clear(fpga);

    // Mark interface as no longer initialized
    fpga->initialized = 0;

    // Remove reference to HAL
    fpga->hal = NULL;

    // Reset control word to 0
    fpga->ctrl_word = 0;

    return 0;
}

// Writes the current control word to hardware
//
// THis is used after setting multiple fields to apply them all at once
void fpga_if_commit(fpga_if_t *fpga) {
    write_ctrl(fpga);
}

// Clear all FPGA output
// Disables everything:
// - no lane
// - no LED mode
// - no tempo
// - no enable
void fpga_if_clear(fpga_if_t *fpga) {
    if (!fpga) return;

    // Reset control word to all zeros
    fpga->ctrl_word = 0;

    // Write the cleared value to hardware
    write_ctrl(fpga);
}


// Sets which lane is active
// Lane values are encoded into bits [1:0] of ctrl_word
void fpga_if_set_lane(fpga_if_t *fpga, fpga_lane_t lane) {
    if (!fpga) return;

    // Clear existing LED mode bits
    fpga->ctrl_word &= ~CTRL_LANE_MASK;
    
    // Set new LED mode (2 bit value)
    fpga->ctrl_word |= (((uint32_t)lane & 0x3u) << CTRL_LANE_SHIFT);
}

// Sets LED animation mode
//
// LED mode is encoded into bits [3:2]
void fpga_if_set_led_mode(fpga_if_t *fpga, fpga_led_mode_t mode) {
    if (!fpga) return;

    // Clear existing LED mode bits
    fpga->ctrl_word &= ~CTRL_LED_MASK;

    // Set new LED mode (2-bit value)
    fpga->ctrl_word |= (((uint32_t)mode & 0x3u) << CTRL_LED_SHIFT);
}

// Set tempo value
// tempo is encoded into bits [7:4]
// This corresponds to FPGA timing logic (e.g., BPM mapping)
void fpga_if_set_tempo(fpga_if_t *fpga, fpga_tempo_t tempo) {
    if (!fpga) return;

    // Clear existing tempo bits
    fpga->ctrl_word &= ~CTRL_TEMPO_MASK;

    // Set new tempo (4 bit value)
    fpga->ctrl_word |= (((uint32_t)tempo & 0xFu) << CTRL_TEMPO_SHIFT);
}


// Enables or disables the FPGA logic
// WHen enable = true:
// - FPGA outputs become active
//
// WHen enable = false:
// - FPGA outputs are effectively disabled
void fpga_if_set_enable(fpga_if_t *fpga, bool enable) {
    if (!fpga) return;

    if (enable) fpga->ctrl_word |= CTRL_ENABLE_BIT;     // Set enable bit
    else        fpga->ctrl_word &= ~CTRL_ENABLE_BIT;    // Clear enable bit
}

// Sets or clears the FPGA reset signal
//
// Reset is typically used to restart timing circuits like beat generators
void fpga_if_set_reset(fpga_if_t *fpga, bool reset) {
    if (!fpga) return;

    if (reset) fpga->ctrl_word |= CTRL_RESET_BIT; // Assert reset
    else       fpga->ctrl_word &= ~CTRL_RESET_BIT; // Deassert reset
}

// Generate a short reset pulse on the FPGA
//
// THis is used to:
// -restart beat timing
// - reset interanal FPGA state machines
//
// Implementation:
// 1. Set reset bit
// 2. Write to hardware
// 3. Wait briefly
// 4. Clear reset bit
// 5. Write again
void fpga_if_reset_pulse(fpga_if_t *fpga) {
    if (!fpga) return;

    // Step 1. assert reset
    fpga_if_set_reset(fpga, true);

    // Step 2. Apply to hardware
    write_ctrl(fpga);

    // Step 3. tiny delay just to make the pulse obvious
    usleep(1000);

    // Step 4. deassert reset
    fpga_if_set_reset(fpga, false);
    
    // Step 5. apply again
    write_ctrl(fpga);
}


// Converts a game mode / difficulty into an FPGA tempo val

// This keeps mapping consistent between software and hardware timing
fpga_tempo_t fpga_if_game_mode_to_tempo(game_mode_t mode)
{
    switch (mode)
    {
        case GAME_MODE_EASY:
            return FPGA_TEMPO_45_BPM;

        case GAME_MODE_MEDIUM:
            return FPGA_TEMPO_60_BPM;

        case GAME_MODE_HARD:
            return FPGA_TEMPO_75_BPM;

        case GAME_MODE_EXPERT:
            return FPGA_TEMPO_75_BPM;

        default:
            return FPGA_TEMPO_60_BPM;
    }
}

// Convenience function to apply a full FPGA configuration in one call
//
// This bundles together:
// - lane selection
// - LED mode
// - tempo (derived from game mode)
// - enable flag
//
// Then commits everything to hardware
void fpga_if_apply(fpga_if_t *fpga, game_mode_t game_mode, uint8_t lane, fpga_led_mode_t led_mode, bool enable) {
    if (!fpga) return;

    // Set lane (masked to 2 bits for safety)
    fpga_if_set_lane(fpga, (fpga_lane_t)(lane & 0x3u));
    
    // Set LED animation mode
    fpga_if_set_led_mode(fpga, led_mode);
    
    // Convert game mode into FPGA tempo and set it
    fpga_if_set_tempo(fpga, fpga_if_game_mode_to_tempo(game_mode));

    // Enable or disable output
    fpga_if_set_enable(fpga, enable);

    // Apply all changes to hardware
    fpga_if_commit(fpga);
}