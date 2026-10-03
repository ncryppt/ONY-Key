#pragma once

#include <juce_graphics/juce_graphics.h>
#include <cmath>

namespace onykey::ui
{

/** ONY Verb's stylised seven-blade leaf (LeafShape.h) for the Kush Koma
    theme: blades meet at the origin, the longest pointing up; `size` is its
    length. */
inline juce::Path makeLeafPath (float size)
{
    struct Blade { float angleDeg, lengthMul, widthMul; };
    static const Blade blades[] = {
        { -90.0f, 1.00f, 0.16f },
        { -55.0f, 0.86f, 0.17f }, { -125.0f, 0.86f, 0.17f },
        { -20.0f, 0.64f, 0.18f }, { -160.0f, 0.64f, 0.18f },
        {  18.0f, 0.40f, 0.16f }, { -198.0f, 0.40f, 0.16f },
    };

    juce::Path path;
    for (const auto& b : blades)
    {
        const float a = juce::degreesToRadians (b.angleDeg);
        const juce::Point<float> dir (std::cos (a), std::sin (a)), perp (-dir.y, dir.x), base;
        const auto tip = base + dir * (size * b.lengthMul);
        const auto mid = base + dir * (size * b.lengthMul * 0.55f);
        path.startNewSubPath (base);
        path.quadraticTo (mid + perp * (size * b.widthMul), tip);
        path.quadraticTo (mid - perp * (size * b.widthMul), base);
        path.closeSubPath();
    }
    return path;
}

/** ONY Verb's chubby beaver silhouette (BeaverShape.h) for the Canada Eh?
    theme: paddle tail left, head right, roughly centred on the body; `size`
    is about nose to tail tip. */
inline juce::Path makeBeaverPath (float size)
{
    juce::Path path;
    path.addEllipse (-size * 0.66f, -size * 0.08f, size * 0.40f, size * 0.16f); // flat paddle tail
    path.addEllipse (-size * 0.34f, -size * 0.22f, size * 0.60f, size * 0.44f); // body
    path.addEllipse ( size * 0.18f, -size * 0.18f, size * 0.34f, size * 0.32f); // head
    path.addEllipse ( size * 0.24f, -size * 0.30f, size * 0.10f, size * 0.10f); // ear
    path.addEllipse ( size * 0.40f, -size * 0.30f, size * 0.10f, size * 0.10f); // ear
    return path;
}

} // namespace onykey::ui
