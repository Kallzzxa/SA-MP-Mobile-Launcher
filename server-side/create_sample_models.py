# RenderWare DFF & TXD Sample Files
# Sesuai RenderWare Binary Stream Specification:
# DFF Section ID: 0x10 (Clump), Header: Type (uint32), Size (uint32), Version (uint32)
# TXD Section ID: 0x16 (Texture Dictionary)
import struct

def create_sample_dff(filename):
    # Minimalist valid RenderWare Clump header
    section_id = 0x10 # rwID_CLUMP
    version = 0x1803FFFF # 3.6.0.3 (GTA SA version)
    body = b"SAMP_03DL_SAMPLE_DFF_OBJECT"
    size = len(body)
    header = struct.pack("<III", section_id, size, version)
    with open(filename, "wb") as f:
        f.write(header + body)

def create_sample_txd(filename):
    section_id = 0x16 # rwID_TEXDICTIONARY
    version = 0x1803FFFF # 3.6.0.3 (GTA SA version)
    body = b"SAMP_03DL_SAMPLE_TXD_TEXTURE"
    size = len(body)
    header = struct.pack("<III", section_id, size, version)
    with open(filename, "wb") as f:
        f.write(header + body)

if __name__ == "__main__":
    create_sample_dff("models/sample_cube.dff")
    create_sample_txd("models/sample_cube.txd")
    print("Sample DFF and TXD generated.")
