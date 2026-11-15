#include "../../app/src/main/cpp/samp/net/passenger_sync_fields.h"
#include "../../app/src/main/cpp/samp/net/remote_sync_packet_reader.h"
#include "../../app/src/main/cpp/samp/net/object_move_reader.h"
#include "BitStream.h"
#include <cassert>
#include <cstddef>
#include <cstring>
#include <limits>
#include <initializer_list>

#pragma pack(push, 1)
struct PassengerPayload {
    uint16_t vehicle, flags;
    uint8_t health, armour;
    uint16_t lrAnalog, udAnalog, wKeys;
    float pos[3];
};
#pragma pack(pop)
static_assert(sizeof(PassengerPayload) == 24, "wire payload size");
static_assert(offsetof(PassengerPayload, flags) == 2 && offsetof(PassengerPayload, pos) == 12, "wire offsets");

struct FakePlayer {
    uint16_t lr = 0, ud = 0, keys = 0;
    void SetKeys(uint16_t a, uint16_t b, uint16_t c) { lr = a; ud = b; keys = c; }
};

int main() {
    // Exercise bit reads and stream-to-stream copies over byte boundaries.
    RakNet::BitStream bits;
    bits.Write(uint8_t{0x01}); bits.Write(uint8_t{0x80});
    bool bit = false;
    for (int i = 0; i < 16; ++i) {
        assert(bits.Read(bit));
        assert(bit == (i == 7 || i == 8));
    }
    assert(!bits.Read(bit));
    bits.ResetReadPointer();
    RakNet::BitStream copy;
    copy.Write1();
    copy.Write(&bits, 16);
    assert(copy.Read(bit) && bit);
    for (int i = 0; i < 16; ++i) {
        assert(copy.Read(bit));
        assert(bit == (i == 7 || i == 8));
    }
    for (uint8_t seat = 1; seat <= 7; ++seat) {
        for (uint8_t extra = 0; extra < 4; ++extra) {
            auto flags = MakePassengerSyncFlags(seat, true, false, 29, extra);
            assert(PassengerSeat(flags) == seat && PassengerWeapon(flags) == 29 && PassengerDriveBy(flags));
            assert(((flags >> 14) & 3) == extra);
        }
    }
    assert(MakePassengerSyncFlags(7, true, false, 29, 3) == 0xdd47);
    assert(MakePassengerSyncFlags(7, false, true, 29, 0) == 0x1d87);
    PassengerPayload payload{0x1234, 0xdd47, 100, 50, 0x1234, 0xff80, 0x0024, {1, 2, 3}};
    FakePlayer player;
    ApplyPassengerKeys(player, payload);
    assert(player.lr == 0x1234 && player.ud == 0xff80 && player.keys == 0x0024);

    RakNet::BitStream packet;
    packet.Write(uint8_t{211}); packet.Write(uint16_t{0x5678});
    packet.Write(reinterpret_cast<const char*>(&payload), sizeof(payload));
    assert(packet.GetNumberOfBytesUsed() == 27);
    auto data = packet.GetData();
    assert(data[0] == 211 && data[1] == 0x78 && data[2] == 0x56 &&
           data[3] == 0x34 && data[4] == 0x12 && data[5] == 0x47 && data[6] == 0xdd);
    for (unsigned n = 0; n <= 27; ++n) {
        RakNet::BitStream input(data, n, false);
        uint16_t id = 0;
        PassengerPayload result{};
        bool ok = ReadRemoteSyncPacket(input, id, result);
        assert(ok == (n == 27));
        if (ok) assert(id == 0x5678 && std::memcmp(&result, &payload, sizeof(payload)) == 0);
    }

    RakNet::BitStream move;
    move.Write(uint16_t{42});
    for (float value : {0.f, 0.f, 0.f, 1.f, 2.f, 3.f, 5.f, -1000.f, -1000.f, -1000.f}) move.Write(value);
    assert(move.GetNumberOfBitsUsed() == 336);
    for (int n = 0; n <= 336; ++n) {
        RakNet::BitStream input(move.GetData(), move.GetNumberOfBytesUsed(), false);
        input.SetWriteOffset(n);
        ObjectMoveData result{};
        bool ok = ReadObjectMove(input, result);
        assert(ok == (n == 336));
        if (ok) assert(result.id == 42 && result.speed == 5.f && result.z == 3.f && result.rotX == -1000.f);
    }
    for (float bad : {0.f, -1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        RakNet::BitStream input;
        input.Write(uint16_t{42});
        for (float v : {0.f, 0.f, 0.f, 1.f, 2.f, 3.f, bad, 0.f, 0.f, 0.f}) input.Write(v);
        ObjectMoveData result{};
        assert(!ReadObjectMove(input, result));
    }
}
