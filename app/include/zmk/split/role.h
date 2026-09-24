/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/sys/util_macro.h>
#include <zephyr/bluetooth/addr.h>

enum zmk_split_mode {
    ZMK_SPLIT_MODE_DONGLE = 0,     /* this half is a split peripheral; a dongle is the central */
    ZMK_SPLIT_MODE_STANDALONE = 1, /* this half is the split central and talks to hosts */
};

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_DYNAMIC)

bool zmk_split_role_is_central(void);
enum zmk_split_mode zmk_split_role_get_mode(void);
/* Persists synchronously. Takes effect on the next boot. */
int zmk_split_role_set_mode(enum zmk_split_mode mode);

/* Address of the dongle this half bonded to while in dongle mode, or NULL if unknown. */
const bt_addr_le_t *zmk_split_role_dongle_addr(void);
int zmk_split_role_set_dongle_addr(const bt_addr_le_t *addr);

#else

static inline bool zmk_split_role_is_central(void) {
    return !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL);
}

#endif /* IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_DYNAMIC) */
