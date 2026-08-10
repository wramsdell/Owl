# Owl

**Networked Logic Analyzer & Bus Decoder**

*Product Specification*

Revision 0.1 — DRAFT | 2026-03-06

| Field | Value |
|---|---|
| Status | Draft — Pre-development |
| Revision | 0.1 |
| Target Hardware | Owl (custom RP2350-based board) |
| Dev Platform | Raspberry Pi Pico 2 + Mikroe Eth Click (LAN9250 via SPI) |
| License | Fully open source — hardware, firmware, and software (specific licenses TBD) |
| Audience | Firmware engineers, hardware / EE lab use |

---

## 1. Purpose and Scope

Owl is a standalone, networked debugging instrument for firmware and hardware engineers who need deep, time-correlated visibility into digital bus traffic, analog signals, and multi-protocol systems — without requiring a host PC to run the capture engine or decode logic.

This document defines requirements for the first product in the Owl family. It covers system architecture, hardware constraints, firmware design, the web-based user interface, the decoder subsystem, network time synchronization (IEEE 1588v2 PTP), and multi-device operation. It is intended to drive all subsequent engineering work and to serve as a reference for open-source contributors.

---

## 2. Design Philosophy

- **Self-contained.** The device serves its own web UI. No cloud dependency or internet connection is required for operation.
- **Instrument-grade.** Owl is a professional debugging tool first. Ease of use is valued but not at the cost of capability.
- **Open.** All hardware design files, firmware, and software are published under open-source licenses.
- **Composable.** Multiple Owls on a network operate as a time-aligned ensemble, not a collection of isolated instruments.
- **Extensible.** The decoder subsystem accepts user-authored decoders without firmware modification.

---

## 3. Target Users

Primary users are firmware and embedded systems engineers, and hardware / EE lab engineers. Representative tasks include:

- **Board bring-up:** monitoring multiple SPI/I2C/UART buses simultaneously while verifying initialization sequences
- **Protocol debugging:** triggering on a specific I2C device address or UART byte value and capturing the surrounding context
- **Multi-board correlation:** using two or more time-synchronized Owls to capture causally related events on physically separate boards
- **Custom protocol decoding:** writing a decoder for a proprietary or niche bus (e.g., a custom SPI register map) without modifying Owl firmware

---

## 4. System Overview

Owl is built on the Raspberry Pi RP2350 microcontroller. The system is partitioned across its two Arm Cortex-M33 cores:

| Core | Responsibilities |
|---|---|
| Core 0 — Capture Core | PIO state machine management; DMA buffer orchestration; trigger evaluation; Native Decoder execution; inter-core result messaging to Core 1 |
| Core 1 — Comms Core | Zephyr RTOS; Ethernet (LAN9250 via SPI); USB device stack; WebSocket server; REST API; PTP clock discipline; User Decoder host (post-capture) |

The PIO subsystem is the heart of the capture engine. Up to eight RP2350 PIO state machines run simultaneously for digital capture, protocol-aware triggering, and Native Decoding. The internal ADC provides supplementary analog capture. A self-hosted web application provides all user interaction: configuration, live waveform visualization, decoder management, and multi-Owl coordination.

---

## 5. Hardware Specification

### 5.1 Core Processor

- Raspberry Pi RP2350 — dual Arm Cortex-M33 cores at 150 MHz, 520 KB on-chip SRAM
- Selected for: flexible PIO subsystem, dual-core partitioning capability, native USB, broad open-source toolchain support

### 5.2 Digital Input Channels

- 8 channels
- 3.3V logic native; 5V-tolerant input path via level translation (hardware design TBD)
- Input impedance target: >= 100 kΩ at probe tip
- ESD protection on all input pins
- All 8 channels must be mapped to contiguous GPIO pins for PIO input mapping

### 5.3 Analog Input

- 3 channels via RP2350 internal ADC
- 12-bit resolution, 0–3.3 V input range
- Maximum sample rate: ~500 kSPS shared across active analog channels (RP2350 ADC hardware limit)

> **Note:** The RP2350 ADC and digital capture DMA channels share the same bus fabric and must be assigned carefully to avoid throughput contention. ADC DMA channels are assigned lower priority than digital capture channels. At maximum digital capture rates, ADC sample rate may be reduced automatically.

### 5.4 Capture Memory — Tiered and Auto-Detected

