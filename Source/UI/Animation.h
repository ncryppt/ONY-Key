#pragma once

#include <cmath>

namespace onykey::ui
{

/** A value that eases towards a target, frame-rate independently:
    `tick (dt)` moves it ~63% of the remaining way every `timeConstant` s. */
struct Eased
{
    float value = 0.0f, target = 0.0f, timeConstant = 0.12f;

    void snap (float v) noexcept  { value = target = v; }
    void set (float t) noexcept   { target = t; }

    /** Returns true while still moving (i.e. a repaint is needed). */
    bool tick (float dt) noexcept
    {
        if (std::abs (target - value) < 0.0005f)
        {
            value = target;
            return false;
        }

        value += (target - value) * (1.0f - std::exp (-dt / timeConstant));
        return true;
    }
};

inline float easeOutCubic (float t) noexcept
{
    const float u = 1.0f - (t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t);
    return 1.0f - u * u * u;
}

} // namespace onykey::ui
