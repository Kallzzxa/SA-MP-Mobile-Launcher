# Desain Registrasi Custom Model 0.3.DL — SA-MP Mobile

## Temuan RE (2026-10-14, rlnmy)

Binary `libGTASA.so` (armeabi-v7a, 60MB) **tidak di-strip** — simbol C++ lengkap tersedia.
Lokasi: `~/workspace/upstream-peria/app/src/main/jniLibs/armeabi-v7a/libGTASA.so`

### Fungsi kunci (alamat = VA Thumb, simbol dari `nm`)

| Fungsi | Alamat | Kegunaan |
|--------|--------|----------|
| `CStreaming::AddImageToList(const char*, bool)` | 0x2cf7d1 | Daftarkan IMG archive ke streaming system |
| `CStreaming::RequestModel(int, int)` | 0x2d299d | Request model untuk di-load |
| `CModelInfo::AddAtomicModel(int)` | 0x385a39 | Alokasi slot model object (56 byte/entry) |
| `CModelInfo::AddClumpModel(int)` | 0x3860a5 | Alokasi slot model ped/vehicle |
| `CModelInfo::ms_modelInfoPtrs` | 0x91dcb8 (BSS) | Array pointer model info, index = model ID |

### Cara kerja AddAtomicModel (dari disassembly)
1. `ms_numModels++`
2. Hitung pointer slot baru: `&ms_modelInfo[numModels]` (tiap entry 56 byte)
3. Panggil virtual constructor (vtable[7])
4. Simpan di `ms_modelInfoPtrs[modelId] = slot_baru`
5. Return pointer model info

## Desain implementasi

### Alur
```
Downloader selesai (DFF+TXD di disk)
    → Pack ke custom IMG v2 (misal: /data/.../custom_dl.img)
    → Panggil CStreaming::AddImageToList("custom_dl.img", false)
    → Untuk tiap custom model ID:
        → CModelInfo::AddAtomicModel(modelId) atau AddClumpModel(modelId)
        → Set nama model di struct (agar streaming bisa cari file by name)
    → CStreaming::RequestModel(modelId) saat server spawn entity
    → Game load DFF/TXD dari IMG custom via jalur normal
```

### Detail teknis
1. **IMG v2 format:** Header "VER2", diikuti directory entries (offset, size, name[24]). Pack DFF/TXD yang sudah di-download + CRC32 verified.
2. **Nama file:** Harus cocok dengan nama model yang di-register (streaming cari by name hash). Dari protokol 0.3.DL, nama file dikirim server atau di-generate dari model ID.
3. **Model ID:** Pakai range custom (20000+ atau via `GetVirtualModelId()` yang sudah ada di codebase untuk remap).
4. **Kapan register:** Setelah SEMUA download selesai (atau per-batch), sebelum server spawn entity. Hook di titik setelah `ProcessTick` selesai.
5. **Fallback:** Jika model belum ter-register saat server request → fallback ke model default (jangan crash). Ini mitigasi sementara yang disarankan.

### Yang TIDAK perlu
- Hook manual ke internals streaming (terlalu rumit) — `AddImageToList` + `AddAtomicModel` cukup karena simbol tersedia.
- Modifikasi libGTASA.so itu sendiri.

### Risiko
- Format TXD/DFF: file dari server PC mungkin tidak kompatibel dengan renderer Android (kompresi tekstur DXT vs ETC). Perlu validasi atau konversi.
- Ukuran IMG: 325 model bisa besar — pastikan storage cukup dan loading tidak blocking UI thread.

## Referensi
- Disassembly `AddAtomicModel`: lihat catatan RE 2026-10-14
- Dokumen protokol: `~/workspace/docs_03dl/part1_protocol.md`
- Customskin reference: `~/workspace/ref_customskin`
