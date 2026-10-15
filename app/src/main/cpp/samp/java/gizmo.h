//
// Gizmo 3D untuk EditObject ala PC
// Render panah 3D (X=merah, Y=hijau, Z=biru) di sekitar object yang diedit
//

#pragma once

#include "game/common.h"
#include "game/Lines.h"

class CGizmo {
public:
    enum eAxis {
        AXIS_NONE = -1,
        AXIS_X = 0,
        AXIS_Y = 1,
        AXIS_Z = 2
    };

    // Panjang panah gizmo (dalam unit world)
    static constexpr float GIZMO_LENGTH = 2.0f;

    // Render gizmo di posisi object. activeAxis = axis yang sedang aktif (highlight)
    static void Render(const CVector& pos, eAxis activeAxis);

    // Set/get axis aktif (dipanggil dari Java via JNI saat user ganti mode)
    static void SetActiveAxis(eAxis axis);
    static eAxis GetActiveAxis();

private:
    static eAxis s_activeAxis;
};
