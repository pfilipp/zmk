/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#define SPLIT_MODE_DONGLE_CMD 0
#define SPLIT_MODE_HOST_CMD 1

/* &split_mode SM_DONGLE      -> become a split peripheral of the dongle */
/* &split_mode SM_HOST <n>    -> become the split central and talk to host profile n */
#define SM_DONGLE SPLIT_MODE_DONGLE_CMD 0
#define SM_HOST SPLIT_MODE_HOST_CMD
