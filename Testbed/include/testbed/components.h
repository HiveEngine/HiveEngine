#pragma once

#include <waggle/components.h>

using namespace waggle;

#include <queen/reflect/component_reflector.h>
#include <queen/reflect/field_attributes.h>

// Orbit behavior: circles around a target point
struct OrbitCamera
{
    hive::math::Float3 target{0.f, 0.f, 0.f};
    float radius{3.f};
    float height{1.5f};
    float speed{1.f};

    static void Reflect(queen::ComponentReflector<>& r)
    {
        r.Field("target", &OrbitCamera::target);
        r.Field("radius", &OrbitCamera::radius).Range(0.1f, 100.f);
        r.Field("height", &OrbitCamera::height);
        r.Field("speed", &OrbitCamera::speed).Range(0.f, 10.f);
    }
};

// Free-look FPS camera (WASD + mouse)
struct FreeCamera
{
    float move_speed{5.f};
    float look_sensitivity{0.003f};
    float yaw{0.f};
    float pitch{0.f};

    static void Reflect(queen::ComponentReflector<>& r)
    {
        r.Field("move_speed", &FreeCamera::move_speed).Range(0.1f, 50.f);
        r.Field("look_sensitivity", &FreeCamera::look_sensitivity).Range(0.0001f, 0.01f);
        r.Field("yaw", &FreeCamera::yaw).Flag(queen::FieldFlag::Angle);
        r.Field("pitch", &FreeCamera::pitch).Flag(queen::FieldFlag::Angle);
    }
};

// Continuous rotation around an axis
struct Spin
{
    hive::math::Float3 axis{0.f, 1.f, 0.f};
    float speed{1.f};

    static void Reflect(queen::ComponentReflector<>& r)
    {
        r.Field("axis", &Spin::axis);
        r.Field("speed", &Spin::speed).Range(0.f, 20.f);
    }
};
