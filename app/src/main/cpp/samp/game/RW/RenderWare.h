#pragma once

#include "types.h"
#include "color.h"
#include "rwcore.h"
#include "rtquat.h"
#include "rphanim.h"
#include "rpskin.h"

#include "rpworld.h"
#include "RwHelper.h"

#include "stream.h"

struct RwTexDictionary;

extern RwStream* (*RwStreamOpen)(
    RwStreamType type,
    RwStreamAccessType accessType,
    const void* data
);

extern RwBool (*RwStreamFindChunk)(
    RwStream* stream,
    RwUInt32 type,
    RwUInt32* lengthOut,
    RwUInt32* versionOut
);

extern RwBool (*RwStreamClose)(RwStream* stream, void* data);

extern RwTexDictionary* (*RwTexDictionaryGtaStreamRead)(
    RwStream* stream
);

extern RpClump* (*RpClumpStreamRead)(RwStream* stream);

/* skeleton.h */
typedef struct RsGlobalType RsGlobalType;
struct RsGlobalType
{
    const RwChar *appName;
    RwInt32 maximumWidth;
    RwInt32 maximumHeight;
    RwInt32 maxFPS;
    RwBool quit;
    // useless stuff
    // ...
};

#define RWRSTATE(a) (reinterpret_cast<void *>(a))
#define RWRGBALONG(r,g,b,a) ((unsigned int) (((a) << 24) | ((r) << 16) | ((g) << 8) | (b)))

/* rtpng.h */
extern RwImage* (*RtPNGImageWrite)(RwImage* image, const RwChar* imageName);
extern RwImage* (*RtPNGImageRead)(const RwChar* imageName);

extern RwBool (*RwTextureDestroy)(RwTexture* texture);

extern RsGlobalType* RsGlobal;