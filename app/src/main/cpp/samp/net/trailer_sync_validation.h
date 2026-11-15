#pragma once

#include <cmath>
#include <cstdint>

// The driver packet identifies the attached vehicle; stale trailer packets must
// not be applied to a different attachment. Vehicle ID 0 means no trailer.
inline bool IsMatchingTrailerSync(uint16_t packetId, uint16_t attachedId,
                                  uint16_t tractorId, unsigned int maxVehicles) {
    return packetId != 0 && packetId < maxVehicles && packetId == attachedId && packetId != tractorId;
}

inline bool IsFiniteTrailerVector(float x, float y, float z) {
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
}

inline bool IsValidTrailerQuaternion(float w, float x, float y, float z) {
    if (!std::isfinite(w) || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return false;
    // Avoid overflow and division by zero when normalizing untrusted network data.
    if (std::fabs(w) > 2.0f || std::fabs(x) > 2.0f || std::fabs(y) > 2.0f || std::fabs(z) > 2.0f) return false;
    return w*w + x*x + y*y + z*z > 0.000001f;
}
