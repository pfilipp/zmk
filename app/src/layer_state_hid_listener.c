/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/hid.h>
#include <zmk/endpoints.h>
#include <zmk/keymap.h>

static int layer_state_hid_listener(const zmk_event_t *eh) {
    zmk_keymap_layer_index_t index = zmk_keymap_highest_layer_active();
    zmk_hid_layer_state_set(index, zmk_keymap_layer_locked(
        zmk_keymap_layer_index_to_id(index)) ? 1 : 0);
    return zmk_endpoint_send_layer_state_report();
}

ZMK_LISTENER(layer_state_hid_listener, layer_state_hid_listener);
ZMK_SUBSCRIPTION(layer_state_hid_listener, zmk_layer_state_changed);
