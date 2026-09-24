/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_split_mode

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>
#include <dt-bindings/zmk/split_mode.h>
#include <zmk/behavior.h>
#include <zmk/split/role.h>
#include <zmk/events/position_state_changed.h>

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_DYNAMIC)
#include <zmk/ble.h>
#endif

#if IS_ENABLED(CONFIG_ZMK_SPLIT_CENTRAL_MODE_GATE)
#include <zmk/split/central_mode_gate.h>
#endif

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_DYNAMIC)

/* Time for the settings write and any in-flight split command to complete before we drop links. */
#define SWITCH_DELAY_MS 100
/* Time for the disconnects to go out on air before the reboot. */
#define REBOOT_DELAY_MS 300

static void reboot_work_cb(struct k_work *work) {
    LOG_INF("Rebooting into new split mode");
    sys_reboot(SYS_REBOOT_WARM);
}

static K_WORK_DELAYABLE_DEFINE(reboot_work, reboot_work_cb);

static void disconnect_conn(struct bt_conn *conn, void *data) {
    bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
}

static void switch_work_cb(struct k_work *work) {
    /* Block every advertising/scanning restart that the disconnects would otherwise trigger. */
    zmk_split_role_set_switch_pending();
    bt_conn_foreach(BT_CONN_TYPE_LE, disconnect_conn, NULL);
    k_work_schedule(&reboot_work, K_MSEC(REBOOT_DELAY_MS));
}

static K_WORK_DELAYABLE_DEFINE(switch_work, switch_work_cb);

static int handle_pressed(struct zmk_behavior_binding *binding,
                          struct zmk_behavior_binding_event event) {
    enum zmk_split_mode requested = (binding->param1 == SPLIT_MODE_DONGLE_CMD)
                                        ? ZMK_SPLIT_MODE_DONGLE
                                        : ZMK_SPLIT_MODE_STANDALONE;

    if (binding->param1 == SPLIT_MODE_HOST_CMD) {
        int err = zmk_ble_prof_select_persist(binding->param2);
        if (err) {
            LOG_ERR("Failed to select host profile %d (%d)", binding->param2, err);
        }
    }

    if (requested == zmk_split_role_get_mode()) {
        LOG_DBG("Already in split mode %d", requested);
        return ZMK_BEHAVIOR_OPAQUE;
    }

    if (k_work_delayable_is_pending(&switch_work) || k_work_delayable_is_pending(&reboot_work)) {
        LOG_DBG("Mode switch already in progress");
        return ZMK_BEHAVIOR_OPAQUE;
    }

    int err = zmk_split_role_set_mode(requested);
    if (err) {
        LOG_ERR("Failed to persist split mode %d (%d)", requested, err);
        return ZMK_BEHAVIOR_OPAQUE;
    }

    LOG_INF("Split mode -> %s, rebooting shortly",
            requested == ZMK_SPLIT_MODE_DONGLE ? "dongle" : "standalone");
    k_work_schedule(&switch_work, K_MSEC(SWITCH_DELAY_MS));
    return ZMK_BEHAVIOR_OPAQUE;
}

#elif IS_ENABLED(CONFIG_ZMK_SPLIT_CENTRAL_MODE_GATE)

static int handle_pressed(struct zmk_behavior_binding *binding,
                          struct zmk_behavior_binding_event event) {
    if (binding->param1 != SPLIT_MODE_HOST_CMD) {
        /* Back to dongle mode is signalled by the standalone half reconnecting to us. */
        return ZMK_BEHAVIOR_OPAQUE;
    }

    if (event.source == ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL) {
        LOG_WRN("split_mode pressed on the dongle itself; ignoring");
        return ZMK_BEHAVIOR_OPAQUE;
    }

    int err = zmk_split_central_mode_gate_enter_standalone(event.source);
    if (err) {
        LOG_ERR("Failed to enter standalone mode (%d)", err);
    }
    return ZMK_BEHAVIOR_OPAQUE;
}

#else

static int handle_pressed(struct zmk_behavior_binding *binding,
                          struct zmk_behavior_binding_event event) {
    /* Plain peripheral (the right half): the centrals handle the switch. */
    return ZMK_BEHAVIOR_OPAQUE;
}

#endif

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    return handle_pressed(binding, event);
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_split_mode_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
    .locality = BEHAVIOR_LOCALITY_GLOBAL,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &behavior_split_mode_driver_api);

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
