#include "../../app/src/main/cpp/samp/net/object_material_reader.h"
#include "../../app/src/main/cpp/samp/game/streaming_budget.h"
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include <type_traits>

// Mock byte stream exercises bounds and sequential parsing without GTA/Android.
struct Stream {
    std::vector<unsigned char> bytes;
    size_t offset = 0;
    template <typename T> bool Read(T& v) {
        if (offset + sizeof(T) > bytes.size()) return false;
        std::memcpy(&v, bytes.data() + offset, sizeof(T));
        offset += sizeof(T); return true;
    }
    bool Read(char* out, int len) {
        if (offset + static_cast<size_t>(len) > bytes.size()) return false;
        std::memcpy(out, bytes.data() + offset, len); offset += len; return true;
    }
};
static auto decode = [](char* text, size_t size, Stream& s) {
    uint8_t n;
    if (!s.Read(n) || n >= size || !s.Read(text, n)) return false;
    text[n] = '\0'; return true;
};
int main() {
    Stream two{{1, 3, 0x12, 0x00, 3, 't','x','d', 3, 't','e','x', 1,2,3,4,
                2, 4, 30, 5, 'A','r','i','a','l', 18, 1, 1,2,3,4, 5,6,7,8, 0, 2, 'H','i'}};
    ObjectMaterialData a{}, b{};
    assert(ReadObjectMaterial(two, decode, a));
    assert(a.index == 3 && a.model == 0x12 && std::string(a.txd) == "txd" && std::string(a.texture) == "tex");
    assert(ReadObjectMaterial(two, decode, b));
    assert(b.index == 4 && std::string(b.font) == "Arial" && std::string(b.text) == "Hi");
    assert(two.offset == two.bytes.size());
    Stream truncated{{1, 1, 0, 0, 4, 'x'}};
    ObjectMaterialData bad{};
    assert(!ReadObjectMaterial(truncated, decode, bad));
    Stream unknown{{9, 1}};
    assert(!ReadObjectMaterial(unknown, decode, bad));
    Stream longFont{{2, 1, 10, 255}};
    longFont.bytes.insert(longFont.bytes.end(), 255, 'x');
    longFont.bytes.insert(longFont.bytes.end(), {12, 0, 0,0,0,0, 0,0,0,0, 0, 0});
    ObjectMaterialData longResult{};
    assert(ReadObjectMaterial(longFont, decode, longResult));
    assert(std::strlen(longResult.font) == 255);
    assert(StreamingRemainingBudget(100, 25) == 75);
    assert(StreamingRemainingBudget(100, 100) == 0);
    assert(StreamingRemainingBudget(100, 101) == 0);
}
