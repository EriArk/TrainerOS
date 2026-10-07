// SPDX-License-Identifier: GPL-3.0-or-later
// Independent implementation of Gambatte's libretro battery-clock convention.
// This is not a save-state format. Halt/carry are volatile, as in that convention.
#pragma once
#include <cstdint>

struct TrainerMbc3Rtc {
    std::uint64_t base = 0;
    std::uint64_t haltedAt = 0;
    std::uint8_t flags = 0;

    std::uint64_t elapsed(std::uint64_t now) {
        const auto clock = (flags & 0x40) ? haltedAt : now;
        if (clock < base) return 0; // A host clock correction must not underflow.
        auto value = clock - base;
        // Compatibility with Gambatte's battery RTC, including its 511-day
        // normalization boundary (not a claim about hardware's 512-day wrap).
        constexpr std::uint64_t period = 511 * 86400;
        if (value > period) {
            const auto periods = (value - 1) / period;
            base += periods * period;
            value -= periods * period;
            flags |= 0x80;
        }
        return value;
    }
    std::uint8_t get(int reg, std::uint64_t now) {
        const auto value = elapsed(now);
        switch (reg) {
        case 8: return value % 60;
        case 9: return value / 60 % 60;
        case 10: return value / 3600 % 24;
        case 11: return value / 86400 & 255;
        case 12: return flags | ((value / 86400 >> 8) & 1);
        default: return 0;
        }
    }
    void set(int reg, std::uint8_t byte, std::uint64_t now) {
        const auto value = elapsed(now);
        auto changed = value;
        switch (reg) {
        case 8: changed = value - value % 60 + byte % 60; break;
        case 9: changed = value - (value / 60 % 60) * 60 + (byte % 60) * 60; break;
        case 10: changed = value - (value / 3600 % 24) * 3600 + (byte % 24) * 3600; break;
        case 11: changed = value - (value / 86400 & 255) * 86400 + std::uint64_t(byte) * 86400; break;
        case 12: changed = value % (256 * 86400) + std::uint64_t(byte & 1) * 256 * 86400; break;
        default: return;
        }
        const auto clock = (flags & 0x40) ? haltedAt : now;
        base = clock >= changed ? clock - changed : 0;
        if (reg == 12) {
            if (!(flags & 0x40) && (byte & 0x40)) haltedAt = now;
            if ((flags & 0x40) && !(byte & 0x40)) base += now >= haltedAt ? now - haltedAt : 0;
            flags = byte & 0xc0;
        }
    }
};
