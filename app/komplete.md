https://github.com/hugovangalen/komplementary-kontrol
https://github.com/GoaSkin/qKontrol

```
$ cat /etc/udev/rules.d/99-joost-komplete.rules 
SUBSYSTEM=="usb", ATTRS{idVendor}=="17cc", ATTRS{idProduct}=="1620", MODE="0660", GROUP="joostn"
KERNEL=="hiddev*", ATTRS{idVendor}=="17cc", ATTRS{idProduct}=="1620", MODE="0660", GROUP="joostn"
```

```
hexdump -ve '1/1 "%.2x "' /sys/bus/hid/devices/0003\:17CC\:1620.000E/report_descriptor
06 01 ff 09 00 a1 01 09 01 a1 02 85 01 09 02 15 00 25 01 75 01 95 48 81 02 09 06 15 00 26 e7 03 75 10 95 08 81 02 09 09 15 00 26 ff 03 75 10 95 02 81 02 09 03 15 00 25 0f 75 04 95 02 81 02 09 0c 15 00 26 ff 00 75 08 95 01 81 02 c0 09 01 a1 02 85 02 09 08 15 00 26 ff 07 75 10 95 02 81 02 09 44 15 00 27 ff ff 00 00 75 10 95 03 81 02 c0 09 01 a1 02 85 0f 09 a2 15 00 26 ff 00 75 08 95 01 81 02 c0 09 40 a1 02 85 40 09 41 15 00 27 ff ff 00 00 75 10 95 0f 81 02 c0 09 80 a1 02 85 80 09 81 15 00 25 7f 75 08 95 45 91 02 c0 09 80 a1 02 85 81 09 81 15 00 25 7f 75 08 95 3d 91 02 c0 09 a0 a1 02 85 a0 09 a2 15 00 26 ff 00 75 08 95 01 91 02 09 a2 15 00 26 ff 00 75 08 95 01 91 02 c0 09 a0 a1 02 85 a1 09 a2 15 00 26 ff 00 75 08 95 60 91 02 09 a2 15 00 26 ff 00 75 08 95 60 91 02 09 a2 15 00 25 1f 75 08 95 08 91 02 09 a2 15 00 27 ff ff 00 00 75 10 95 01 91 02 09 a2 15 00 26 ff 00 75 08 95 01 91 02 c0 09 a0 a1 02 85 a2 09 a2 15 00 26 ff 00 75 08 95 0c 91 02 09 a2 15 00 26 ff 00 75 08 95 0c 91 02 09 a2 15 00 26 ff 00 75 08 95 14 91 02 c0 09 a0 a1 02 85 a3 09 a2 15 00 26 ff 00 75 08 95 0c 91 02 09 a2 15 00 26 ff 00 75 08 95 0c 91 02 09 a2 15 00 26 ff 00 75 08 95 0c 91 02 09 a2 15 00 26 ff 00 75 08 95 0c 91 02 09 a2 15 00 26 ff 00 75 08 95 0c 91 02 09 a2 15 00 26 ff 00 75 08 95 0c 91 02 c0 09 a0 a1 02 85 a4 09 a2 15 00 26 ff 00 75 08 95 80 91 02 c0 09 a0 a1 02 85 aa 09 a2 15 00 27 ff ff 00 00 75 10 95 19 81 02 c0 09 a0 a1 02 85 af 09 a2 15 00 26 ff 00 75 08 95 01 91 02 09 a2 15 00 26 ff 00 75 08 95 01 91 02 c0 09 d0 a1 02 85 d0 09 d1 15 00 27 ff ff 00 00 75 10 95 02 b1 02 09 d1 15 00 26 ff 00 75 08 95 01 b1 02 09 e7 15 00 25 64 75 08 95 01 b1 02 09 81 15 00 25 7f 75 08 95 01 b1 02 09 d1 15 00 26 ff 00 75 08 95 01 b1 02 09 e6 15 00 25 64 75 08 95 06 b1 02 09 d1 15 00 26 ff 00 75 08 95 02 b1 02 09 d1 15 00 26 ff 00 75 08 95 10 b1 02 c0 09 d8 a1 02 85 d8 09 d1 15 00 27 ff ff 00 00 75 10 95 02 b1 03 09 d1 15 00 27 ff ff 00 00 75 10 95 02 b1 03 09 d1 15 00 27 ff ff 00 00 75 10 95 04 b1 03 09 d1 15 00 26 ff 00 75 08 95 01 b1 03 09 d1 15 00 26 ff 00 75 08 95 0f b1 03 c0 09 d8 a1 02 85 d9 09 d1 15 00 26 ff 00 75 08 95 20 b1 03 c0 09 f0 a1 02 85 f0 09 06 15 00 26 ff 00 75 08 95 08 b1 02 09 09 15 00 26 ff 00 75 08 95 02 b1 02 09 08 15 00 26 ff 00 75 08 95 02 b1 02 c0 09 f0 a1 02 85 f1 09 81 15 00 25 7f 75 08 95 d0 b1 02 c0 09 f0 a1 02 85 f3 09 81 15 00 25 7f 75 08 95 01 91 02 c0 09 f0 a1 02 85 f4 09 f4 15 00 26 ff 00 75 08 95 01 91 02 09 f5 15 00 26 ff 00 75 08 95 1f 91 02 c0 09 e0 a1 02 85 f8 09 e1 15 00 27 ff ff 00 00 75 10 95 01 b1 03 09 e2 15 00 27 ff ff 00 00 75 10 95 01 b1 03 09 e3 15 00 26 ff 00 75 08 95 01 b1 03 09 e4 15 00 26 ff 00 75 08 95 01 b1 03 09 e5 15 00 26 ff 00 75 08 95 01 b1 03 09 e7 15 00 25 64 75 08 95 01 b1 02 09 e6 15 00 25 64 75 08 95 01 b1 02 09 e8 15 00 25 01 75 08 95 01 b1 02 c0 09 e0 a1 02 85 f9 09 e1 15 00 27 ff ff 00 00 75 10 95 01 b1 03 09 e2 15 00 27 ff ff 00 00 75 10 95 01 b1 03 09 e3 15 00 26 ff 00 75 08 95 01 b1 03 09 e4 15 00 26 ff 00 75 08 95 01 b1 03 09 e5 15 00 26 ff 00 75 08 95 01 b1 03 09 e7 15 00 25 64 75 08 95 01 b1 02 09 e6 15 00 25 64 75 08 95 01 b1 02 09 e8 15 00 25 01 75 08 95 01 b1 02 c0 c0
```

