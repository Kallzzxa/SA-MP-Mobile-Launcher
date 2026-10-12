// ====================================================================
// Nusantara Roleplay - Custom 0.3.DL Model Loader System
// Integrasi AddSimpleModel dan AddCharModel untuk open.mp
// ====================================================================

#if defined _CUSTOM_MODELS_LOADER_INC
    #endinput
#endif
#define _CUSTOM_MODELS_LOADER_INC

// Inisialisasi daftar custom model saat gamemode start
stock LoadCustom03DLModels()
{
    print("[0.3.DL] Inisialisasi custom models untuk client sync...");

    // Contoh registrasi custom simple object model:
    // AddSimpleModel(virtualWorld, baseid, newid, const dff[], const textureLibrary[]);
    // ID -2000 memakai base model 19379 (cube/building base)
    new bool:success = AddSimpleModel(-1, 19379, -2000, "sample_cube.dff", "sample_cube.txd");
    if (success) {
        printf("[0.3.DL] Model berhasil terdaftar: ID -2000 (sample_cube.dff)");
    } else {
        printf("[0.3.DL] Gagal mendaftarkan custom model - pastikan open.mp custom models component aktif");
    }

    return 1;
}

// Callback saat player selesai download custom model
public OnPlayerFinishedDownloading(playerid, virtualworld)
{
    SendClientMessage(playerid, 0x2563EBFF, "[SERVER] Semua aset custom model 0.3.DL berhasil dimuat.");
    return 1;
}
