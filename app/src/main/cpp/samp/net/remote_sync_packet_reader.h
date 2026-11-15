#pragma once
#include <cstdint>

// Common server-to-client framing: packet ID, player ID, then packed payload.
template <typename Stream, typename PlayerId, typename Payload>
bool ReadRemoteSyncPacket(Stream& stream, PlayerId& playerId, Payload& payload) {
    uint8_t packetId;
    return stream.Read(packetId) && stream.Read(playerId) &&
           stream.Read(reinterpret_cast<char*>(&payload), sizeof(payload));
}
