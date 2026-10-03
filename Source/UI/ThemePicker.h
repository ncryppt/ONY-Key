#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "Animation.h"

namespace onykey::ui
{

/** Header pill showing the current theme (swatch + name). Clicking opens a
    menu of ONY Verb's theme palettes, grouped Dark / Light, each with a
    colour swatch, plus the "Hide NSFW themes" toggle. Applying a theme is
    the editor's job, via the callbacks. */
class ThemePicker final : public juce::Button
{
public:
    std::function<void (int)> onThemeChosen;
    std::function<void (bool)> onHideNsfwChanged;

    ThemePicker() : juce::Button ("Theme")
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip ("Theme");
        hover.timeConstant = 0.08f;
    }

    void setCurrent (int index, bool hideNsfwThemes)
    {
        current = index;
        hideNsfw = hideNsfwThemes;
        repaint();
    }

    void tick (float dt)
    {
        if (hover.tick (dt))
            repaint();
    }

    void paintButton (juce::Graphics& g, bool isHighlighted, bool isDown) override
    {
        hover.set (isHighlighted ? 1.0f : 0.0f);

        auto r = getLocalBounds().toFloat().reduced (2.0f);
        const float radius = r.getHeight() * 0.5f;
        if (isDown)
            Theme::fillRecessed (g, r, radius);
        else
        {
            Theme::dropShadow (g, r, radius, 0.45f, 6, 2);
            Theme::fillRaised (g, r, radius);
            if (hover.value > 0.01f)
            {
                g.setColour (Theme::accent.withAlpha (0.4f * hover.value));
                g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
            }
        }

        auto content = r.reduced (radius * 0.55f, 0.0f);

        // Live swatch: the current accent (tracks Acid Trip's hue cycle).
        auto swatch = content.removeFromLeft (r.getHeight() * 0.56f).withSizeKeepingCentre (r.getHeight() * 0.56f, r.getHeight() * 0.56f);
        g.setColour (Theme::accent.withAlpha (0.3f));
        g.fillEllipse (swatch.expanded (2.5f));
        g.setColour (Theme::accent);
        g.fillEllipse (swatch);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.fillEllipse (swatch.reduced (swatch.getWidth() * 0.3f).translated (-swatch.getWidth() * 0.12f, -swatch.getHeight() * 0.12f).withSizeKeepingCentre (swatch.getWidth() * 0.25f, swatch.getWidth() * 0.25f));

        // Chevron.
        auto chevronArea = content.removeFromRight (10.0f);
        juce::Path chevron;
        const auto cc = chevronArea.getCentre().translated (0.0f, isDown ? 1.0f : 0.0f);
        chevron.startNewSubPath (cc.x - 3.5f, cc.y - 1.8f);
        chevron.lineTo (cc.x, cc.y + 1.8f);
        chevron.lineTo (cc.x + 3.5f, cc.y - 1.8f);
        const auto ink = Theme::textSecondary.interpolatedWith (Theme::textPrimary, hover.value);
        g.setColour (ink);
        g.strokePath (chevron, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        content.removeFromLeft (8.0f);
        g.setFont (Theme::labelFont (10.0f));
        g.drawText (juce::String (Theme::getThemePalettes()[(size_t) juce::jlimit (0, 17, current)].name).toUpperCase(),
                    content, juce::Justification::centredLeft, true);
    }

    void clicked() override
    {
        constexpr int hideNsfwId = 1000;
        const auto& palettes = Theme::getThemePalettes();

        juce::PopupMenu menu;
        bool lightHeading = false;
        menu.addSectionHeader ("DARK");
        for (size_t i = 0; i < palettes.size(); ++i)
        {
            const auto& p = palettes[i];
            if (hideNsfw && Theme::isNsfwTheme (p) && (int) i != current)
                continue;
            if (p.isLight && ! lightHeading)
            {
                menu.addSeparator();
                menu.addSectionHeader ("LIGHT");
                lightHeading = true;
            }

            juce::PopupMenu::Item item (p.name);
            item.setID ((int) i + 1).setTicked ((int) i == current);
            item.setImage (std::make_unique<juce::DrawableImage> (makeSwatch (p)));
            menu.addItem (std::move (item));
        }

        menu.addSeparator();
        menu.addItem (hideNsfwId, "Hide NSFW themes", true, hideNsfw);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth() + 30),
                            [safeThis = juce::Component::SafePointer<ThemePicker> (this)] (int result)
                            {
                                if (safeThis == nullptr || result == 0)
                                    return;
                                if (result == hideNsfwId)
                                {
                                    if (safeThis->onHideNsfwChanged)
                                        safeThis->onHideNsfwChanged (! safeThis->hideNsfw);
                                }
                                else if (safeThis->onThemeChosen)
                                {
                                    safeThis->onThemeChosen (result - 1);
                                }
                            });
    }

private:
    /** Round swatch: the theme's ground with its accent as a disc inside. */
    static juce::Image makeSwatch (const Theme::ThemePalette& p)
    {
        constexpr int size = 36;
        juce::Image image (juce::Image::ARGB, size, size, true);
        juce::Graphics g (image);
        const auto r = image.getBounds().toFloat().reduced (2.0f);
        g.setColour (p.background);
        g.fillEllipse (r);
        g.setColour (p.isLight ? p.hairline.darker (0.2f) : p.hairline.brighter (0.4f));
        g.drawEllipse (r, 2.0f);
        g.setColour (p.accent);
        g.fillEllipse (r.reduced (r.getWidth() * 0.26f));
        return image;
    }

    int current = Theme::defaultThemeIndex;
    bool hideNsfw = false;
    Eased hover;
};

} // namespace onykey::ui
