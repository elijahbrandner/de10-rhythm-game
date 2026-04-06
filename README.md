# Rhythm-Based Timing Game

## Overview
This project is an embedded rhythm-based timing game developed on the Terasic DE10-Standard FPGA platform. The system combines an ARM Hard Processor System (HPS) running a C application with custom FPGA logic to create an interactive, timing-critical game.

Players observe rhythm and sequence prompts displayed on seven-segment displays, then attempt to replicate the sequence using physical inputs (buttons and accelerometer) with accurate timing. The system evaluates both correctness and timing precision, producing a normalized score and performance rating.

Advanced gameplay introduces accelerometer-based “shake” notes, adding a physical interaction component to the rhythm mechanics.

---

## Key Features
- Real-time rhythm-based gameplay with timing-sensitive scoring
- Multiple difficulty modes (Easy, Medium, Hard, Expert)
- Ladder mode (automatic progression) and Free-select mode
- FPGA-driven beat-synchronized LED animations
- Seven-segment display prompts for input lanes
- Accelerometer-based shake detection (Expert mode)
- LCD-based UI displaying game state, score, and rating
- Memory-mapped HPS ↔ FPGA communication via LW bridge

---

## System Architecture
The system is partitioned into hardware and software components:

### HPS (ARM Cortex-A9)
- Game state machine (idle → playback → results)
- Sequence generation and difficulty scaling
- Input handling (buttons, switches, accelerometer)
- Scoring logic and timing window evaluation
- LCD display control

### FPGA (Cyclone V)
- Beat and tempo engine (deterministic timing)
- LED animation controller (beat-synchronized effects)
- Seven-segment display driver (lane prompts)

The FPGA provides precise timing and visual synchronization, while the HPS manages all gameplay logic and scoring.

---

## Platform
- Terasic DE10-Standard
- ARM Cortex-A9 HPS
- Intel Cyclone V FPGA

---

## Technologies
- C (HPS application)
- Verilog (FPGA logic)
- Memory-mapped I/O (/dev/mem, LW Bridge)
- I2C communication (ADXL345 accelerometer)
- Hardware peripherals:
  - Seven-segment displays (HEX)
  - LEDs
  - Push buttons (KEY)
  - Switches (SW)
  - LCD display

---

## Gameplay Flow
1. **Idle** – Player selects mode and starts the game
2. **Preview** – Sequence is displayed (no input)
3. **Countdown** – Short delay before gameplay
4. **Playback** – Player replicates the sequence in time
5. **Results** – Score (0–100) and rating displayed

In Expert mode, certain steps require a shake input instead of a button press.

---

## Controls
- **KEY0–KEY3** → Input lanes
- **SW9** → Mode select (Ladder / Free)
- **SW1–SW0** → Difficulty (Free-select mode)
- **All switches ON + KEY0** → Exit

---

## Project Structure
- `src/` – HPS C application
  - `game/` – Game logic, scoring, sequence generation
  - `fpga/` – FPGA interface layer (control word handling)
  - `hal/` – Memory-mapped hardware abstraction
  - `peripherals/` – LCD, buttons, switches, accelerometer
- `lib/` – Hardware address mappings
- `docs/` – Design documents and milestone reports

---

## FPGA Design
The FPGA implements three key modules:
- **beat_engine.v** – Generates tempo-based beat signals
- **hex_prompt.v** – Displays lane prompts on HEX displays
- **led_controller.v** – Handles LED animations

These modules are integrated in `rhythm_fpga_circuits.v` and controlled via a 32-bit control word from the HPS.

---

## Milestones
- Milestone 1: System design and architecture
- Milestone 2–4: Core gameplay and FPGA integration
- Milestone 5: Final system integration and testing

---

## Demo
🎥 https://youtu.be/a1bEhrv9Sis

---

## Repository
🔗 https://github.com/elijahbrandner/de10-rhythm-game

---

## Future Improvements
- Fine-tune scoring windows and gameplay difficulty
- Improve LED/HEX synchronization timing
- Add additional game modes and visual effects
- Enhance accelerometer calibration for more reliable shake detection
