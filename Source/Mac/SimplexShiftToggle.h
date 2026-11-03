//
// SimplexShiftToggle.h
//
// Built-in Simplex only: a bare Shift tap toggles Chinese and English.
// Header-only so the logic can be tested with clang++ without the app target.
//
// Modifier bits checked against the Command Line Tools SDK on 2026-11-03:
// NSEventModifierFlagShift = 1 << 17, NX_DEVICELSHIFTKEYMASK = 0x2,
// NX_DEVICERSHIFTKEYMASK = 0x4, kVK_Shift = 0x38, kVK_RightShift = 0x3C.

#ifndef SimplexShiftToggle_h
#define SimplexShiftToggle_h

#include <stdint.h>
#include <string.h>

namespace OpenVanilla {

static const char kBuiltInSimplexIdentifier[] = "org.openvanilla.OVIMTableBased.simplex";
static const uint32_t kGenericShiftBit = 1u << 17;
static const uint32_t kDeviceLeftShiftBit = 0x00000002u;
static const uint32_t kDeviceRightShiftBit = 0x00000004u;
static const uint16_t kLeftShiftKeyCode = 56;
static const uint16_t kRightShiftKeyCode = 60;
static const uint64_t kSimplexShiftMaxTapDurationNs = 500000000ull;

enum class SimplexShiftAction {
    None,
    Toggle
};

enum class SimplexKeyRoute {
    Standard,
    EnglishPassThrough
};

// Chinese plus pending: the tap switches to English and the caller must drop
// the uncommitted radicals, candidates and associated-phrase UI without
// inserting them. English plus pending still does not switch. Nothing is
// stored for a later automatic toggle.
enum class SimplexShiftResolution {
    NotATap,
    Switched,
    PreservedPending,
    CancelPendingAndSwitch
};

enum class SimplexPunctuationDecision {
    NotSpecial,
    // Insert the ASCII base, then the existing half-width-to-full-width
    // preference. The caller must not skip that preference.
    EmitThroughWidthFilter,
    // Pending Chinese stays. No punctuation is inserted, committed, discarded
    // or stored for a later key.
    KeepPending
};

inline SimplexShiftResolution ApplySimplexShiftTap(class SimplexShiftToggle &session, bool validTap, bool hasPendingComposition);

struct SimplexShiftSides {
    bool leftDown;
    bool rightDown;
};

inline SimplexShiftSides SidesFromModifierBits(uint32_t modifierFlags, uint16_t keyCode)
{
    SimplexShiftSides sides;
    sides.leftDown = (modifierFlags & kDeviceLeftShiftBit) != 0;
    sides.rightDown = (modifierFlags & kDeviceRightShiftBit) != 0;
    if (!sides.leftDown && !sides.rightDown && (modifierFlags & kGenericShiftBit) != 0) {
        if (keyCode == kLeftShiftKeyCode) {
            sides.leftDown = true;
        } else if (keyCode == kRightShiftKeyCode) {
            sides.rightDown = true;
        }
    }
    return sides;
}

inline bool IsBuiltInSimplexTarget(const char *identifier, bool customTableOverridesBuiltIn)
{
    if (customTableOverridesBuiltIn || identifier == 0 || identifier[0] == '\0') {
        return false;
    }
    return strcmp(identifier, kBuiltInSimplexIdentifier) == 0;
}

inline SimplexKeyRoute RouteSimplexKeyDown(bool builtInSimplex, bool englishMode)
{
    if (builtInSimplex && englishMode) {
        return SimplexKeyRoute::EnglishPassThrough;
    }
    return SimplexKeyRoute::Standard;
}

struct SimplexShiftEvent {
    bool leftDown;
    bool rightDown;
    bool commandDown;
    bool optionDown;
    bool controlDown;
    uint64_t timeNs;
};

class SimplexShiftToggle {
public:
    SimplexShiftToggle()
        : englishMode_(false)
        , leftDown_(false)
        , rightDown_(false)
        , armed_(SideNone)
        , armedAtNs_(0)
        , dirty_(false)
        , modeTooltipShown_(false)
    {
    }

    SimplexShiftAction flagsChanged(const SimplexShiftEvent &event)
    {
        const bool blocking = event.commandDown || event.optionDown || event.controlDown;
        const bool prevLeft = leftDown_;
        const bool prevRight = rightDown_;
        leftDown_ = event.leftDown;
        rightDown_ = event.rightDown;

        if (blocking || (event.leftDown && event.rightDown)) {
            disarm();
            return SimplexShiftAction::None;
        }

        const bool leftReleased = prevLeft && !event.leftDown;
        const bool rightReleased = prevRight && !event.rightDown;
        const bool leftPressed = event.leftDown && !prevLeft;
        const bool rightPressed = event.rightDown && !prevRight;
        if ((leftReleased && rightPressed) || (rightReleased && leftPressed)) {
            disarm();
            return SimplexShiftAction::None;
        }

        if (armed_ == SideLeft && leftReleased && !event.rightDown) {
            return finish(event.timeNs);
        }
        if (armed_ == SideRight && rightReleased && !event.leftDown) {
            return finish(event.timeNs);
        }

        if (leftPressed && !event.rightDown) {
            arm(SideLeft, event.timeNs);
        } else if (rightPressed && !event.leftDown) {
            arm(SideRight, event.timeNs);
        }
        return SimplexShiftAction::None;
    }