```
0x06, 0x01, 0xFF,  // Usage Page (Vendor Defined 0xFF01)
0x09, 0x00,        // Usage (0x00)
0xA1, 0x01,        // Collection (Application)
0x09, 0x01,        //   Usage (0x01)
0xA1, 0x02,        //   Collection (Logical)
0x85, 0x01,        //     Report ID (1)
0x09, 0x02,        //     Usage (0x02)
0x15, 0x00,        //     Logical Minimum (0)
0x25, 0x01,        //     Logical Maximum (1)
0x75, 0x01,        //     Report Size (1)
0x95, 0x48,        //     Report Count (72)
0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0x09, 0x06,        //     Usage (0x06)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xE7, 0x03,  //     Logical Maximum (999)
0x75, 0x10,        //     Report Size (16)
0x95, 0x08,        //     Report Count (8)
0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0x09, 0x09,        //     Usage (0x09)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x03,  //     Logical Maximum (1023)
0x75, 0x10,        //     Report Size (16)
0x95, 0x02,        //     Report Count (2)
0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0x09, 0x03,        //     Usage (0x03)
0x15, 0x00,        //     Logical Minimum (0)
0x25, 0x0F,        //     Logical Maximum (15)
0x75, 0x04,        //     Report Size (4)
0x95, 0x02,        //     Report Count (2)
0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0x09, 0x0C,        //     Usage (0x0C)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x01,        //     Report Count (1)
0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0xC0,              //   End Collection
0x09, 0x01,        //   Usage (0x01)
0xA1, 0x02,        //   Collection (Logical)
0x85, 0x02,        //     Report ID (2)
0x09, 0x08,        //     Usage (0x08)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x07,  //     Logical Maximum (2047)
0x75, 0x10,        //     Report Size (16)
0x95, 0x02,        //     Report Count (2)
0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0x09, 0x44,        //     Usage (0x44)
0x15, 0x00,        //     Logical Minimum (0)
0x27, 0xFF, 0xFF, 0x00, 0x00,  //     Logical Maximum (65534)
0x75, 0x10,        //     Report Size (16)
0x95, 0x03,        //     Report Count (3)
0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0xC0,              //   End Collection
0x09, 0x01,        //   Usage (0x01)
0xA1, 0x02,        //   Collection (Logical)
0x85, 0x0F,        //     Report ID (15)
0x09, 0xA2,        //     Usage (0xA2)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x01,        //     Report Count (1)
0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0xC0,              //   End Collection
0x09, 0x40,        //   Usage (0x40)
0xA1, 0x02,        //   Collection (Logical)
0x85, 0x40,        //     Report ID (64)
0x09, 0x41,        //     Usage (0x41)
0x15, 0x00,        //     Logical Minimum (0)
0x27, 0xFF, 0xFF, 0x00, 0x00,  //     Logical Maximum (65534)
0x75, 0x10,        //     Report Size (16)
0x95, 0x0F,        //     Report Count (15)
0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0xC0,              //   End Collection
0x09, 0x80,        //   Usage (0x80)
0xA1, 0x02,        //   Collection (Logical)
0x85, 0x80,        //     Report ID (-128)
0x09, 0x81,        //     Usage (0x81)
0x15, 0x00,        //     Logical Minimum (0)
0x25, 0x7F,        //     Logical Maximum (127)
0x75, 0x08,        //     Report Size (8)
0x95, 0x45,        //     Report Count (69)
0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0xC0,              //   End Collection
0x09, 0x80,        //   Usage (0x80)
0xA1, 0x02,        //   Collection (Logical)
0x85, 0x81,        //     Report ID (-127)
0x09, 0x81,        //     Usage (0x81)
0x15, 0x00,        //     Logical Minimum (0)
0x25, 0x7F,        //     Logical Maximum (127)
0x75, 0x08,        //     Report Size (8)
0x95, 0x3D,        //     Report Count (61)
0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0xC0,              //   End Collection
0x09, 0xA0,        //   Usage (0xA0)
0xA1, 0x02,        //   Collection (Logical)
0x85, 0xA0,        //     Report ID (-96)
0x09, 0xA2,        //     Usage (0xA2)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x01,        //     Report Count (1)
0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xA2,        //     Usage (0xA2)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x01,        //     Report Count (1)
0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0xC0,              //   End Collection
0x09, 0xA0,        //   Usage (0xA0)
0xA1, 0x02,        //   Collection (Logical)
0x85, 0xA1,        //     Report ID (-95)
0x09, 0xA2,        //     Usage (0xA2)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x60,        //     Report Count (96)
0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xA2,        //     Usage (0xA2)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x60,        //     Report Count (96)
0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xA2,        //     Usage (0xA2)
0x15, 0x00,        //     Logical Minimum (0)
0x25, 0x1F,        //     Logical Maximum (31)
0x75, 0x08,        //     Report Size (8)
0x95, 0x08,        //     Report Count (8)
0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xA2,        //     Usage (0xA2)
0x15, 0x00,        //     Logical Minimum (0)
0x27, 0xFF, 0xFF, 0x00, 0x00,  //     Logical Maximum (65534)
0x75, 0x10,        //     Report Size (16)
0x95, 0x01,        //     Report Count (1)
0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xA2,        //     Usage (0xA2)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x01,        //     Report Count (1)
0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0xC0,              //   End Collection
0x09, 0xA0,        //   Usage (0xA0)
0xA1, 0x02,        //   Collection (Logical)
0x85, 0xA2,        //     Report ID (-94)
0x09, 0xA2,        //     Usage (0xA2)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x0C,        //     Report Count (12)
0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xA2,        //     Usage (0xA2)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x0C,        //     Report Count (12)
0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xA2,        //     Usage (0xA2)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x14,        //     Report Count (20)
0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0xC0,              //   End Collection
0x09, 0xA0,        //   Usage (0xA0)
0xA1, 0x02,        //   Collection (Logical)
0x85, 0xA3,        //     Report ID (-93)
0x09, 0xA2,        //     Usage (0xA2)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x0C,        //     Report Count (12)
0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xA2,        //     Usage (0xA2)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x0C,        //     Report Count (12)
0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xA2,        //     Usage (0xA2)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x0C,        //     Report Count (12)
0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xA2,        //     Usage (0xA2)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x0C,        //     Report Count (12)
0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xA2,        //     Usage (0xA2)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x0C,        //     Report Count (12)
0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xA2,        //     Usage (0xA2)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x0C,        //     Report Count (12)
0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0xC0,              //   End Collection
0x09, 0xA0,        //   Usage (0xA0)
0xA1, 0x02,        //   Collection (Logical)
0x85, 0xA4,        //     Report ID (-92)
0x09, 0xA2,        //     Usage (0xA2)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x80,        //     Report Count (-128)
0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0xC0,              //   End Collection
0x09, 0xA0,        //   Usage (0xA0)
0xA1, 0x02,        //   Collection (Logical)
0x85, 0xAA,        //     Report ID (-86)
0x09, 0xA2,        //     Usage (0xA2)
0x15, 0x00,        //     Logical Minimum (0)
0x27, 0xFF, 0xFF, 0x00, 0x00,  //     Logical Maximum (65534)
0x75, 0x10,        //     Report Size (16)
0x95, 0x19,        //     Report Count (25)
0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0xC0,              //   End Collection
0x09, 0xA0,        //   Usage (0xA0)
0xA1, 0x02,        //   Collection (Logical)
0x85, 0xAF,        //     Report ID (-81)
0x09, 0xA2,        //     Usage (0xA2)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x01,        //     Report Count (1)
0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xA2,        //     Usage (0xA2)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x01,        //     Report Count (1)
0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0xC0,              //   End Collection
0x09, 0xD0,        //   Usage (0xD0)
0xA1, 0x02,        //   Collection (Logical)
0x85, 0xD0,        //     Report ID (-48)
0x09, 0xD1,        //     Usage (0xD1)
0x15, 0x00,        //     Logical Minimum (0)
0x27, 0xFF, 0xFF, 0x00, 0x00,  //     Logical Maximum (65534)
0x75, 0x10,        //     Report Size (16)
0x95, 0x02,        //     Report Count (2)
0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xD1,        //     Usage (0xD1)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x01,        //     Report Count (1)
0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xE7,        //     Usage (0xE7)
0x15, 0x00,        //     Logical Minimum (0)
0x25, 0x64,        //     Logical Maximum (100)
0x75, 0x08,        //     Report Size (8)
0x95, 0x01,        //     Report Count (1)
0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0x81,        //     Usage (0x81)
0x15, 0x00,        //     Logical Minimum (0)
0x25, 0x7F,        //     Logical Maximum (127)
0x75, 0x08,        //     Report Size (8)
0x95, 0x01,        //     Report Count (1)
0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xD1,        //     Usage (0xD1)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x01,        //     Report Count (1)
0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xE6,        //     Usage (0xE6)
0x15, 0x00,        //     Logical Minimum (0)
0x25, 0x64,        //     Logical Maximum (100)
0x75, 0x08,        //     Report Size (8)
0x95, 0x06,        //     Report Count (6)
0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xD1,        //     Usage (0xD1)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x02,        //     Report Count (2)
0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xD1,        //     Usage (0xD1)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x10,        //     Report Count (16)
0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0xC0,              //   End Collection
0x09, 0xD8,        //   Usage (0xD8)
0xA1, 0x02,        //   Collection (Logical)
0x85, 0xD8,        //     Report ID (-40)
0x09, 0xD1,        //     Usage (0xD1)
0x15, 0x00,        //     Logical Minimum (0)
0x27, 0xFF, 0xFF, 0x00, 0x00,  //     Logical Maximum (65534)
0x75, 0x10,        //     Report Size (16)
0x95, 0x02,        //     Report Count (2)
0xB1, 0x03,        //     Feature (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xD1,        //     Usage (0xD1)
0x15, 0x00,        //     Logical Minimum (0)
0x27, 0xFF, 0xFF, 0x00, 0x00,  //     Logical Maximum (65534)
0x75, 0x10,        //     Report Size (16)
0x95, 0x02,        //     Report Count (2)
0xB1, 0x03,        //     Feature (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xD1,        //     Usage (0xD1)
0x15, 0x00,        //     Logical Minimum (0)
0x27, 0xFF, 0xFF, 0x00, 0x00,  //     Logical Maximum (65534)
0x75, 0x10,        //     Report Size (16)
0x95, 0x04,        //     Report Count (4)
0xB1, 0x03,        //     Feature (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xD1,        //     Usage (0xD1)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x01,        //     Report Count (1)
0xB1, 0x03,        //     Feature (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xD1,        //     Usage (0xD1)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x0F,        //     Report Count (15)
0xB1, 0x03,        //     Feature (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0xC0,              //   End Collection
0x09, 0xD8,        //   Usage (0xD8)
0xA1, 0x02,        //   Collection (Logical)
0x85, 0xD9,        //     Report ID (-39)
0x09, 0xD1,        //     Usage (0xD1)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x20,        //     Report Count (32)
0xB1, 0x03,        //     Feature (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0xC0,              //   End Collection
0x09, 0xF0,        //   Usage (0xF0)
0xA1, 0x02,        //   Collection (Logical)
0x85, 0xF0,        //     Report ID (-16)
0x09, 0x06,        //     Usage (0x06)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x08,        //     Report Count (8)
0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0x09,        //     Usage (0x09)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x02,        //     Report Count (2)
0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0x08,        //     Usage (0x08)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x02,        //     Report Count (2)
0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0xC0,              //   End Collection
0x09, 0xF0,        //   Usage (0xF0)
0xA1, 0x02,        //   Collection (Logical)
0x85, 0xF1,        //     Report ID (-15)
0x09, 0x81,        //     Usage (0x81)
0x15, 0x00,        //     Logical Minimum (0)
0x25, 0x7F,        //     Logical Maximum (127)
0x75, 0x08,        //     Report Size (8)
0x95, 0xD0,        //     Report Count (-48)
0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0xC0,              //   End Collection
0x09, 0xF0,        //   Usage (0xF0)
0xA1, 0x02,        //   Collection (Logical)
0x85, 0xF3,        //     Report ID (-13)
0x09, 0x81,        //     Usage (0x81)
0x15, 0x00,        //     Logical Minimum (0)
0x25, 0x7F,        //     Logical Maximum (127)
0x75, 0x08,        //     Report Size (8)
0x95, 0x01,        //     Report Count (1)
0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0xC0,              //   End Collection
0x09, 0xF0,        //   Usage (0xF0)
0xA1, 0x02,        //   Collection (Logical)
0x85, 0xF4,        //     Report ID (-12)
0x09, 0xF4,        //     Usage (0xF4)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x01,        //     Report Count (1)
0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xF5,        //     Usage (0xF5)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x1F,        //     Report Count (31)
0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0xC0,              //   End Collection
0x09, 0xE0,        //   Usage (0xE0)
0xA1, 0x02,        //   Collection (Logical)
0x85, 0xF8,        //     Report ID (-8)
0x09, 0xE1,        //     Usage (0xE1)
0x15, 0x00,        //     Logical Minimum (0)
0x27, 0xFF, 0xFF, 0x00, 0x00,  //     Logical Maximum (65534)
0x75, 0x10,        //     Report Size (16)
0x95, 0x01,        //     Report Count (1)
0xB1, 0x03,        //     Feature (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xE2,        //     Usage (0xE2)
0x15, 0x00,        //     Logical Minimum (0)
0x27, 0xFF, 0xFF, 0x00, 0x00,  //     Logical Maximum (65534)
0x75, 0x10,        //     Report Size (16)
0x95, 0x01,        //     Report Count (1)
0xB1, 0x03,        //     Feature (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xE3,        //     Usage (0xE3)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x01,        //     Report Count (1)
0xB1, 0x03,        //     Feature (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xE4,        //     Usage (0xE4)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x01,        //     Report Count (1)
0xB1, 0x03,        //     Feature (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xE5,        //     Usage (0xE5)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x01,        //     Report Count (1)
0xB1, 0x03,        //     Feature (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xE7,        //     Usage (0xE7)
0x15, 0x00,        //     Logical Minimum (0)
0x25, 0x64,        //     Logical Maximum (100)
0x75, 0x08,        //     Report Size (8)
0x95, 0x01,        //     Report Count (1)
0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xE6,        //     Usage (0xE6)
0x15, 0x00,        //     Logical Minimum (0)
0x25, 0x64,        //     Logical Maximum (100)
0x75, 0x08,        //     Report Size (8)
0x95, 0x01,        //     Report Count (1)
0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xE8,        //     Usage (0xE8)
0x15, 0x00,        //     Logical Minimum (0)
0x25, 0x01,        //     Logical Maximum (1)
0x75, 0x08,        //     Report Size (8)
0x95, 0x01,        //     Report Count (1)
0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0xC0,              //   End Collection
0x09, 0xE0,        //   Usage (0xE0)
0xA1, 0x02,        //   Collection (Logical)
0x85, 0xF9,        //     Report ID (-7)
0x09, 0xE1,        //     Usage (0xE1)
0x15, 0x00,        //     Logical Minimum (0)
0x27, 0xFF, 0xFF, 0x00, 0x00,  //     Logical Maximum (65534)
0x75, 0x10,        //     Report Size (16)
0x95, 0x01,        //     Report Count (1)
0xB1, 0x03,        //     Feature (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xE2,        //     Usage (0xE2)
0x15, 0x00,        //     Logical Minimum (0)
0x27, 0xFF, 0xFF, 0x00, 0x00,  //     Logical Maximum (65534)
0x75, 0x10,        //     Report Size (16)
0x95, 0x01,        //     Report Count (1)
0xB1, 0x03,        //     Feature (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xE3,        //     Usage (0xE3)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x01,        //     Report Count (1)
0xB1, 0x03,        //     Feature (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xE4,        //     Usage (0xE4)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x01,        //     Report Count (1)
0xB1, 0x03,        //     Feature (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xE5,        //     Usage (0xE5)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x01,        //     Report Count (1)
0xB1, 0x03,        //     Feature (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xE7,        //     Usage (0xE7)
0x15, 0x00,        //     Logical Minimum (0)
0x25, 0x64,        //     Logical Maximum (100)
0x75, 0x08,        //     Report Size (8)
0x95, 0x01,        //     Report Count (1)
0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xE6,        //     Usage (0xE6)
0x15, 0x00,        //     Logical Minimum (0)
0x25, 0x64,        //     Logical Maximum (100)
0x75, 0x08,        //     Report Size (8)
0x95, 0x01,        //     Report Count (1)
0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x09, 0xE8,        //     Usage (0xE8)
0x15, 0x00,        //     Logical Minimum (0)
0x25, 0x01,        //     Logical Maximum (1)
0x75, 0x08,        //     Report Size (8)
0x95, 0x01,        //     Report Count (1)
0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0xC0,              //   End Collection
0xC0,              // End Collection

// 1054 bytes
```

