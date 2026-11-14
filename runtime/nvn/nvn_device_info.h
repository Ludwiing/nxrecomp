// Device properties returned by nvnDeviceGetInteger.
// Switch 2 values follow seedds/nxnative@04811a63, dm/gfx/nvn/hle_nvn.c (MIT), whose comments cite SDK 20.5.19 and
// 21.4.4 device data. Properties marked runtime-only keep the Switch 1 value below 0x60 and return zero above it,
// matching the documented host fallback. https://github.com/seedds/nxnative/blob/04811a63/dm/gfx/nvn/hle_nvn.c
#pragma once
#include <stddef.h>

enum { NVN_DEVICE_INFO_SWITCH2_RUNTIME = -1 };

static const int nvn_device_info_switch2[0xAA] = {
    // Sports Resort's NVN2 bootstrap explicitly requires 231.41 before it initializes its device object.
    [0x00] = 231, [0x01] = 41, [0x02] = 14, [0x03] = 65536, [0x04] = 256, [0x05] = 8, [0x06] = 16, [0x07] = 4,
    [0x08] = 16, [0x09] = 32, [0x0A] = 16, [0x0B] = 4, [0x0C] = 4, [0x0D] = 4, [0x0E] = 32, [0x0F] = 32,
    [0x10] = 32, [0x11] = 256, [0x12] = 256, [0x13] = 4, [0x14] = 8, [0x15] = 512, [0x16] = 256,
    [0x17] = NVN_DEVICE_INFO_SWITCH2_RUNTIME, [0x18] = NVN_DEVICE_INFO_SWITCH2_RUNTIME, [0x19] = 4096,
    [0x1A] = 16384, [0x1B] = 16, [0x1C] = 1024, [0x1D] = 1024, [0x1E] = 64, [0x1F] = 1024,
    [0x20] = 2147483647, [0x21] = 65535, [0x22] = 65535, [0x23] = 8, [0x24] = 1048576, [0x25] = 4096,
    [0x26] = 16, [0x27] = 4096, [0x28] = 1, [0x29] = 1, [0x2A] = 1,
    [0x2B] = NVN_DEVICE_INFO_SWITCH2_RUNTIME, [0x2C] = 16, [0x2D] = NVN_DEVICE_INFO_SWITCH2_RUNTIME,
    [0x2E] = 201, [0x2F] = 201, [0x30] = 20, [0x31] = 0, [0x32] = 1, [0x33] = 8, [0x34] = 8,
    [0x35] = 4, [0x36] = 32, [0x37] = NVN_DEVICE_INFO_SWITCH2_RUNTIME, [0x38] = NVN_DEVICE_INFO_SWITCH2_RUNTIME,
    [0x39] = 128, [0x3A] = NVN_DEVICE_INFO_SWITCH2_RUNTIME, [0x3B] = 1, [0x3C] = 4,
    [0x3D] = NVN_DEVICE_INFO_SWITCH2_RUNTIME, [0x3E] = NVN_DEVICE_INFO_SWITCH2_RUNTIME,
    [0x3F] = NVN_DEVICE_INFO_SWITCH2_RUNTIME, [0x40] = NVN_DEVICE_INFO_SWITCH2_RUNTIME,
    [0x41] = NVN_DEVICE_INFO_SWITCH2_RUNTIME, [0x42] = 1, [0x43] = 1, [0x44] = 1, [0x45] = 1, [0x46] = 5,
    [0x47] = 1, [0x48] = 1, [0x49] = 1, [0x4A] = 1, [0x4B] = 1, [0x4C] = 1, [0x4D] = 16, [0x4E] = 2048,
    [0x4F] = 32, [0x50] = 4096, [0x51] = 65536, [0x52] = 65536, [0x53] = 256, [0x54] = 16384,
    [0x55] = 262144, [0x56] = 4096, [0x57] = 1, [0x58] = 4, [0x59] = 2, [0x5A] = 0, [0x5B] = 4096,
    [0x5C] = 16384, [0x5D] = 4096, [0x5E] = 128, [0x5F] = 32, [0x60] = 65535, [0x61] = 1,
    [0x62] = NVN_DEVICE_INFO_SWITCH2_RUNTIME, [0x63] = 256, [0x64] = 16, [0x65] = 65535, [0x66] = 65535,
    [0x67] = 256, [0x68] = 1, [0x69] = 16, [0x6A] = 16, [0x6B] = 16, [0x6C] = 1, [0x6D] = 1,
    [0x6E] = 1, [0x6F] = 256, [0x70] = 0, [0x71] = 16, [0x72] = 8, [0x73] = 16, [0x74] = 32,
    [0x75] = 31, [0x76] = 536870912, [0x77] = 16777216, [0x78] = 16777216, [0x79] = 1073741824,
    [0x7A] = 32, [0x7B] = 1, [0x7C] = 64, [0x7D] = NVN_DEVICE_INFO_SWITCH2_RUNTIME, [0x7E] = 1,
    [0x7F] = 2147483647, [0x80] = 32, [0x81] = 32, [0x82] = 4194303, [0x83] = 512, [0x84] = 256,
    [0x85] = 32, [0x86] = 16383, [0x87] = NVN_DEVICE_INFO_SWITCH2_RUNTIME, [0x88] = 8, [0x89] = 0,
    [0x8A] = 0, [0x8B] = 4, [0x8C] = 256, [0x8D] = 16, [0x8E] = 0, [0x8F] = 1, [0x90] = 31,
    [0x91] = 0, [0x92] = 64, [0x93] = 1, [0x94] = 58368, [0x95] = 24576, [0x96] = 0, [0x97] = 256,
    [0x98] = 262144, [0x99] = NVN_DEVICE_INFO_SWITCH2_RUNTIME, [0x9A] = NVN_DEVICE_INFO_SWITCH2_RUNTIME,
    [0x9B] = 0, [0x9C] = 201, [0x9D] = 14, [0x9E] = 201, [0x9F] = 20, [0xA0] = 163840, [0xA1] = 8,
    [0xA2] = 4, [0xA3] = 256, [0xA4] = 256, [0xA5] = 4, [0xA6] = 65536, [0xA7] = 201, [0xA8] = 5,
    [0xA9] = 1,
};

static inline int nvn_device_info_get(int switch2, unsigned item, const int *switch1, size_t switch1_count, int *out) {
    if (!switch1 || !out) return 0;
    if (!switch2) {
        if (item >= switch1_count) return 0;
        *out = switch1[item];
        return 1;
    }
    if (item >= sizeof nvn_device_info_switch2 / sizeof nvn_device_info_switch2[0]) {
        *out = 0;
        return 1;
    }
    int value = nvn_device_info_switch2[item];
    if (value == NVN_DEVICE_INFO_SWITCH2_RUNTIME) {
        *out = item < 0x60 && item < switch1_count ? switch1[item] : 0;
        return 1;
    }
    *out = value;
    return 1;
}

static inline int nvn_device_info_switch2_is_runtime(unsigned item) {
    return item < sizeof nvn_device_info_switch2 / sizeof nvn_device_info_switch2[0] &&
           nvn_device_info_switch2[item] == NVN_DEVICE_INFO_SWITCH2_RUNTIME;
}

static inline int nvn_device_info_switch2_is_unknown(unsigned item) {
    return item >= sizeof nvn_device_info_switch2 / sizeof nvn_device_info_switch2[0];
}
