/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <zephyr/settings/settings.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/logging/log.h>

#include <zmk/ble.h>
#include <zmk/split/role.h>

/* `depends on SETTINGS` in Kconfig would close a dependency loop through ZMK_USB, so assert here. */
BUILD_ASSERT(IS_ENABLED(CONFIG_SETTINGS), "ZMK_SPLIT_ROLE_DYNAMIC requires CONFIG_SETTINGS");

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static uint8_t current_mode = ZMK_SPLIT_MODE_DONGLE;
static bt_addr_le_t dongle_addr;
static bool dongle_addr_known = false;
static bool switch_pending = false;

bool zmk_split_role_is_central(void) { return current_mode == ZMK_SPLIT_MODE_STANDALONE; }

enum zmk_split_mode zmk_split_role_get_mode(void) { return current_mode; }

int zmk_split_role_set_mode(enum zmk_split_mode mode) {
    current_mode = mode;
    return settings_save_one("split/mode", &current_mode, sizeof(current_mode));
}

const bt_addr_le_t *zmk_split_role_dongle_addr(void) {
    return dongle_addr_known ? &dongle_addr : NULL;
}

int zmk_split_role_set_dongle_addr(const bt_addr_le_t *addr) {
    bt_addr_le_copy(&dongle_addr, addr);
    dongle_addr_known = true;
    return settings_save_one("split/dongle_addr", &dongle_addr, sizeof(dongle_addr));
}

void zmk_split_role_note_central_conn(struct bt_conn *conn) {
    const bt_addr_le_t *peer = bt_conn_get_dst(conn);

    if (dongle_addr_known || zmk_ble_profile_index(peer) >= 0) {
        return;
    }

    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(peer, addr, sizeof(addr));
    LOG_INF("Recording %s as the dongle", addr);
    zmk_split_role_set_dongle_addr(peer);
}

bool zmk_split_role_switch_pending(void) { return switch_pending; }

void zmk_split_role_set_switch_pending(void) { switch_pending = true; }

static int role_settings_set(const char *name, size_t len, settings_read_cb read_cb,
                             void *cb_arg) {
    const char *next;

    if (settings_name_steq(name, "mode", &next) && !next) {
        if (len != sizeof(current_mode)) {
            return -EINVAL;
        }
        int rc = read_cb(cb_arg, &current_mode, sizeof(current_mode));
        if (rc < 0) {
            return rc;
        }
        LOG_INF("Split mode: %s", zmk_split_role_is_central() ? "standalone (central)"
                                                                : "dongle (peripheral)");
    } else if (settings_name_steq(name, "dongle_addr", &next) && !next) {
        if (len != sizeof(dongle_addr)) {
            return -EINVAL;
        }
        int rc = read_cb(cb_arg, &dongle_addr, sizeof(dongle_addr));
        if (rc < 0) {
            return rc;
        }
        dongle_addr_known = true;
    }

    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(zmk_split_role, "split", NULL, role_settings_set, NULL, NULL);
