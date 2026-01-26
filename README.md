# Rhythm-Based Timing Game

## Overview
This project is an embedded rhythm-based timing game developed on the Terasic DE10-Standard FPGA platform. The system combines an ARM Hard Processor System (HPS) running a C application with custom FPGA logic to create an interactive timing and sequence-matching game.

Players observe rhythm and sequence prompts displayed on seven-segment displays, then attempt to replicate the sequence using physical inputs with accurate timing. Advanced modes introduce accelerometer-based "shake" notes for added complexity.

## Platform
- Terasic DE10-Standard
- ARM Cortex-A9 HPS
- Intel Cyclone V FPGA

## Technologies
- C (ARM HPS application)
- Verilog/VHDL (FPGA logic)
- Memory-mapped I/O (LW Bridge)
- Seven-segment displays, LEDs, buttons, switches, accelerometer

## Project Structure
- `hps/` – ARM HPS C application
- `fpga/` – Custom FPGA logic modules
- `docs/` – Design documents, diagrams, and milestone deliverables

## Milestones
- Milestone 1: Project Proposal and Initial Design
- Milestone 2–5: Iterative development and FPGA integration