```
[joostn@ideapad ~]$ lsusb -d 17cc:1620 -v

Bus 003 Device 007: ID 17cc:1620 Native Instruments KOMPLETE KONTROL S61 MK2
Device Descriptor:
  bLength                18
  bDescriptorType         1
  bcdUSB               2.00
  bDeviceClass          239 Miscellaneous Device
  bDeviceSubClass         2 [unknown]
  bDeviceProtocol         1 Interface Association
  bMaxPacketSize0        64
  idVendor           0x17cc Native Instruments
  idProduct          0x1620 KOMPLETE KONTROL S61 MK2
  bcdDevice            0.59
  iManufacturer           2 Native Instruments
  iProduct                3 KOMPLETE KONTROL S61 MK2
  iSerial                 1 7BD76471
  bNumConfigurations      1
  Configuration Descriptor:
    bLength                 9
    bDescriptorType         2
    wTotalLength       0x00cb
    bNumInterfaces          5
    bConfigurationValue     1
    iConfiguration          0 
    bmAttributes         0x80
      (Bus Powered)
    MaxPower              480mA
    Interface Association:
      bLength                 8
      bDescriptorType        11
      bFirstInterface         0
      bInterfaceCount         2
      bFunctionClass          1 Audio
      bFunctionSubClass       0 [unknown]
      bFunctionProtocol      32 
      iFunction               0 
    Interface Descriptor:
      bLength                 9
      bDescriptorType         4
      bInterfaceNumber        0
      bAlternateSetting       0
      bNumEndpoints           0
      bInterfaceClass         1 Audio
      bInterfaceSubClass      1 Control Device
      bInterfaceProtocol     32 
      iInterface              3 KOMPLETE KONTROL S61 MK2
      AudioControl Interface Descriptor:
        bLength                 9
        bDescriptorType        36
        bDescriptorSubtype      1 (HEADER)
        bcdADC               2.00
        bCategory               8
        wTotalLength       0x0009
        bmControls           0x00
    Interface Descriptor:
      bLength                 9
      bDescriptorType         4
      bInterfaceNumber        1
      bAlternateSetting       0
      bNumEndpoints           2
      bInterfaceClass         1 Audio
      bInterfaceSubClass      3 MIDI Streaming
      bInterfaceProtocol      0 
      iInterface              7 KOMPLETE KONTROL S61 MK2 MIDI
      MIDIStreaming Interface Descriptor:
        bLength                 7
        bDescriptorType        36
        bDescriptorSubtype      1 (HEADER)
        bcdADC               1.00
        wTotalLength       0x005d
      MIDIStreaming Interface Descriptor:
        bLength                 6
        bDescriptorType        36
        bDescriptorSubtype      2 (MIDI_IN_JACK)
        bJackType               1 Embedded
        bJackID                40
        iJack                   0 
      MIDIStreaming Interface Descriptor:
        bLength                 6
        bDescriptorType        36
        bDescriptorSubtype      2 (MIDI_IN_JACK)
        bJackType               1 Embedded
        bJackID                42
        iJack                   0 
      MIDIStreaming Interface Descriptor:
        bLength                 6
        bDescriptorType        36
        bDescriptorSubtype      2 (MIDI_IN_JACK)
        bJackType               2 External
        bJackID                41
        iJack                  19 KOMPLETE KONTROL S61 MK2 IN
      MIDIStreaming Interface Descriptor:
        bLength                 6
        bDescriptorType        36
        bDescriptorSubtype      2 (MIDI_IN_JACK)
        bJackType               2 External
        bJackID                43
        iJack                  20 KOMPLETE KONTROL S61 MK2 IN EXT
      MIDIStreaming Interface Descriptor:
        bLength                 9
        bDescriptorType        36
        bDescriptorSubtype      3 (MIDI_OUT_JACK)
        bJackType               1 Embedded
        bJackID                50
        bNrInputPins            1
        baSourceID( 0)         41
        BaSourcePin( 0)         1
        iJack                   0 
      MIDIStreaming Interface Descriptor:
        bLength                 9
        bDescriptorType        36
        bDescriptorSubtype      3 (MIDI_OUT_JACK)
        bJackType               1 Embedded
        bJackID                52
        bNrInputPins            1
        baSourceID( 0)         43
        BaSourcePin( 0)         1
        iJack                   0 
      MIDIStreaming Interface Descriptor:
        bLength                 9
        bDescriptorType        36
        bDescriptorSubtype      3 (MIDI_OUT_JACK)
        bJackType               2 External
        bJackID                51
        bNrInputPins            1
        baSourceID( 0)         40
        BaSourcePin( 0)         1
        iJack                  21 KOMPLETE KONTROL S61 MK2 OUT
      MIDIStreaming Interface Descriptor:
        bLength                 9
        bDescriptorType        36
        bDescriptorSubtype      3 (MIDI_OUT_JACK)
        bJackType               2 External
        bJackID                53
        bNrInputPins            1
        baSourceID( 0)         42
        BaSourcePin( 0)         1
        iJack                  22 KOMPLETE KONTROL S61 MK2 OUT EXT
      Endpoint Descriptor:
        bLength                 7
        bDescriptorType         5
        bEndpointAddress     0x81  EP 1 IN
        bmAttributes            2
          Transfer Type            Bulk
          Synch Type               None
          Usage Type               Data
        wMaxPacketSize     0x0200  1x 512 bytes
        bInterval               0
        MIDIStreaming Endpoint Descriptor:
          bLength                 6
          bDescriptorType        37
          bDescriptorSubtype      1 (Invalid)
          bNumEmbMIDIJack         2
          baAssocJackID( 0)      50
          baAssocJackID( 1)      52
      Endpoint Descriptor:
        bLength                 7
        bDescriptorType         5
        bEndpointAddress     0x01  EP 1 OUT
        bmAttributes            2
          Transfer Type            Bulk
          Synch Type               None
          Usage Type               Data
        wMaxPacketSize     0x0200  1x 512 bytes
        bInterval               0
        MIDIStreaming Endpoint Descriptor:
          bLength                 6
          bDescriptorType        37
          bDescriptorSubtype      1 (Invalid)
          bNumEmbMIDIJack         2
          baAssocJackID( 0)      40
          baAssocJackID( 1)      42
    Interface Descriptor:
      bLength                 9
      bDescriptorType         4
      bInterfaceNumber        2
      bAlternateSetting       0
      bNumEndpoints           2
      bInterfaceClass         3 Human Interface Device
      bInterfaceSubClass      0 [unknown]
      bInterfaceProtocol      0 
      iInterface              8 KOMPLETE KONTROL S61 MK2 HID
        HID Device Descriptor:
          bLength                 9
          bDescriptorType        33
          bcdHID               1.11
          bCountryCode            0 Not supported
          bNumDescriptors         1
          bDescriptorType        34 Report
          wDescriptorLength    1054
          Report Descriptors: 
            ** UNAVAILABLE **
      Endpoint Descriptor:
        bLength                 7
        bDescriptorType         5
        bEndpointAddress     0x82  EP 2 IN
        bmAttributes            3
          Transfer Type            Interrupt
          Synch Type               None
          Usage Type               Data
        wMaxPacketSize     0x0040  1x 64 bytes
        bInterval               2
      Endpoint Descriptor:
        bLength                 7
        bDescriptorType         5
        bEndpointAddress     0x02  EP 2 OUT
        bmAttributes            3
          Transfer Type            Interrupt
          Synch Type               None
          Usage Type               Data
        wMaxPacketSize     0x0040  1x 64 bytes
        bInterval               2
    Interface Descriptor:
      bLength                 9
      bDescriptorType         4
      bInterfaceNumber        3
      bAlternateSetting       0
      bNumEndpoints           1
      bInterfaceClass       255 Vendor Specific Class
      bInterfaceSubClass    189 [unknown]
      bInterfaceProtocol      0 
      iInterface              9 KOMPLETE KONTROL S61 MK2 BD
      Endpoint Descriptor:
        bLength                 7
        bDescriptorType         5
        bEndpointAddress     0x03  EP 3 OUT
        bmAttributes            2
          Transfer Type            Bulk
          Synch Type               None
          Usage Type               Data
        wMaxPacketSize     0x0200  1x 512 bytes
        bInterval               1
    Interface Descriptor:
      bLength                 9
      bDescriptorType         4
      bInterfaceNumber        4
      bAlternateSetting       0
      bNumEndpoints           0
      bInterfaceClass       254 Application Specific Interface
      bInterfaceSubClass      1 Device Firmware Update
      bInterfaceProtocol      1 
      iInterface             10 KOMPLETE KONTROL S61 MK2 DFU
      Device Firmware Upgrade Interface Descriptor:
        bLength                             9
        bDescriptorType                    33
        bmAttributes                       11
          Will Detach
          Manifestation Intolerant
          Upload Supported
          Download Supported
        wDetachTimeout                    250 milliseconds
        wTransferSize                      64 bytes
        bcdDFUVersion                   1.10
Device Qualifier (for other device speed):
  bLength                10
  bDescriptorType         6
  bcdUSB               2.00
  bDeviceClass          239 Miscellaneous Device
  bDeviceSubClass         2 [unknown]
  bDeviceProtocol         1 Interface Association
  bMaxPacketSize0        64
  bNumConfigurations      1
Device Status:     0x0000
  (Bus Powered)
[joostn@ideapad ~]$ 

```
report 0xa4: key zones
    qKontrol/source/qkontrol.cpp setKeyzones()


