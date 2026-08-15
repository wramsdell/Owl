/*
 * Copyright (c) 2026 Ward Ramsdell
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(owl_main, LOG_LEVEL_INF);

/*
 * Dual-core partitioning (per product spec owl_spec_v0_1):
 *
 * Core 0 (Capture Core) - started first via Kconfig SMP configuration:
 *   - PIO state machine management
 *   - DMA buffer orchestration
 *   - Trigger evaluation
 *   - Native Decoder execution
 *   - Inter-core messaging
 *
 * Core 1 (Comms Core) - Zephyr RTOS initialized here:
 *   - Ethernet (LAN9250 via SPI)
 *   - USB device stack
 *   - WebSocket server
 *   - REST API
 *   - PTP clock discipline
 */

/* Core 0 entry point: capture subsystem initialization */
static void core0_capture_init(const struct device *dev)
{
	LOG_INF("Core 0: Capture subsystem initialized");
	/*
	 * TODO: Initialize PIO state machines for digital input channels
	 * TODO: Set up DMA buffer ring for sample data
	 * TODO: Configure trigger evaluation engine
	 */
	while (1) {
		k_sleep(K_MSEC(1000));
		LOG_DBG("Core 0: Capture core running");
	}
}

/* Core 1 entry point: communications subsystem initialization */
static void core1_comms_init(const struct device *dev)
{
	LOG_INF("Core 1: Comms subsystem initialized");
	/*
	 * TODO: Initialize LAN9250 Ethernet controller (SPI vs SQI TBD)
	 * TODO: Bring up USB device stack for host connection
	 * TODO: Start WebSocket server (ws://<device-ip>/ws)
	 * TODO: Start REST API server on Zephyr HTTP listener
	 * TODO: Initialize PTP clock discipline using LAN9250 HW timestamp unit
	 */

	/* Placeholder: ping every second until comms stack is wired up */
	while (1) {
		k_sleep(K_MSEC(1000));
		LOG_DBG("Core 1: Comms core running");
	}
}

/*
 * SMP dual-core configuration:
 * - Core 0 boots first and runs the capture subsystem
 * - Core 1 boots after core0_capture_init completes
 * This matches the product spec's partitioning model.
 */
K_CORE_DEFINE(core0_capture_init, "core0", 7, K_BOOT_PRIORITY_DEFAULT);
K_CORE_DEFINE(core1_comms_init, "core1", 8, K_BOOT_PRIORITY_DEFAULT);

int main(void)
{
	/*
	 * Zephyr SMP kernel manages core lifecycle.
	 * Application code runs on Core 1 (comms).
	 * Core 0 (capture) is initialized via K_CORE_DEFINE above.
	 */
	LOG_INF("Owl oscilloscope firmware starting...");
	LOG_INF("Platform: RP2350 dual-core Cortex-M33 @ 150 MHz");

	return 0;
}
