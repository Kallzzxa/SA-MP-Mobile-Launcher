#include "../../app/src/main/cpp/samp/net/object_material_reader.h"
#include "BitStream.h"
#include "StringCompressor.h"
#include <cassert>
#include <cstring>

static void writeName(RakNet::BitStream& stream, const char* name) {
    const auto len = static_cast<uint8_t>(std::strlen(name));
    stream.Write(len);
    if (len) stream.Write(name, len);
}

int main() {
    // Trailing bits in a partial source byte must not contaminate later writes.
    RakNet::BitStream bitTest;
    unsigned char dirty = 0xff;
    bitTest.WriteBits(&dirty, 3, false);
    bitTest.Write0(); bitTest.Write0(); bitTest.Write1();
    assert(bitTest.ReadBit() && bitTest.ReadBit() && bitTest.ReadBit());
    assert(!bitTest.ReadBit() && !bitTest.ReadBit() && bitTest.ReadBit());

    StringCompressor::AddReference();
    auto compressor = StringCompressor::Instance();
    RakNet::BitStream packet;
    packet.Write(uint8_t{1}); packet.Write(uint8_t{3}); packet.Write(uint16_t{1234});
    writeName(packet, "building"); writeName(packet, "brick"); packet.Write(uint32_t{0xFF012345});
    packet.Write(uint8_t{2}); packet.Write(uint8_t{4}); packet.Write(uint8_t{30});
    writeName(packet, "Arial"); packet.Write(uint8_t{18}); packet.Write(uint8_t{1});
    packet.Write(uint32_t{0xFFFFFFFF}); packet.Write(uint32_t{0}); packet.Write(uint8_t{1});
    compressor->EncodeString("Hello mapping", 2048, &packet);
    packet.Write(uint8_t{1}); packet.Write(uint8_t{5}); packet.Write(uint16_t{987});
    writeName(packet, ""); writeName(packet, "glass"); packet.Write(uint32_t{123});
    auto decode = [compressor](char* out, size_t len, RakNet::BitStream& data) {
        return compressor->DecodeString(out, static_cast<int>(len), &data);
    };
    RakNet::BitStream input(packet.GetData(), packet.GetNumberOfBytesUsed(), false);
    input.SetWriteOffset(packet.GetNumberOfBitsUsed());
    ObjectMaterialData first{}, second{}, third{};
    assert(ReadObjectMaterial(input, decode, first));
    assert(ReadObjectMaterial(input, decode, second));
    assert(ReadObjectMaterial(input, decode, third));
    assert(first.index == 3 && first.model == 1234 && std::strcmp(first.texture, "brick") == 0);
    assert(second.index == 4 && std::strcmp(second.text, "Hello mapping") == 0);
    assert(third.index == 5 && std::strcmp(third.txd, "") == 0 && std::strcmp(third.texture, "glass") == 0);
    assert(input.GetNumberOfUnreadBits() == 0);

    RakNet::BitStream cut(packet.GetData(), packet.GetNumberOfBytesUsed(), false);
    cut.SetWriteOffset(16);
    ObjectMaterialData incomplete{};
    assert(!ReadObjectMaterial(cut, decode, incomplete));
    StringCompressor::RemoveReference();
}
