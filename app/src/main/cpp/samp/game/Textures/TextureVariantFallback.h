//
// Texture database variant fallback.
//
// The engine picks a texture variant from what the GPU advertises (dxt, etc,
// pvr, 360 ...) and asks TextureDatabase::LoadThumbs for exactly that file:
// "texdb/<db>/<db>.<suffix>.tmb". A data set that ships several variants but
// not the one this GPU asked for leaves every texture in that database missing,
// which the renderer shows as black buildings, black pedestrians and a black
// radar map, and which also feeds the RLE decompressor data it cannot decode.
//
// This hook keeps the engine in charge but asks for a variant that actually
// exists, so the same database loads instead of coming out empty.
//

#pragma once

class TextureVariantFallback
{
public:
    // Installs the TextureDatabase::LoadThumbs hook. Safe to call once.
    static void Initialise();
};
