#pragma once
#include <cstdint>

// SA-MP/open.mp passenger packet: seat(6), drive-by(1), cuffed(1),
// weapon(6), additional key(2), serialized as a little-endian uint16.
constexpr uint16_t MakePassengerSyncFlags(uint8_t seat, bool driveBy, bool cuffed,
                                         uint8_t weapon, uint8_t additionalKey) {
    return static_cast<uint16_t>((seat & 0x3fu) | (driveBy ? 0x40u : 0u) |
            (cuffed ? 0x80u : 0u) | ((weapon & 0x3fu) << 8) | ((additionalKey & 0x03u) << 14));
}
constexpr uint8_t PassengerSeat(uint16_t flags) { return static_cast<uint8_t>(flags & 0x3fu); }
constexpr uint8_t PassengerWeapon(uint16_t flags) { return static_cast<uint8_t>((flags >> 8) & 0x3fu); }
constexpr bool PassengerDriveBy(uint16_t flags) { return (flags & 0x40u) != 0; }

template <typename Player, typename Sync>
void ApplyPassengerKeys(Player& player, const Sync& sync) {
    player.SetKeys(sync.lrAnalog, sync.udAnalog, sync.wKeys);
}
