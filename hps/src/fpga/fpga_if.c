#include "fpga/fpga_if.h"
#include <string.h>

// ------------------------------------------------------------
// Register map (byte offsets relative to RHYTHM_IF_BASE)
// ------------------------------------------------------------

#define REG_CTRL_BYTES                 0x00
#define REG_MODE_BYTES                 0x04
#define REG_BPM_BYTES                  0x08
#define REG_SEQ_LEN_BYTES              0x0C

#define REG_SEQ_DATA0_BYTES            0x20   // step array starts here (each step = 4 bytes)

#define REG_STATUS_BYTES               0x100
#define REG_STEP_INDEX_BYTES           0x104
#define REG_BEAT_EDGE_BYTES            0x108
#define REG_WINDOW_ACTIVE_BYTES        0x10C
#define REG_SHAKE_WINDOW_ACTIVE_BYTES  0x110

// CTRL bits
#define CTRL_RUN_PREVIEW   (1u << 0)
#define CTRL_RUN_PLAYBACK  (1u << 1)
#define CTRL_RESET         (1u << 2)
#define CTRL_LED_IDLE_EN   (1u << 3)

// STATUS bits
#define STATUS_PREVIEW_ACTIVE  (1u << 0)
#define STATUS_PLAYBACK_ACTIVE (1u << 1)
#define STATUS_DONE            (1u << 2)

// Max supported steps in HW register array (pick a safe ceiling)
#define FPGA_MAX_STEPS 64u

// ------------------------------------------------------------
// Internal helpers (HAL-based MMIO)
// ------------------------------------------------------------

static inline void write_reg(fpga_if_t *fpga, uint32_t reg_byte_off, uint32_t value) {
  if (!fpga || !fpga->hal) return;
  hal_write32(fpga->hal, RHYTHM_IF_BASE + reg_byte_off, value);
}

static inline uint32_t read_reg(fpga_if_t *fpga, uint32_t reg_byte_off) {
  if (!fpga || !fpga->hal) return 0;
  return hal_read32(fpga->hal, RHYTHM_IF_BASE + reg_byte_off);
}

// ------------------------------------------------------------
// Public API
// ------------------------------------------------------------

int fpga_if_init(fpga_if_t *fpga, hal_map_t *hal) {
  if (!fpga || !hal || !hal->virtual_base) return -1;
  memset(fpga, 0, sizeof(*fpga));
  fpga->hal = hal;
  fpga_if_reset(fpga);
  return 0;
}

uint32_t fpga_if_pack_step(fpga_step_t s) {
  // Packed format:
  // bits[1:0] lane (0..3)
  // bit2 flash_all
  // bit3 shake
  uint32_t v = 0;
  v |= (uint32_t)(s.lane & 0x3u);
  v |= (s.flash_all ? (1u << 2) : 0u);
  v |= (s.shake ? (1u << 3) : 0u);
  return v;
}

void fpga_if_reset(fpga_if_t *fpga) {
  if (!fpga) return;

  // assert reset, clear run bits
  uint32_t ctrl = read_reg(fpga, REG_CTRL_BYTES);
  ctrl |= CTRL_RESET;
  ctrl &= ~(CTRL_RUN_PREVIEW | CTRL_RUN_PLAYBACK);
  write_reg(fpga, REG_CTRL_BYTES, ctrl);

  // deassert reset
  ctrl &= ~CTRL_RESET;
  write_reg(fpga, REG_CTRL_BYTES, ctrl);
}

void fpga_if_configure(fpga_if_t *fpga, game_mode_t mode, uint32_t bpm, uint32_t seq_len) {
  if (!fpga) return;

  fpga->mode = mode;
  fpga->bpm = bpm;
  fpga->seq_len = seq_len;

  write_reg(fpga, REG_MODE_BYTES, (uint32_t)mode);
  write_reg(fpga, REG_BPM_BYTES, bpm);
  write_reg(fpga, REG_SEQ_LEN_BYTES, seq_len);

  // enable idle LEDs by default (FPGA can ignore if not implemented yet)
  uint32_t ctrl = read_reg(fpga, REG_CTRL_BYTES);
  ctrl |= CTRL_LED_IDLE_EN;
  write_reg(fpga, REG_CTRL_BYTES, ctrl);
}

int fpga_if_load_sequence(fpga_if_t *fpga, const fpga_step_t *steps, uint32_t count) {
  if (!fpga || !steps) return -1;
  if (count == 0 || count > FPGA_MAX_STEPS) return -1;

  fpga->seq_len = count;
  write_reg(fpga, REG_SEQ_LEN_BYTES, count);

  for (uint32_t i = 0; i < count; i++) {
    uint32_t word = fpga_if_pack_step(steps[i]);
    write_reg(fpga, REG_SEQ_DATA0_BYTES + (i * 4u), word);
  }

  return 0;
}

void fpga_if_start_preview(fpga_if_t *fpga) {
  if (!fpga) return;
  uint32_t ctrl = read_reg(fpga, REG_CTRL_BYTES);
  ctrl |= CTRL_RUN_PREVIEW;
  ctrl &= ~CTRL_RUN_PLAYBACK;
  write_reg(fpga, REG_CTRL_BYTES, ctrl);
}

void fpga_if_start_playback(fpga_if_t *fpga) {
  if (!fpga) return;
  uint32_t ctrl = read_reg(fpga, REG_CTRL_BYTES);
  ctrl |= CTRL_RUN_PLAYBACK;
  ctrl &= ~CTRL_RUN_PREVIEW;
  write_reg(fpga, REG_CTRL_BYTES, ctrl);
}

void fpga_if_stop(fpga_if_t *fpga) {
  if (!fpga) return;
  uint32_t ctrl = read_reg(fpga, REG_CTRL_BYTES);
  ctrl &= ~(CTRL_RUN_PREVIEW | CTRL_RUN_PLAYBACK);
  write_reg(fpga, REG_CTRL_BYTES, ctrl);
}

fpga_status_t fpga_if_read_status(fpga_if_t *fpga) {
  fpga_status_t s;
  memset(&s, 0, sizeof(s));
  if (!fpga) return s;

  uint32_t st = read_reg(fpga, REG_STATUS_BYTES);
  s.preview_active  = (st & STATUS_PREVIEW_ACTIVE) != 0;
  s.playback_active = (st & STATUS_PLAYBACK_ACTIVE) != 0;
  s.done            = (st & STATUS_DONE) != 0;

  s.step_index = read_reg(fpga, REG_STEP_INDEX_BYTES);
  s.beat_edge  = read_reg(fpga, REG_BEAT_EDGE_BYTES);

  s.window_active       = (read_reg(fpga, REG_WINDOW_ACTIVE_BYTES) != 0);
  s.shake_window_active = (read_reg(fpga, REG_SHAKE_WINDOW_ACTIVE_BYTES) != 0);

  return s;
}
