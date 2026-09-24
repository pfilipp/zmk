/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/logging/log.h>

#include <zmk/ble.h>
#include <zmk/split/role.h>
#include <zmk/split/central_mode_gate.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* `depends on SETTINGS` in Kconfig would close a dependency loop through ZMK_USB, so assert here. */
BUILD_ASSERT(IS_ENABLED(CONFIG_SETTINGS), "ZMK_SPLIT_CENTRAL_MODE_GATE requires CONFIG_SETTINGS");

#define LEFT_SLOT_UNKNOWN 0xFF

/* Let the forwarded split command reach the peripherals before we drop them. */
#define DISCONNECT_DELAY_MS 500

static uint8_t mode = ZMK_SPLIT_MODE_DONGLE;
static uint8_t left_slot = LEFT_SLOT_UNKNOWN;

static int persist(void) {
    /* Write left_slot before mode: if we fail partway through, a stale/unknown slot with
     * mode=standalone is safe (allows() treats an unknown slot as an open gate), while a new
     * slot with the old mode is also harmless either way. */
    int err = settings_save_one("split/left_slot", &left_slot, sizeof(left_slot));
    if (err) {
        return err;
    }
    return settings_save_one("split/mode", &mode, sizeof(mode));
}

bool zmk_split_central_mode_gate_allows(const bt_addr_le_t *addr) {
    if (mode != ZMK_SPLIT_MODE_STANDALONE || left_slot == LEFT_SLOT_UNKNOWN) {
        return true;
    }

    const bt_addr_le_t *left = zmk_ble_peripheral_addr(left_slot);
    if (left == NULL || bt_addr_le_cmp(left, BT_ADDR_LE_ANY) == 0) {
        /* Slot address unknown (e.g. bonds cleared): do not lock everyone out. */
        return true;
    }
    return bt_addr_le_cmp(addr, left) == 0;
}

static void disconnect_peripheral(struct bt_conn *conn, void *data) {
    struct bt_conn_info info;

    if (bt_conn_get_info(conn, &info) == 0 && info.role == BT_CONN_ROLE_CENTRAL) {
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }
}

static void disconnect_work_cb(struct k_work *work) {
    bt_conn_foreach(BT_CONN_TYPE_LE, disconnect_peripheral, NULL);
}

static K_WORK_DELAYABLE_DEFINE(disconnect_work, disconnect_work_cb);

int zmk_split_central_mode_gate_enter_standalone(uint8_t slot) {
    mode = ZMK_SPLIT_MODE_STANDALONE;
    left_slot = slot;

    /* Runtime state is authoritative for this session; a failed persist only affects the next boot. */
    int err = persist();
    if (err) {
        LOG_ERR("Failed to persist standalone mode (%d)", err);
    }

    LOG_INF("Peripheral in slot %d goes standalone; releasing peripherals", slot);
    k_work_schedule(&disconnect_work, K_MSEC(DISCONNECT_DELAY_MS));
    return err;
}

void zmk_split_central_mode_gate_peripheral_connected(int slot) {
    if (k_work_delayable_is_pending(&disconnect_work)) {
        /* The standalone half re-advertised to us while we were still dropping links; a
         * reconnect now does not mean it came back. Repeated SM_HOST presses also land here. */
        LOG_DBG("Ignoring peripheral reconnect during the switch to standalone");
        return;
    }

    if (mode == ZMK_SPLIT_MODE_STANDALONE && slot >= 0 && slot == left_slot) {
        mode = ZMK_SPLIT_MODE_DONGLE;
        LOG_INF("Standalone peripheral is back; dongle mode");
        int err = persist();
        if (err) {
            LOG_ERR("Failed to persist dongle mode (%d)", err);
        }
    }
}

static int gate_settings_set(const char *name, size_t len, settings_read_cb read_cb,
                             void *cb_arg) {
    const char *next;
    uint8_t *target = NULL;

    if (settings_name_steq(name, "mode", &next) && !next) {
        target = &mode;
    } else if (settings_name_steq(name, "left_slot", &next) && !next) {
        target = &left_slot;
    }

    if (target) {
        if (len != sizeof(*target)) {
            return -EINVAL;
        }
        int rc = read_cb(cb_arg, target, sizeof(*target));
        if (rc < 0) {
            return rc;
        }
    }

    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(zmk_split_central_mode_gate, "split", NULL, gate_settings_set, NULL,
                               NULL);
