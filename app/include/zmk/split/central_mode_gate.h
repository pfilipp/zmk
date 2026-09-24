/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/bluetooth/addr.h>

/* True when the central may connect to a peripheral advertising from addr. */
bool zmk_split_central_mode_gate_allows(const bt_addr_le_t *addr);

/* The peripheral in slot left_slot asked to become standalone: remember it, refuse the others. */
int zmk_split_central_mode_gate_enter_standalone(uint8_t left_slot);

/* A peripheral connected into slot; if it is the standalone one, we are back in dongle mode. */
void zmk_split_central_mode_gate_peripheral_connected(int slot);