```
1 0 0 0 0 8 0 0 0 0 d5 2 37 0 af 3 e7 1 53 1 40 0 d0 3 c2 3 0 0 0 0 8 24 
1 0 0 0 0 0 0 0 0 0 d5 2 37 0 af 3 e7 1 53 1 40 0 d0 3 c2 3 0 0 0 0 8 24 
```

https://github.com/ojacques/SynthesiaKontrol/blob/master/color_scan.py

https://github.com/ojacques/SynthesiaKontrol/blob/master/SynthesiaKontrol.py


# bitmap:
https://github.com/GoaSkin/qKontrol/blob/b478fd9818c1b01c695722762e6d55a9b2e0228e/source/qkontrol.cpp#L150
void qkontrolWindow::drawImage(uint8_t screen, QPixmap *pixmap, ushort x, ushort y)

# init:
    # Write 3 bytes to the device: 0xa0, 0x00, 0x00
    # Thanks to @xample for investigating this
    device.write([0xa0, 0x00, 0x00])

# init:
0xa0 + 2 bytes
Set to [0,0] to enable interactive mode

# lights:
keyboard: 0x81 + 61 bytes (0-127)
buttons: 0x80 + 69 bytes  (0-127)

# overige HID output:
0xa1: 96 bytes + 96 bytes + 8 bytes (0-31) + 1 ushort (0-65534) + 1 byte (0-255)
0xa2: 12 bytes + 12 bytes + 20 bytes (0-255)
0xa3: 6 x 12 bytes
0xa4: 128 bytes (key zones!)
0xaf: 2 bytes
0xd0: feature report: 2 ushort (0-65534) + 1 byte (0-255) + 1 byte (0-100) + ...meer
0xd8: feature report: 
0xd9: feature report: 32 bytes
0xf0: feature report: 8 bytes + 2 bytes + 2 bytes
0xf1: feature report: 0xd0 bytes (0-127)
0xf3: feature report: 1 byte (0-127)
0xf4: feature report: 1 byte (0-255) + 31 bytes (0-255)
0xf8: feature report: 1 ushort (0-65534) + 1 ushort (0-65534) + 1 byte (0-255) + 1 byte (0-255) + 1 byte (0-255) + 1 byte (0-100) + 1 byte (0-100) + 1 byte (0-1)
0xf9: feature report: 1 ushort (0-65534) + 1 ushort (0-65534) + 1 byte (0-255) + 1 byte (0-255) + 1 byte (0-255) + 1 byte (0-100) + 1 byte (0-100) + 1 byte (0-1)

# incoming reports:
0x01: 19 bytes: state of all buttons
0xaa: state of all controllers (in midi mode)

LCD kleuren:

byte 0: RrrrrGgg
byte 1: gggBbbbb