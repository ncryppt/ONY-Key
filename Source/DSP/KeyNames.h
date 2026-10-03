#pragma once

#include <juce_core/juce_core.h>

namespace onykey
{

/** Key indices used everywhere: 0-11 = major keys with tonic pitch class
    0 (C) .. 11 (B), 12-23 = minor keys with tonic (index - 12). */
inline bool isMinorKey (int key) noexcept      { return key >= 12; }
inline int  tonicOf (int key) noexcept         { return key % 12; }
inline int  relativeKeyOf (int key) noexcept   { return isMinorKey (key) ? (tonicOf (key) + 3) % 12 : 12 + (tonicOf (key) + 9) % 12; }

/** Pitch-class name, spelled the way the key is conventionally written
    (Db major but C# minor, Bb major and Bb minor, F# either way). Plain
    ASCII "#"/"b", as DAWs and sample packs write them; see withMusicSymbols(). */
inline juce::String tonicName (int key)
{
    static const char* const majorNames[] = { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    static const char* const minorNames[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B" };
    if (key < 0 || key >= 24)
        return "-";
    return isMinorKey (key) ? minorNames[tonicOf (key)] : majorNames[tonicOf (key)];
}

/** Pitch-class name for a single note (sharps, as most samplers/DAWs show). */
inline juce::String noteName (int pitchClass)
{
    static const char* const names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return pitchClass >= 0 && pitchClass < 12 ? juce::String (names[pitchClass]) : "-";
}

/** "F#" -> "F♯", "Bb" -> "B♭" (only an accidental straight after the note
    letter, so "Bbm" keeps its "m"). For large display text, where the real
    glyphs look right; small labels stay ASCII. */
inline juce::String withMusicSymbols (const juce::String& name)
{
    if (name.length() < 2 || ! juce::String ("ABCDEFG").containsChar (name[0]))
        return name;
    if (name[1] == '#')
        return name.substring (0, 1) + juce::String (juce::CharPointer_UTF8 ("\xe2\x99\xaf")) + name.substring (2);
    if (name[1] == 'b')
        return name.substring (0, 1) + juce::String (juce::CharPointer_UTF8 ("\xe2\x99\xad")) + name.substring (2);
    return name;
}

/** "F# minor" */
inline juce::String keyName (int key)
{
    if (key < 0 || key >= 24)
        return "-";
    return tonicName (key) + (isMinorKey (key) ? " minor" : " major");
}

/** "F#m" / "Db" — compact form used in tight spots (wheel labels). */
inline juce::String shortKeyName (int key)
{
    if (key < 0 || key >= 24)
        return "-";
    return tonicName (key) + (isMinorKey (key) ? "m" : "");
}

/** Camelot wheel number 1-12 (C major / A minor = 8). */
inline int camelotNumber (int key)
{
    const int majorTonic = isMinorKey (key) ? (tonicOf (key) + 3) % 12 : tonicOf (key);
    const int fifthsIndex = (majorTonic * 7) % 12; // C=0, G=1, D=2 ...
    return (fifthsIndex + 7) % 12 + 1;
}

/** "11A" (minor) / "8B" (major). */
inline juce::String camelotCode (int key)
{
    if (key < 0 || key >= 24)
        return "-";
    return juce::String (camelotNumber (key)) + (isMinorKey (key) ? "A" : "B");
}

/** True for keys whose signature uses flats (F, Bb, Eb, Ab, Db major and
    their relative minors), so notes can be spelled the way the key is. */
inline bool usesFlats (int key)
{
    if (key < 0 || key >= 24)
        return false;
    // Anything we already name with a flat (Eb minor, Bb minor...) spells
    // its notes with flats too.
    if (tonicName (key).containsChar ('b'))
        return true;
    const int majorTonic = isMinorKey (key) ? (tonicOf (key) + 3) % 12 : tonicOf (key);
    return majorTonic == 5 || majorTonic == 10 || majorTonic == 3 || majorTonic == 8 || majorTonic == 1;
}

/** Note name spelled for a key: "A#" in F# minor, "Bb" in G minor. */
inline juce::String noteNameInKey (int pitchClass, int key)
{
    static const char* const flatNames[] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    if (! usesFlats (key) || pitchClass < 0 || pitchClass >= 12)
        return noteName (pitchClass);
    return flatNames[pitchClass];
}

/** How `other` relates to `key`, in words producers use: "relative major",
    "parallel minor", "a fifth up"... Empty if they aren't closely related. */
inline juce::String keyRelation (int key, int other)
{
    if (key < 0 || other < 0 || key == other)
        return {};
    if (relativeKeyOf (key) == other)
        return isMinorKey (other) ? "relative minor" : "relative major";
    if (tonicOf (key) == tonicOf (other))
        return isMinorKey (other) ? "parallel minor" : "parallel major";
    if (isMinorKey (key) == isMinorKey (other))
    {
        const int up = (tonicOf (other) - tonicOf (key) + 12) % 12;
        if (up == 7) return "a fifth up";
        if (up == 5) return "a fifth down";
        if (up == 2) return "a tone up";
        if (up == 10) return "a tone down";
    }
    return {};
}

/** True if two keys mix harmonically by the Camelot rule: same number, or
    one step around the wheel with the same letter. */
inline bool camelotCompatible (int a, int b)
{
    if (a < 0 || b < 0 || a == b)
        return false;
    const int na = camelotNumber (a), nb = camelotNumber (b);
    if (na == nb)
        return true;
    const int diff = (na - nb + 12) % 12;
    return isMinorKey (a) == isMinorKey (b) && (diff == 1 || diff == 11);
}

} // namespace onykey
