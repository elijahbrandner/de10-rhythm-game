#include "fpga/fpga_if.h"

#include <string.h>
#include <unistd.h>

// ------------------------------------------------------------
// Control word bit fields
// ------------------------------------------------------------
#define CTRL_LANE_SHIFT     0
#define CTRL_LANE_MASK      (0x3u << CTRL_LANE_SHIFT)

#define CTRL_LED_SHIFT      2
#define CTRL_LED_MASK       (0x3u << CTRL_LED_SHIFT)

#define CTRL_TEMPO_SHIFT    4
#define CTRL_TEMPO_MASK     (0xFu << CTRL_TEMPO_SHIFT)

#define CTRL_ENABLE_BIT     (1u << 8)
#define CTRL_RESET_BIT      (1u << 9)

// ------------------------------------------------------------
// Internal helper
// ------------------------------------------------------------
static inline void write_ctrl(fpga_if_t *fpga) {
    if (!fpga || !fpga->hal || !fpga->initialized) return;
    hal_write32(fpga->hal, JP1_BASE, fpga->ctrl_word);
}

// ------------------------------------------------------------
// Public API
// ------------------------------------------------------------
int fpga_if_init(fpga_if_t *fpga, hal_map_t *hal) {
    if (!fpga || !hal || !hal->virtual_base) return -1;

    memset(fpga, 0, sizeof(*fpga));
    fpga->hal = hal;
    fpga->ctrl_word = 0;
    fpga->initialized = 1;

    // Clear JP1 control word on startup
    write_ctrl(fpga);
    return 0;
}

int fpga_if_cleanup(fpga_if_t *fpga) {
    if (!fpga) return -1;

    fpga_if_clear(fpga);
    fpga->initialized = 0;
    fpga->hal = NULL;
    fpga->ctrl_word = 0;

    return 0;
}

void fpga_if_commit(fpga_if_t *fpga) {
    write_ctrl(fpga);
}

void fpga_if_clear(fpga_if_t *fpga) {
    if (!fpga) return;
    fpga->ctrl_word = 0;
    write_ctrl(fpga);
}

void fpga_if_set_lane(fpga_if_t *fpga, fpga_lane_t lane) {
    if (!fpga) return;

    fpga->ctrl_word &= ~CTRL_LANE_MASK;
    fpga->ctrl_word |= (((uint32_t)lane & 0x3u) << CTRL_LANE_SHIFT);
}

void fpga_if_set_led_mode(fpga_if_t *fpga, fpga_led_mode_t mode) {
    if (!fpga) return;

    fpga->ctrl_word &= ~CTRL_LED_MASK;
    fpga->ctrl_word |= (((uint32_t)mode & 0x3u) << CTRL_LED_SHIFT);
}

void fpga_if_set_tempo(fpga_if_t *fpga, fpga_tempo_t tempo) {
    if (!fpga) return;

    fpga->ctrl_word &= ~CTRL_TEMPO_MASK;
    fpga->ctrl_word |= (((uint32_t)tempo & 0xFu) << CTRL_TEMPO_SHIFT);
}

void fpga_if_set_enable(fpga_if_t *fpga, bool enable) {
    if (!fpga) return;

    if (enable) fpga->ctrl_word |= CTRL_ENABLE_BIT;
    else        fpga->ctrl_word &= ~CTRL_ENABLE_BIT;
}

void fpga_if_set_reset(fpga_if_t *fpga, bool reset) {
    if (!fpga) return;

    if (reset) fpga->ctrl_word |= CTRL_RESET_BIT;
    else       fpga->ctrl_word &= ~CTRL_RESET_BIT;
}

void fpga_if_reset_pulse(fpga_if_t *fpga) {
    if (!fpga) return;

    // assert reset
    fpga_if_set_reset(fpga, true);
    write_ctrl(fpga);

    // tiny delay just to make the pulse obvious
    usleep(1000);

    // deassert reset
    fpga_if_set_reset(fpga, false);
    write_ctrl(fpga);
}

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

void fpga_if_apply(fpga_if_t *fpga, game_mode_t game_mode, uint8_t lane, fpga_led_mode_t led_mode, bool enable) {
    if (!fpga) return;

    fpga_if_set_lane(fpga, (fpga_lane_t)(lane & 0x3u));
    fpga_if_set_led_mode(fpga, led_mode);
    fpga_if_set_tempo(fpga, fpga_if_game_mode_to_tempo(game_mode));
    fpga_if_set_enable(fpga, enable);
    fpga_if_commit(fpga);
}