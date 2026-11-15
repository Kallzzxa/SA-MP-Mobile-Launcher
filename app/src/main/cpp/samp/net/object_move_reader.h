#pragma once
#include <cmath>
#include <cstdint>

struct ObjectMoveData {
    uint16_t id;
    float fromX, fromY, fromZ;
    float x, y, z, speed, rotX, rotY, rotZ;
};

// GTA's world/sector structures are only designed for positions near the map.
// Finite but extreme coordinates can still overflow sector calculations in
// CPhysical::RemoveAndAdd and corrupt the world entity lists.
inline bool IsSafeObjectCoordinate(float value) {
    constexpr float kMaxObjectWorldCoordinate = 10000.0f;
    return std::isfinite(value) && std::fabs(value) <= kMaxObjectWorldCoordinate;
}

inline bool IsSafeObjectPosition(float x, float y, float z) {
    return IsSafeObjectCoordinate(x) && IsSafeObjectCoordinate(y) &&
           IsSafeObjectCoordinate(z);
}

template <typename Stream>
bool ReadObjectMove(Stream& stream, ObjectMoveData& move) {
    if (!stream.Read(move.id) || !stream.Read(move.fromX) || !stream.Read(move.fromY) ||
        !stream.Read(move.fromZ) || !stream.Read(move.x) || !stream.Read(move.y) ||
        !stream.Read(move.z) || !stream.Read(move.speed) || !stream.Read(move.rotX) ||
        !stream.Read(move.rotY) || !stream.Read(move.rotZ)) return false;
    return IsSafeObjectPosition(move.fromX, move.fromY, move.fromZ) &&
           IsSafeObjectPosition(move.x, move.y, move.z) &&
           std::isfinite(move.speed) && move.speed > 0.0f &&
           std::isfinite(move.rotX) && std::isfinite(move.rotY) && std::isfinite(move.rotZ);
}
