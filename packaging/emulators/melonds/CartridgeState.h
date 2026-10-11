// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "NDSCart/CartCommon.h"
#include <optional>
#include <stdexcept>
#include <vector>

namespace traineros::ds {
constexpr size_t MaxCartridgeSaveBytes = 64 * 1024 * 1024;
// A null entry explicitly starts a fresh cartridge. An empty supplied image is
// invalid, never a fallback to fresh progress. Admission, exact-ROM binding and
// consent belong to the caller; this contract grants no file or network access.
struct CartridgeStartup {
    std::vector<std::optional<std::vector<uint8_t>>> seats;
    size_t ownerSeat = 0;
};
struct OwnerCartridgeReturn {
    uint32_t frame = 0;
    std::vector<uint8_t> bytes;
};
inline void InitializeCartridgeState(melonDS::NDSCart::CartCommon& cartridge,
                                     const std::vector<uint8_t>& bytes) {
    const auto expected = cartridge.GetSaveMemoryLength();
    if (bytes.empty() || bytes.size() > MaxCartridgeSaveBytes
            || expected != bytes.size() || !cartridge.GetSaveMemory())
        throw std::invalid_argument("Cartridge save must match the exact core-reported size");
    // The upstream setter silently truncates a mismatched image. Validate first.
    cartridge.SetSaveMemory(bytes.data(), uint32_t(bytes.size()));
}
inline std::vector<uint8_t> CopyCartridgeState(const melonDS::NDSCart::CartCommon& cartridge) {
    const auto size = cartridge.GetSaveMemoryLength();
    const auto* data = cartridge.GetSaveMemory();
    if (!size || size > MaxCartridgeSaveBytes || !data)
        throw std::runtime_error("Cartridge has no supported save memory");
    return {data, data + size};
}
}
