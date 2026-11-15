#include "../../app/src/main/cpp/samp/net/trailer_sync_validation.h"
#include <cassert>
#include <limits>

int main() {
    assert(IsMatchingTrailerSync(7, 7, 2, 2000));
    assert(!IsMatchingTrailerSync(7, 8, 2, 2000));
    assert(!IsMatchingTrailerSync(0, 0, 2, 2000));
    assert(!IsMatchingTrailerSync(2, 2, 2, 2000));
    assert(!IsMatchingTrailerSync(2000, 2000, 2, 2000));
    assert(IsFiniteTrailerVector(1, 0, -4));
    assert(!IsFiniteTrailerVector(0, std::numeric_limits<float>::infinity(), 0));
    assert(!IsFiniteTrailerVector(0, std::numeric_limits<float>::quiet_NaN(), 0));
    assert(IsValidTrailerQuaternion(1, 0, 0, 0));
    assert(!IsValidTrailerQuaternion(0, 0, 0, 0));
    assert(!IsValidTrailerQuaternion(1, 0, 3, 0));
    assert(!IsValidTrailerQuaternion(1, std::numeric_limits<float>::quiet_NaN(), 0, 0));
}