| Tier | Capacity | Notes |
|---|---|---|
| On-chip SRAM (baseline, always present) | ~400 KB usable for capture | At 200 MSPS, 8ch: ~2.0 ms capture window |
| External QSPI PSRAM (optional) | Up to 8 MB (e.g., APS6404L) | Auto-detected at boot. Expands window to ~40 ms at 200 MSPS |
| SD Card (optional, stretch goal) | Effectively unlimited | Deep captures via streaming. Requires SPI arbitration with Ethernet. Adds latency. |

At boot, firmware probes for external PSRAM and SD card presence. The device reports its detected `capture_depth_ms` and `max_sample_rate` to the web UI at connect time, and the UI adapts its controls accordingly. No manual configuration is required.

> **Note:** The QSPI bus to external PSRAM may be a throughput bottleneck at high sample rates. At 200 MSPS with 8 channels the raw data rate is 1.6 Gbps (200 MB/s), which is likely to exceed the sustainable QSPI write bandwidth to an APS6404L (practical QSPI throughput to that device is on the order of 80–133 MB/s depending on clock frequency and access patterns). **Lossless compression of the sample stream prior to PSRAM writes should be evaluated** — the RP2350's DMA and/or a dedicated Core 0 compression pass may be able to reduce the effective write bandwidth requirement sufficiently at lower signal activity levels. This is an open issue (see Issue 11).

### 5.5 Ethernet — LAN9250

- Microchip LAN9250: single-chip 10/100 Ethernet MAC+PHY
- Host interface: SPI / SQI (used on dev platform; SQI preferred for bandwidth on final hardware)
- Hardware IEEE 1588v2 timestamp unit — full dedicated section in LAN9250 datasheet (Section 14)
- 16 KB internal packet buffer SRAM
- Single 3.3V supply operation

> **Note:** The LAN9250 was confirmed to include a hardware PTP timestamp unit supporting IEEE 1588v2. This is a first-class hardware capability — the PTP implementation shall use the hardware timestamp registers, not software timestamping. Accuracy expectations are sub-microsecond under controlled network conditions.

### 5.6 USB

- RP2350 native USB 1.1 Full Speed device port
- Exposed as USB CDC-ACM (virtual serial) for debug / configuration fallback
- May optionally expose USB mass storage for decoder file management

### 5.7 Development Platform

The current development platform is a Raspberry Pi Pico 2 paired with a Mikroe Eth Click board (LAN9250 via SPI). This platform is used exclusively for firmware development and validation. All firmware and software targets final Owl hardware. Pico 2-specific bring-up notes are maintained in a separate development document.

---

## 6. Firmware Architecture

### 6.1 RTOS and Framework

- Core 1: Zephyr RTOS, handling Ethernet, USB, WebSocket/REST server, and PTP daemon
- Core 0: bare-metal or lightweight cooperative scheduler for deterministic capture and trigger processing
- Inter-core communication via shared ring buffers and Zephyr IPC primitives (mailbox / message queue)
- Zephyr drivers used for LAN9250 (Ethernet), USB device, SPI, GPIO, and ADC

### 6.2 PIO Resource Allocation

The RP2350 provides two PIO blocks, each with four state machines and 32 shared instruction words. The following allocation is the initial plan; actual assignments will be validated against instruction memory constraints during development.

| State Machine(s) | PIO Block | Function |
|---|---|---|
| SM0 | PIO0 | High-speed digital capture — primary, all 8 channels via autopush DMA |
| SM1 | PIO0 | Trigger pattern matcher (pattern/sequence trigger mode) |
| SM2–SM3 | PIO0 | Native Decoder slot A (e.g., UART, SPI) |
| SM4–SM5 | PIO1 | Native Decoder slot B (e.g., I2C, MDIO) |
| SM6–SM7 | PIO1 | User Decoder PIO slot; reserved for Bus Stimulus (stretch goal) |

### 6.3 Sample Rate and Capture Depth

Sample rate is user-configurable. The PIO capture state machine clock divider is adjusted to achieve the target rate. Capture depth scales inversely with sample rate.

| Sample Rate | Data Rate (8ch) | Window (SRAM only) | Window (8 MB PSRAM) |
|---|---|---|---|
| 200 MSPS | 1.6 Gbps | ~2.0 ms | ~40 ms |
| 100 MSPS | 800 Mbps | ~4.0 ms | ~80 ms |
| 10 MSPS | 80 Mbps | ~40 ms | ~800 ms |
| 1 MSPS | 8 Mbps | ~400 ms | ~8 s |

