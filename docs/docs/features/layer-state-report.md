---
title: Layer State Report
---

ZMK can send the active layer index and lock state to the host over a custom HID input report, enabling host-side applications to react to layer changes in real time (e.g. displaying an on-screen indicator or switching application profiles).

## Enabling

To enable the layer state report, set `CONFIG_ZMK_HID_LAYER_STATE_REPORT=y` in your board or shield configuration.

:::warning

When enabling the feature, changes are made to the HID report descriptor which some hosts may not pick up automatically over BLE. Be sure to [remove and re-pair to your hosts](bluetooth.md#refreshing-the-hid-descriptor) once you enable the feature.

This feature uses a **vendor-specific HID usage page** (`0xFF00`). All operating systems will receive the report, but none will act on it natively — a host-side companion application is required to read and interpret the data.

:::

## HID Report Protocol

The layer state report uses the following HID descriptor structure:

| Field      | Value                          |
| ---------- | ------------------------------ |
| Usage Page | `0xFF00` (vendor-defined)      |
| Report ID  | `0x04`                         |

The report body is 2 bytes:

| Byte | Field    | Type   | Description                                              |
| ---- | -------- | ------ | -------------------------------------------------------- |
| 0    | `layer`  | uint8  | Highest active layer index                               |
| 1    | `locked` | uint8  | `1` if the layer is toggled/locked, `0` otherwise        |

## Host-Side Implementation

Because the report uses a vendor-specific usage page, you need a companion application on the host to read it. Below are minimal examples for each major platform.

### Python (cross-platform)

Using the [`hidapi`](https://pypi.org/project/hidapi/) library:

```python
import hid

ZMK_VENDOR_ID = 0x1D50  # adjust to your board's VID
ZMK_PRODUCT_ID = 0x615E  # adjust to your board's PID
REPORT_ID = 0x04

device = hid.device()
device.open(ZMK_VENDOR_ID, ZMK_PRODUCT_ID)
device.set_nonblocking(False)

print("Listening for layer state reports...")
while True:
    data = device.read(64)
    if data and data[0] == REPORT_ID:
        layer = data[1]
        locked = data[2]
        print(f"Layer: {layer}, Locked: {bool(locked)}")
```

### macOS (Swift)

Using `IOKit`'s HID Manager:

```swift
import IOKit.hid

let manager = IOHIDManagerCreate(kCFAllocatorDefault, IOOptionBits(kIOHIDOptionsTypeNone))
IOHIDManagerSetDeviceMatchingMultiple(manager, nil)
IOHIDManagerScheduleWithRunLoop(manager, CFRunLoopGetCurrent(), CFRunLoopMode.defaultMode.rawValue)
IOHIDManagerOpen(manager, IOOptionBits(kIOHIDOptionsTypeNone))

let callback: IOHIDReportCallback = { context, result, sender, type, reportID, report, reportLength in
    guard reportID == 0x04, reportLength >= 2 else { return }
    let layer = report[0]
    let locked = report[1]
    print("Layer: \(layer), Locked: \(locked != 0)")
}

if let devices = IOHIDManagerCopyDevices(manager) as? Set<IOHIDDevice> {
    for device in devices {
        let report = UnsafeMutablePointer<UInt8>.allocate(capacity: 64)
        IOHIDDeviceRegisterInputReportCallback(device, report, 64, callback, nil)
    }
}

CFRunLoopRun()
```

### Linux (C)

Reading from `/dev/hidraw`:

```c
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(void) {
    int fd = open("/dev/hidraw0", O_RDONLY);  /* adjust device path */
    if (fd < 0) { perror("open"); return 1; }

    unsigned char buf[3];
    while (read(fd, buf, sizeof(buf)) > 0) {
        if (buf[0] == 0x04) {
            printf("Layer: %u, Locked: %u\n", buf[1], buf[2]);
        }
    }

    close(fd);
    return 0;
}
```

### Windows (C)

Using the Windows HID API:

```c
#include <windows.h>
#include <hidsdi.h>
#include <setupapi.h>
#include <stdio.h>

int main(void) {
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO devInfo = SetupDiGetClassDevs(&hidGuid, NULL, NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    /* Enumerate devices and open the matching one (VID/PID check omitted for brevity) */
    SP_DEVICE_INTERFACE_DATA ifData = { .cbSize = sizeof(ifData) };
    SetupDiEnumDeviceInterfaces(devInfo, NULL, &hidGuid, 0, &ifData);

    DWORD reqSize;
    SetupDiGetDeviceInterfaceDetail(devInfo, &ifData, NULL, 0, &reqSize, NULL);
    PSP_DEVICE_INTERFACE_DETAIL_DATA detail = malloc(reqSize);
    detail->cbSize = sizeof(*detail);
    SetupDiGetDeviceInterfaceDetail(devInfo, &ifData, detail, reqSize, NULL, NULL);

    HANDLE dev = CreateFile(detail->DevicePath, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    free(detail);

    unsigned char buf[3];
    DWORD bytesRead;
    while (ReadFile(dev, buf, sizeof(buf), &bytesRead, NULL) && bytesRead > 0) {
        if (buf[0] == 0x04) {
            printf("Layer: %u, Locked: %u\n", buf[1], buf[2]);
        }
    }

    CloseHandle(dev);
    SetupDiDestroyDeviceInfoList(devInfo);
    return 0;
}
```

## Configuration

| Config                                        | Description                                     | Default |
| --------------------------------------------- | ----------------------------------------------- | ------- |
| `CONFIG_ZMK_HID_LAYER_STATE_REPORT`           | Enable the layer state HID report               | n       |
| `CONFIG_ZMK_BLE_LAYER_STATE_REPORT_QUEUE_SIZE`| Max queued layer state reports for BLE           | 5       |

:::note

This feature depends on `!ZMK_SPLIT || ZMK_SPLIT_ROLE_CENTRAL` — on split keyboards, only the central half sends the report.

:::
