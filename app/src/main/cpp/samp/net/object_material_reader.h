#pragma once

#include <cstdint>

// One SA-MP object material. Parsing is bounded independently of rendering.
struct ObjectMaterialData {
    uint8_t type = 0, index = 0, size = 0, fontSize = 0, bold = 0, alignment = 0;
    uint16_t model = 0;
    uint32_t color = 0, fontColor = 0, backgroundColor = 0;
    char txd[256] = {}, texture[256] = {}, font[256] = {}, text[2048] = {};
};

template <typename Stream>
bool ReadMaterialName(Stream& stream, char (&destination)[256]) {
    uint8_t length = 0;
    if (!stream.Read(length) || (length != 0 && !stream.Read(destination, length))) return false;
    destination[length] = '\0';
    return true;
}

template <typename Stream, typename DecodeText>
bool ReadObjectMaterial(Stream& stream, DecodeText decode, ObjectMaterialData& out) {
    if (!stream.Read(out.type) || !stream.Read(out.index)) return false;
    if (out.type == 1) {
        return stream.Read(out.model) && ReadMaterialName(stream, out.txd) &&
            ReadMaterialName(stream, out.texture) && stream.Read(out.color);
    }
    if (out.type == 2) {
        return stream.Read(out.size) && ReadMaterialName(stream, out.font) &&
            stream.Read(out.fontSize) && stream.Read(out.bold) &&
            stream.Read(out.fontColor) && stream.Read(out.backgroundColor) &&
            stream.Read(out.alignment) && decode(out.text, sizeof(out.text), stream);
    }
    return false;
}