> **Note:** 200 MSPS requires RP2350 PIO running with clock divider = 1 at 150 MHz system clock, yielding 150 MSPS per single-instruction cycle. Achieving 200 MSPS may require a 2-instruction unrolled loop with overclocking, or accepting 150 MSPS as the maximum. Validation required during firmware bring-up.

### 6.4 DMA Architecture

Digital capture uses DMA in ping-pong (double-buffer) configuration: while one buffer fills from the PIO FIFO, the other is transferred to main capture RAM and scanned for trigger conditions. Digital capture DMA channels are assigned the highest priority on the bus fabric. ADC DMA operates on a separate lower-priority channel. The inter-core interface uses a lock-free ring buffer in shared SRAM.

### 6.5 Trigger Subsystem

Trigger evaluation runs on Core 0 on the DMA output stream before data is committed to the capture buffer. Three trigger modes are supported:

| Mode | Description |
|---|---|
| Edge / Level | Rising, falling, or either edge on any channel, or high/low level on any channel. Multiple conditions may be AND-ed. |
| Pattern / Sequence | Match a multi-channel bit pattern at a single sample, or a sequence of patterns across consecutive samples. Implemented in the PIO trigger state machine (SM1) and validated in firmware. |
| Bus-Protocol-Aware | Trigger on a decoded value from an active Native Decoder: e.g., a specific I2C device address, a UART byte value, an SPI data word, or an MDIO register. Requires the corresponding Native Decoder to be active and configured. |

All trigger modes support a configurable pre-trigger buffer and post-trigger depth, expressed in the UI as a percentage of total capture depth. The trigger point is marked on the waveform timeline.

---

## 7. Decoder Subsystem

Owl uses a two-tier decoder architecture. Consistent terminology is used throughout firmware, software, and documentation:

| Term | Definition |
|---|---|
| Native Decoder | A protocol decoder implemented in PIO assembly and/or C firmware. Runs in real time on Core 0 during capture. Participates in bus-protocol-aware triggering. Ships with Owl firmware. |
| User Decoder | A protocol decoder authored by the user and loaded at runtime via the web UI. Runs post-capture. Three authoring paths are supported (see below). Does not directly participate in hardware triggering unless synthesized to PIO via the PIO Synthesis path. |

### 7.1 Native Decoders

Native Decoders ship with Owl firmware and are implemented in PIO assembly (signal framing, bit timing) combined with C firmware (value extraction, formatting, trigger integration). Initial target set:

- **UART** — configurable baud rate, data bits, parity, stop bits
- **SPI** — configurable CPOL/CPHA, bit order, word width
- **I2C** — 100k / 400k / 1M bps; address and data decode
- **MDIO / MDC** — IEEE 802.3 Clause 22 and Clause 45
- **USB-C PD** — CC line BMC encoding, basic PD message decode (full PD decode is a stretch goal)
- **1-Wire** — stretch goal

Each Native Decoder exposes configurable bus-protocol-aware trigger conditions (e.g., match address, match data byte, match read/write direction) to the trigger subsystem.

### 7.2 User Decoders — Three Authoring Paths

Users may author decoders using any of three paths. The web UI presents all three options and allows the user to choose.

#### Path A: Direct PIO Assembly

The user writes a decoder in PIO assembly syntax using the MicroPython `@rp2.asm_pio()` decorator convention. This requires knowledge of PIO programming and is the most powerful option for timing-critical protocols. The web UI provides a syntax-highlighted PIO editor, an instruction reference panel, and a program length validator (32-instruction limit). The assembled binary is loaded into PIO SM6 or SM7. Results are passed to Core 1 via inter-core message and rendered in the waveform viewer.

#### Path B: PIO Synthesis (AI-Assisted)

The user writes a high-level Python description of the protocol's electrical and logical behavior. The web UI sends this to an LLM API (browser-side API call; no credentials stored on device) and receives PIO assembly in return. The generated PIO is displayed to the user for review and optional manual editing before being loaded to the device. The user may iterate freely. This path is named **PIO Synthesis** and is a first-class named subsystem in the Owl web application.

> **Note:** PIO Synthesis requires an external LLM API only at authoring time. The resulting PIO binary is stored on the device and requires no further API access. PIO Synthesis is architecturally self-contained and may be published as a standalone open-source tool independent of Owl.

#### Path C: Post-Capture Python (Browser-Side via Pyodide)