    void noteKeyDown()
    {
        if (armed_ != SideNone) {
            dirty_ = true;
        }
    }

    // Drops an in-progress tap. Does not change the Chinese/English mode.
    void cancelGesture()
    {
        disarm();
        leftDown_ = false;
        rightDown_ = false;
    }

    bool englishMode() const
    {
        return englishMode_;
    }

    void toggleEnglishMode()
    {
        englishMode_ = !englishMode_;
    }

    void markModeTooltipShown()
    {
        modeTooltipShown_ = true;
    }

    bool consumeModeTooltipShown()
    {
        const bool shown = modeTooltipShown_;
        modeTooltipShown_ = false;
        return shown;
    }

private:
    enum Side {
        SideNone,
        SideLeft,
        SideRight
    };

    void arm(Side side, uint64_t timeNs)
    {
        armed_ = side;
        armedAtNs_ = timeNs;
        dirty_ = false;
    }

    void disarm()
    {
        armed_ = SideNone;
        dirty_ = false;
    }

    SimplexShiftAction finish(uint64_t timeNs)
    {
        const bool timely = timeNs >= armedAtNs_ && (timeNs - armedAtNs_) <= kSimplexShiftMaxTapDurationNs;
        const bool accepted = !dirty_ && timely;
        disarm();
        if (!accepted) {
            return SimplexShiftAction::None;
        }
        return SimplexShiftAction::Toggle;
    }

    bool englishMode_;
    bool leftDown_;
    bool rightDown_;
    Side armed_;
    uint64_t armedAtNs_;
    bool dirty_;
    bool modeTooltipShown_;
};

inline SimplexShiftToggle &SharedSimplexShiftToggle()
{
    static SimplexShiftToggle session;
    return session;
}

inline SimplexShiftResolution ApplySimplexShiftTap(SimplexShiftToggle &session, bool validTap, bool hasPendingComposition)
{
    if (!validTap) {
        return SimplexShiftResolution::NotATap;
    }
    if (hasPendingComposition && !session.englishMode()) {
        session.toggleEnglishMode();
        return SimplexShiftResolution::CancelPendingAndSwitch;
    }
    if (hasPendingComposition) {
        return SimplexShiftResolution::PreservedPending;
    }
    session.toggleEnglishMode();
    return SimplexShiftResolution::Switched;
}

// Unshifted "." and "," for built-in Simplex, in Chinese and in Shift English,
// with or without Caps Lock. Shift, Command, Option and Control, including
// "<" and ">", stay on the existing path.
inline SimplexPunctuationDecision DecideSimplexUnshiftedPunctuation(
    bool builtInSimplex, bool englishMode, bool shiftOrChord, bool capsLock, bool pending, char key)
{
    (void)englishMode;
    (void)capsLock;
    if (!builtInSimplex || shiftOrChord || (key != '.' && key != ',')) {
        return SimplexPunctuationDecision::NotSpecial;
    }
    if (pending) {
        return SimplexPunctuationDecision::KeepPending;
    }
    return SimplexPunctuationDecision::EmitThroughWidthFilter;
}

// NSStringTransformFullwidthToHalfwidth reversed, measured 2026-11-03:
// "." -> U+FF0E, "," -> U+FF0C. Off leaves ASCII. This is the existing
// preference, not the Simplex candidate list.
inline uint32_t SimplexPunctuationAfterWidthPreference(char key, bool halfWidthToFullWidth)
{
    if (halfWidthToFullWidth) {
        if (key == '.') {
            return 0xFF0Eu;
        }
        if (key == ',') {
            return 0xFF0Cu;
        }
    }
    return (uint32_t)(unsigned char)key;
}

// Built-in Simplex Chinese ignores Caps Lock as an English bypass. The caller
// still passes Caps Lock through for every other input method, and for
// Simplex English mode, where the app applies normal letter case.
inline bool SimplexCapsLockBypassesChinese(bool builtInSimplexChinese)
{
    return !builtInSimplexChinese;
}

inline char NormalizeSimplexRadical(char keyCode)
{
    if (keyCode >= 'A' && keyCode <= 'Z') {
        return (char)(keyCode - 'A' + 'a');
    }
    return keyCode;
}

}

#endif
