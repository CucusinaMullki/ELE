#include "Physics/PhysicsDebugDraw.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyFilter.h>

#ifdef JPH_DEBUG_RENDERER
#include <Jolt/Renderer/DebugRendererSimple.h>

namespace
{
    class LineCollector final : public JPH::DebugRendererSimple
    {
    public:
        std::vector<DebugLine>* out = nullptr;
        uint32_t overrideColor = 0;

        void DrawLine(JPH::RVec3Arg a, JPH::RVec3Arg b, JPH::ColorArg c) override
        {
            const uint32_t col = overrideColor ? overrideColor
                : (uint32_t)c.r | ((uint32_t)c.g << 8) | ((uint32_t)c.b << 16) | ((uint32_t)c.a << 24);
            out->push_back({ { (float)a.GetX(), (float)a.GetY(), (float)a.GetZ() },
                             { (float)b.GetX(), (float)b.GetY(), (float)b.GetZ() }, col });
        }

        void DrawText3D(JPH::RVec3Arg, const std::string_view&, JPH::ColorArg, float) override {}
    };

    class Filter final : public JPH::BodyDrawFilter
    {
    public:
        bool sensors = false;
        bool statics = true;
        bool ShouldDraw(const JPH::Body& b) const override
        {
            if (b.IsSensor() != sensors) return false;
            if (!statics && b.IsStatic()) return false;
            return true;
        }
    };

    LineCollector& collector()
    {
        static LineCollector* c = new LineCollector();
        return *c;
    }
}

bool collectPhysicsDebugLines(JPH::PhysicsSystem& system, const PhysicsDebugOptions& opt,
    glm::vec3 cameraPos, std::vector<DebugLine>& out)
{
    LineCollector& r = collector();
    r.out = &out;
    r.SetCameraPos(JPH::RVec3(cameraPos.x, cameraPos.y, cameraPos.z));

    JPH::BodyManager::DrawSettings s;
    s.mDrawShape = true;
    s.mDrawShapeWireframe = true;
    s.mDrawShapeColor = JPH::BodyManager::EShapeColor::MotionTypeColor;
    s.mDrawBoundingBox = opt.boundingBoxes;

    Filter f;
    f.statics = opt.staticBodies;
    if (opt.colliders)
    {
        f.sensors = false;
        r.overrideColor = 0;
        system.DrawBodies(s, &r, &f);
    }
    if (opt.triggers)
    {
        f.sensors = true;
        f.statics = true;
        r.overrideColor = opt.triggerColor;
        system.DrawBodies(s, &r, &f);
    }
    r.overrideColor = 0;
    r.NextFrame();
    r.out = nullptr;
    return true;
}

#else

bool collectPhysicsDebugLines(JPH::PhysicsSystem&, const PhysicsDebugOptions&, glm::vec3, std::vector<DebugLine>&)
{
    return false;
}

#endif