The user writes a Python decoder that operates on already-captured, timestamped sample data. This decoder runs entirely in the web browser via the Pyodide (Python-to-WebAssembly) runtime. It receives the capture buffer as a NumPy-compatible array and may operate on raw logic values or on the output of an active Native Decoder. This path is best suited for higher-level or application-layer protocol decoding. It cannot participate in hardware triggering.

---

## 8. Web Application

### 8.1 Self-Containment and Hosting

The web application is served entirely from Owl over HTTP. All assets — HTML, CSS, JavaScript, the Pyodide WebAssembly runtime, waveform rendering engine, PIO instruction reference, and decoder editor — are stored on the device. No CDN access, cloud service, or internet connection is required during normal operation. The application must be fully functional in an air-gapped environment.

> **Note:** The Pyodide runtime is approximately 10 MB compressed. A compressed filesystem (e.g., LittleFS with transparent decompression, or a read-only squashfs-like partition) is required. Flash storage allocation must be planned in the hardware design phase to accommodate web assets, firmware, and capture storage.

### 8.2 Communication Transport

- **WebSocket:** real-time streaming of capture data, decoded annotations, and device state
- **REST API:** device configuration, arm/stop/reset control, decoder file management (upload, download, delete)
- Both served from Zephyr's HTTP server on Core 1

### 8.3 Waveform Viewer

The waveform viewer is the primary UI surface, inspired by Sigrok PulseView, Digilent Waveforms, Saleae Logic, and the Bus Pirate's direct interaction model. Required features:

- Zoomable, pannable waveform timeline for all captured channels on a common time axis
- Digital channels rendered as standard logic waveforms; configurable color and label per channel
- Analog channels overlaid on the same timeline with independent Y-axis scaling
- Decoded protocol annotations rendered as labeled overlays on the relevant digital channels (PulseView-style stacked annotation rows)
- Trigger point and pre/post-trigger window clearly indicated on the timeline
- Sample rate and current capture depth displayed at all times
- One or more time cursors with absolute PTP timestamp readout and inter-cursor delta
- Channel labeling — user-defined names stored on device
- Export: raw capture as binary or CSV; decoded annotations as CSV

### 8.4 Configuration Controls

- Sample rate — slider or stepped presets; capture depth shown in real time as rate changes
- Analog channel enable/disable and Y-scale
- Trigger mode selection and all trigger parameters
- Pre/post-trigger buffer split
- Native Decoder configuration: protocol selection and all bus parameters
- User Decoder management: upload, assign to channels, enable/disable, PIO Synthesis interface
- Multi-Owl group configuration (Section 10)

---

## 9. Network Time Synchronization (IEEE 1588v2 PTP)

### 9.1 Purpose

When multiple Owls are deployed simultaneously, all capture timestamps must reference a common time base so that waveforms from different devices can be time-aligned in the web application. IEEE 1588v2 Precision Time Protocol (PTP) provides this capability.

### 9.2 LAN9250 Hardware Timestamp Unit

The LAN9250 Ethernet controller includes a dedicated IEEE 1588v2 hardware timestamp unit (Section 14 of the LAN9250 datasheet), confirmed present. This unit timestamps PTP event frames at the MAC/PHY boundary, providing hardware-assisted synchronization accuracy well below 1 microsecond under controlled network conditions. The PTP implementation on Core 1 shall use the LAN9250 hardware timestamp registers for all Sync and Delay_Req frame timestamping.

### 9.3 PTP Stack

PTP runs as a Zephyr application on Core 1. A software servo loop disciplines the RP2350 timer to the PTP master. Supported topologies:

- Two Owls on the same bench LAN (direct cable or unmanaged switch)
- Multiple Owls distributed across a system under test on a shared LAN
- Owls as PTP slaves to an existing lab PTP grandmaster clock
- One Owl acting as PTP master for a group using the IEEE 1588 Best Master Clock (BMC) algorithm when no external grandmaster is present

### 9.4 Network Considerations

For sub-microsecond synchronization in multi-Owl deployments, a PTP-aware Ethernet switch (transparent clock or boundary clock) is strongly recommended. A standard unmanaged switch introduces non-deterministic store-and-forward delay that degrades PTP accuracy to approximately 10–100 microseconds under load. The web application shall display an estimated inter-device time uncertainty metric when Owls are grouped, derived from PTP servo lock quality.

---

## 10. Multi-Owl Operation

When multiple Owls are present on the same network, the web application running on any one device can discover and group the others. Multi-Owl capabilities:

- Zero-configuration device discovery via mDNS / DNS-SD (Zephyr mdns)
- Common capture configuration pushed from a single controlling UI to all grouped devices simultaneously
- Synchronized arm command initiates capture across all grouped devices
- Capture data from all devices streamed to the controlling web application and displayed on a merged timeline using PTP-derived timestamps for alignment
- Inter-device time uncertainty displayed based on PTP servo lock quality

---

## 11. Bus Stimulus (Stretch Goal)

The ability to drive bus signals out from Owl onto the device under test is a stretch goal for the first hardware revision. No requirements are defined at this time. PIO state machines SM6 and SM7 in PIO1 are reserved for this function (Section 6.2). The implementation strategy — scripted sequences, interactive web UI control, or a combination — is deferred pending further definition.

---

## 12. Open Source

Owl is fully open source. The following will be published:

- **Hardware:** schematics, PCB layout, BOM, manufacturing files
- **Firmware:** all C source, PIO assembly, Zephyr configuration, and build system files
- **Software:** web application source, decoder examples, PIO Synthesis tooling

Specific license selections (CERN-OHL variants for hardware; Apache 2.0, GPL, or MIT for software components) are TBD and will be determined before the first public repository commit. The PIO Synthesis subsystem will be published as a standalone tool in addition to its inclusion in Owl.

---

## 13. Open Issues and Decisions Required

| # | Issue | Notes / Action Required |
|---|---|---|
| 1 | Confirm achievable maximum sample rate | RP2350 PIO at 150 MHz with divider=1 gives 150 MSPS. 200 MSPS requires overclocking or a 2-instruction unrolled capture pattern. Must be validated during firmware bring-up. |
| 2 | PIO instruction memory budget audit | Capture SM + trigger SM + two Native Decoders must fit in two PIO blocks (64 total instructions). Instruction counts must be tracked during decoder implementation. |
| 3 | Zephyr LAN9250 driver PTP support | The existing Zephyr LAN9250 Ethernet driver must be evaluated for IEEE 1588 hardware timestamp register access. Driver patches are likely required. |
| 4 | Waveform rendering library selection | A suitable open-source waveform renderer (or custom canvas/WebGL) must be selected and evaluated for performance at high sample rates. Candidates: custom renderer, adaptation of existing logic analyzer UI code. |
| 5 | Web asset storage and compression | Pyodide runtime is ~10 MB compressed. Flash partition layout must accommodate web assets, firmware, Native Decoder binaries, User Decoder storage, and capture buffer (for SRAM-only configurations). LittleFS or similar required. |
| 6 | PIO Synthesis LLM API selection | API provider for PIO Synthesis must be selected. Claude (Anthropic) is a strong candidate. Browser-side API key handling, user credential storage, and offline fallback behavior must be defined. |
| 7 | Input voltage protection design | RP2350 GPIO is 3.3V native. A 5V-tolerant input path (level translator or buffer selection), ESD protection scheme, and probe interface connector must be defined during hardware design. |
| 8 | Bus stimulus scope definition | No requirements currently. Owner needed to define use cases, interface requirements, and timing goals before HW Rev 2 planning. |
| 9 | Open source license selection | CERN-OHL-S vs. CERN-OHL-P for hardware; GPL vs. Apache 2.0 vs. MIT for firmware/software. Decision required before first public commit. |
| 10 | SPI vs. SQI for LAN9250 | Dev platform uses SPI. Final hardware should use SQI (4-bit) mode for higher Ethernet throughput headroom, especially during high-speed digital capture. SQI driver support in Zephyr must be verified. |
| 11 | QSPI PSRAM write bandwidth and compression | At 200 MSPS (8ch), raw sample data rate is 200 MB/s — likely exceeding sustainable QSPI write throughput to external PSRAM (~80–133 MB/s). Evaluate lossless sample-stream compression (e.g., run-length encoding of idle/repeated patterns) applied on Core 0 before PSRAM writes. Characterize effective compression ratio vs. signal activity level and determine whether PSRAM is viable at maximum sample rate or only at reduced rates. |

---

## 14. Revision History

| Rev | Date | Author | Summary |
|---|---|---|---|
| 0.1 | 2026-03-06 | (TBD) | Initial draft. Covers system architecture, capture engine, trigger subsystem, two-tier decoder system (Native Decoders + User Decoders with PIO Synthesis), IEEE 1588v2 PTP via LAN9250 hardware timestamps, multi-Owl operation, and web application. |
