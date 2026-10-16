//
// Gizmo 3D untuk EditObject ala PC
// Render panah 3D (X=merah, Y=hijau, Z=biru) di sekitar object yang diedit
//

#include "gizmo.h"

CGizmo::eAxis CGizmo::s_activeAxis = CGizmo::AXIS_X;

// Warna axis: X=merah, Y=hijau, Z=biru (konvensi standar)
static uint32_t GetAxisColor(CGizmo::eAxis axis, bool active) {
    uint8_t a = active ? 255 : 160;
    switch (axis) {
        case CGizmo::AXIS_X: return (a << 24) | (255 << 16) | (60 << 8) | 60;
        case CGizmo::AXIS_Y: return (a << 24) | (60 << 16) | (255 << 8) | 60;
        case CGizmo::AXIS_Z: return (a << 24) | (80 << 16) | (120 << 8) | 255;
        default: return 0;
    }
}

void CGizmo::Render(const CVector& pos, eAxis activeAxis) {
    const float L = GIZMO_LENGTH;

    // X axis (merah) — garis + arrowhead
    {
        CVector end(pos.x + L, pos.y, pos.z);
        uint32_t col = GetAxisColor(AXIS_X, activeAxis == AXIS_X);
        CLines::RenderLineNoClipping(pos, end, col, col);
        CLines::RenderLineNoClipping(end, CVector(end.x - 0.3f, end.y + 0.18f, end.z), col, col);
        CLines::RenderLineNoClipping(end, CVector(end.x - 0.3f, end.y - 0.18f, end.z), col, col);
    }
    // Y axis (hijau)
    {
        CVector end(pos.x, pos.y + L, pos.z);
        uint32_t col = GetAxisColor(AXIS_Y, activeAxis == AXIS_Y);
        CLines::RenderLineNoClipping(pos, end, col, col);
        CLines::RenderLineNoClipping(end, CVector(end.x + 0.18f, end.y - 0.3f, end.z), col, col);
        CLines::RenderLineNoClipping(end, CVector(end.x - 0.18f, end.y - 0.3f, end.z), col, col);
    }
    // Z axis (biru)
    {
        CVector end(pos.x, pos.y, pos.z + L);
        uint32_t col = GetAxisColor(AXIS_Z, activeAxis == AXIS_Z);
        CLines::RenderLineNoClipping(pos, end, col, col);
        CLines::RenderLineNoClipping(end, CVector(end.x + 0.18f, end.y, end.z - 0.3f), col, col);
        CLines::RenderLineNoClipping(end, CVector(end.x - 0.18f, end.y, end.z - 0.3f), col, col);
    }
}

void CGizmo::SetActiveAxis(eAxis axis) {
    s_activeAxis = axis;
}

CGizmo::eAxis CGizmo::GetActiveAxis() {
    return s_activeAxis;
}
