# CLAUDE.md

This repository contains the **Owl** project — a compact USB/Packet-Mode mixed-signal oscilloscope built around the Raspberry Pi RP2350 (dual Cortex-M33).

## Documentation-first stage

No source code has been written yet. The repo holds design docs:

- `doc/owl_spec_v0_1.md` — Product spec (target platform, architecture, dual-core partitioning, decoder tiers, visualization approach)
- `doc/websocket_and_data_spec.md` — WebSocket binary framing protocol for device-to-browser streaming
- `visualization_approach/visualize_approach.md` — Visualization architecture notes

## Future development guidelines

### Target platform

- **MCU:** Raspberry Pi RP2350 (Arm Cortex-M33 @ 150 MHz, dual-core)
- **Ethernet:** Microchip LAN9250 via SPI (Mikroe Eth Click board)
- **External memory:** Optional QSPI PSRAM
- **RTOS:** Zephyr on Core 1; bare-metal/cooperative on Core 0
- **Toolchain:** ARM GCC (`arm-none-eabi-gcc`), Zephyr SDK

### Code style conventions (when code is added)

- **C (firmware):** Follow existing Zephyr coding style — snake_case for identifiers, 4-space indent, Kconfig-style comments for config. Use `__packed` structs for hardware registers and wire protocol layouts. Document all public APIs with Doxygen-style block comments.
- **JavaScript/TypeScript (visualization/web):** ES modules, semicolons, single quotes, Prettier-style formatting. Use `async/await` over raw Promises where readable.
- **Documentation:** Markdown files — 80-col line wraps, ATX headings, relative links to other docs.

### Key architecture decisions

- Core 0 owns PIO + DMA + trigger evaluation; Core 1 owns Zephyr + Ethernet + WebSocket + REST API
- All wire protocol fields are little-endian with explicit byte offsets (see `websocket_and_data_spec.md`)
- Two-tier decoder architecture: Native (PIO assembly + C on Core 0) and User-authored (PIO Assembly, PIO Synthesis, or post-capture Python via Pyodide)
