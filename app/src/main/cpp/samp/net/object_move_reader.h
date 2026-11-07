#pragma once
#include <cmath>
#include <cstdint>

struct ObjectMoveData {
    uint16_t id;
    float fromX, fromY, fromZ;
    float x, y, z, speed, rotX, rotY, rotZ;
};

template <typename Stream>
bool ReadObjectMove(Stream& stream, ObjectMoveData& move) {
    if (!stream.Read(move.id) || !stream.Read(move.fromX) || !stream.Read(move.fromY) ||
        !stream.Read(move.fromZ) || !stream.Read(move.x) || !stream.Read(move.y) ||
        !stream.Read(move.z) || !stream.Read(move.speed) || !stream.Read(move.rotX) ||
        !stream.Read(move.rotY) || !stream.Read(move.rotZ)) return false;
    return std::isfinite(move.x) && std::isfinite(move.y) && std::isfinite(move.z) &&
           std::isfinite(move.speed) && move.speed > 0.0f &&
           std::isfinite(move.rotX) && std::isfinite(move.rotY) && std::isfinite(move.rotZ);
}
