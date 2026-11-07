#include "../../app/src/main/cpp/samp/net/dl/dl_imgpack.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>

int main() {
    char dir[] = "/tmp/hopepride-img-test-XXXXXX";
    assert(mkdtemp(dir));
    std::string source = std::string(dir) + "/input.dff";
    std::string output = std::string(dir) + "/output.img";
    FILE* file = fopen(source.c_str(), "wb");
    assert(file && fwrite("DFF", 1, 3, file) == 3 && fclose(file) == 0);
    std::string error;
    assert(DLImgPack::PackImgV2({{source, "model.dff"}}, output, error));
    file = fopen(output.c_str(), "rb");
    assert(file);
    unsigned char header[40];
    assert(fread(header, 1, sizeof(header), file) == sizeof(header));
    assert(memcmp(header, "VER2", 4) == 0 && header[4] == 1);
    assert(header[8] == 1 && header[12] == 1);
    assert(memcmp(header + 16, "model.dff", 9) == 0);
    assert(fseek(file, 2048, SEEK_SET) == 0);
    char payload[3];
    assert(fread(payload, 1, 3, file) == 3 && memcmp(payload, "DFF", 3) == 0);
    assert(fclose(file) == 0);
    assert(!DLImgPack::PackImgV2({{source, "../bad.dff"}}, output, error));
    // A rejected pack must leave the previous output in place.
    file = fopen(output.c_str(), "rb");
    assert(file && fread(header, 1, 4, file) == 4 && memcmp(header, "VER2", 4) == 0);
    assert(fclose(file) == 0);
    assert(remove(source.c_str()) == 0 && remove(output.c_str()) == 0 && rmdir(dir) == 0);
}
