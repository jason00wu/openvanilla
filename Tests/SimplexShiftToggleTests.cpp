#include "SimplexShiftToggle.h"

#include <stdio.h>

static int g_failed = 0;

#define EXPECT(cond)                                                         \
    do {                                                                     \
        if (!(cond)) {                                                       \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++g_failed;                                                      \
        }                                                                    \
    } while (0)

using OpenVanilla::SimplexKeyRoute;
using OpenVanilla::SimplexPunctuationDecision;
using OpenVanilla::SimplexShiftAction;
using OpenVanilla::SimplexShiftEvent;
using OpenVanilla::SimplexShiftResolution;
using OpenVanilla::SimplexShiftToggle;

static SimplexShiftEvent Ev(bool left, bool right, bool command, bool option, bool control, uint64_t timeNs)
{
    SimplexShiftEvent event;
    event.leftDown = left;
    event.rightDown = right;
    event.commandDown = command;
    event.optionDown = option;
    event.controlDown = control;
    event.timeNs = timeNs;
    return event;
}

static SimplexShiftAction Tap(SimplexShiftToggle &toggle, bool left, uint64_t start, uint64_t duration)
{
    SimplexShiftEvent down = left ? Ev(true, false, false, false, false, start)
                                   : Ev(false, true, false, false, false, start);
    EXPECT(toggle.flagsChanged(down) == SimplexShiftAction::None);
    SimplexShiftEvent up = Ev(false, false, false, false, false, start + duration);
    return toggle.flagsChanged(up);
}

static void AcceptIdleTap(SimplexShiftToggle &toggle, bool left, uint64_t start, uint64_t duration)
{
    bool before = toggle.englishMode();
    EXPECT(Tap(toggle, left, start, duration) == SimplexShiftAction::Toggle);
    EXPECT(toggle.englishMode() == before);
    EXPECT(OpenVanilla::ApplySimplexShiftTap(toggle, true, false) == SimplexShiftResolution::Switched);
}

static void TestBareTaps()
{
    SimplexShiftToggle toggle;
    EXPECT(toggle.englishMode() == false);
    AcceptIdleTap(toggle, true, 1000, 1000000);
    EXPECT(toggle.englishMode() == true);
    AcceptIdleTap(toggle, false, 9000000, 1000000);
    EXPECT(toggle.englishMode() == false);
    AcceptIdleTap(toggle, false, 20000000, 0);
    EXPECT(toggle.englishMode() == true);
    AcceptIdleTap(toggle, true, 30000000, OpenVanilla::kSimplexShiftMaxTapDurationNs);
    EXPECT(toggle.englishMode() == false);
}

static void TestLongPressAndKeyChord()
{
    SimplexShiftToggle toggle;
    EXPECT(Tap(toggle, true, 0, OpenVanilla::kSimplexShiftMaxTapDurationNs + 1) == SimplexShiftAction::None);
    EXPECT(toggle.englishMode() == false);

    EXPECT(toggle.flagsChanged(Ev(true, false, false, false, false, 100)) == SimplexShiftAction::None);
    toggle.noteKeyDown();
    EXPECT(toggle.flagsChanged(Ev(false, false, false, false, false, 200)) == SimplexShiftAction::None);
    EXPECT(toggle.englishMode() == false);

    AcceptIdleTap(toggle, false, 5000, 1000);
    EXPECT(toggle.englishMode() == true);
    toggle.noteKeyDown();
    EXPECT(toggle.englishMode() == true);
}

static void TestBothShiftsAndBlockingModifiers()
{
    SimplexShiftToggle both;
    EXPECT(both.flagsChanged(Ev(true, false, false, false, false, 1)) == SimplexShiftAction::None);
    EXPECT(both.flagsChanged(Ev(true, true, false, false, false, 2)) == SimplexShiftAction::None);
    EXPECT(both.flagsChanged(Ev(false, true, false, false, false, 3)) == SimplexShiftAction::None);
    EXPECT(both.flagsChanged(Ev(false, false, false, false, false, 4)) == SimplexShiftAction::None);
    EXPECT(both.englishMode() == false);

    SimplexShiftToggle swap;
    EXPECT(swap.flagsChanged(Ev(true, false, false, false, false, 1)) == SimplexShiftAction::None);
    EXPECT(swap.flagsChanged(Ev(false, true, false, false, false, 2)) == SimplexShiftAction::None);
    EXPECT(swap.flagsChanged(Ev(false, false, false, false, false, 3)) == SimplexShiftAction::None);
    EXPECT(swap.englishMode() == false);

    SimplexShiftToggle command;
    EXPECT(command.flagsChanged(Ev(true, false, true, false, false, 10)) == SimplexShiftAction::None);
    EXPECT(command.flagsChanged(Ev(false, false, false, false, false, 20)) == SimplexShiftAction::None);
    EXPECT(command.englishMode() == false);

    SimplexShiftToggle option;
    EXPECT(option.flagsChanged(Ev(false, true, false, false, false, 10)) == SimplexShiftAction::None);
    EXPECT(option.flagsChanged(Ev(false, true, false, true, false, 15)) == SimplexShiftAction::None);
    EXPECT(option.flagsChanged(Ev(false, false, false, false, false, 20)) == SimplexShiftAction::None);
    EXPECT(option.englishMode() == false);

    SimplexShiftToggle control;
    EXPECT(control.flagsChanged(Ev(true, false, false, false, false, 10)) == SimplexShiftAction::None);
    EXPECT(control.flagsChanged(Ev(true, false, false, false, true, 12)) == SimplexShiftAction::None);
    EXPECT(control.flagsChanged(Ev(true, false, false, false, false, 14)) == SimplexShiftAction::None);
    EXPECT(control.flagsChanged(Ev(false, false, false, false, false, 16)) == SimplexShiftAction::None);
    EXPECT(control.englishMode() == false);
}

static void TestCapsLockHoldAndClock()
{
    SimplexShiftToggle toggle;
    EXPECT(toggle.flagsChanged(Ev(true, false, false, false, false, 100)) == SimplexShiftAction::None);
    EXPECT(toggle.flagsChanged(Ev(true, false, false, false, false, 150)) == SimplexShiftAction::None);
    EXPECT(toggle.flagsChanged(Ev(false, false, false, false, false, 200)) == SimplexShiftAction::Toggle);
    EXPECT(OpenVanilla::ApplySimplexShiftTap(toggle, true, false) == SimplexShiftResolution::Switched);
    EXPECT(toggle.englishMode() == true);

    SimplexShiftToggle backwards;
    EXPECT(backwards.flagsChanged(Ev(true, false, false, false, false, 50)) == SimplexShiftAction::None);
    EXPECT(backwards.flagsChanged(Ev(false, false, false, false, false, 49)) == SimplexShiftAction::None);
    EXPECT(backwards.englishMode() == false);
    EXPECT(Tap(backwards, true, 80, 10) == SimplexShiftAction::Toggle);
}

static void TestFocusLossKeepsMode()
{
    SimplexShiftToggle toggle;
    AcceptIdleTap(toggle, true, 1, 10);
    EXPECT(toggle.englishMode() == true);
    EXPECT(toggle.flagsChanged(Ev(false, true, false, false, false, 100)) == SimplexShiftAction::None);
    toggle.cancelGesture();
    EXPECT(toggle.flagsChanged(Ev(false, false, false, false, false, 110)) == SimplexShiftAction::None);
    EXPECT(toggle.englishMode() == true);
    toggle.toggleEnglishMode();
    EXPECT(toggle.englishMode() == false);
    toggle.markModeTooltipShown();
    EXPECT(toggle.consumeModeTooltipShown() == true);
    EXPECT(toggle.consumeModeTooltipShown() == false);
}

static void TestTargetsAndRoutes()
{
    EXPECT(OpenVanilla::IsBuiltInSimplexTarget(OpenVanilla::kBuiltInSimplexIdentifier, false) == true);
    EXPECT(OpenVanilla::IsBuiltInSimplexTarget(OpenVanilla::kBuiltInSimplexIdentifier, true) == false);
    EXPECT(OpenVanilla::IsBuiltInSimplexTarget("org.openvanilla.OVIMTableBased.cj", false) == false);
    EXPECT(OpenVanilla::IsBuiltInSimplexTarget("org.openvanilla.OVIMTableBased.simplex-ext", false) == false);
    EXPECT(OpenVanilla::IsBuiltInSimplexTarget("", false) == false);
    EXPECT(OpenVanilla::IsBuiltInSimplexTarget(0, false) == false);

    EXPECT(OpenVanilla::RouteSimplexKeyDown(true, false) == SimplexKeyRoute::Standard);
    EXPECT(OpenVanilla::RouteSimplexKeyDown(true, true) == SimplexKeyRoute::EnglishPassThrough);
    EXPECT(OpenVanilla::RouteSimplexKeyDown(false, true) == SimplexKeyRoute::Standard);
}

static void TestPendingPreservedAndNotDeferred()
{
    SimplexShiftToggle chinese;
    EXPECT(Tap(chinese, true, 1000, 1000) == SimplexShiftAction::Toggle);
    EXPECT(chinese.englishMode() == false);
    EXPECT(OpenVanilla::ApplySimplexShiftTap(chinese, true, true) == SimplexShiftResolution::CancelPendingAndSwitch);
    EXPECT(chinese.englishMode() == true);
    EXPECT(chinese.flagsChanged(Ev(false, false, false, false, false, 2000)) == SimplexShiftAction::None);
    EXPECT(OpenVanilla::ApplySimplexShiftTap(chinese, false, false) == SimplexShiftResolution::NotATap);
    EXPECT(chinese.englishMode() == true);

    AcceptIdleTap(chinese, false, 5000, 1000);
    EXPECT(chinese.englishMode() == false);

    SimplexShiftToggle english;
    english.toggleEnglishMode();
    EXPECT(Tap(english, true, 1000, 1000) == SimplexShiftAction::Toggle);
    EXPECT(OpenVanilla::ApplySimplexShiftTap(english, true, true) == SimplexShiftResolution::PreservedPending);
    EXPECT(english.englishMode() == true);
    EXPECT(OpenVanilla::ApplySimplexShiftTap(english, true, false) == SimplexShiftResolution::Switched);
    EXPECT(english.englishMode() == false);
}

static void TestPunctuationAndCapsLock()
{
    EXPECT(OpenVanilla::DecideSimplexUnshiftedPunctuation(true, false, false, '.') == SimplexPunctuationDecision::DirectHalfwidth);
    EXPECT(OpenVanilla::DecideSimplexUnshiftedPunctuation(true, false, false, ',') == SimplexPunctuationDecision::DirectHalfwidth);
    EXPECT(OpenVanilla::DecideSimplexUnshiftedPunctuation(true, false, true, '.') == SimplexPunctuationDecision::KeepPending);
    EXPECT(OpenVanilla::DecideSimplexUnshiftedPunctuation(true, false, true, ',') == SimplexPunctuationDecision::KeepPending);
    EXPECT(OpenVanilla::DecideSimplexUnshiftedPunctuation(true, true, false, '.') == SimplexPunctuationDecision::NotSpecial);
    EXPECT(OpenVanilla::DecideSimplexUnshiftedPunctuation(true, true, true, ',') == SimplexPunctuationDecision::NotSpecial);
    EXPECT(OpenVanilla::DecideSimplexUnshiftedPunctuation(true, false, false, '>') == SimplexPunctuationDecision::NotSpecial);
    EXPECT(OpenVanilla::DecideSimplexUnshiftedPunctuation(false, false, false, '.') == SimplexPunctuationDecision::NotSpecial);

    EXPECT(OpenVanilla::SimplexCapsLockBypassesChinese(true) == false);
    EXPECT(OpenVanilla::SimplexCapsLockBypassesChinese(false) == true);
    EXPECT(OpenVanilla::NormalizeSimplexRadical('A') == 'a');
    EXPECT(OpenVanilla::NormalizeSimplexRadical('Z') == 'z');
    EXPECT(OpenVanilla::NormalizeSimplexRadical('m') == 'm');
    EXPECT(OpenVanilla::NormalizeSimplexRadical('.') == '.');
}

static void TestModifierBits()
{
    OpenVanilla::SimplexShiftSides left = OpenVanilla::SidesFromModifierBits(OpenVanilla::kDeviceLeftShiftBit, 0);
    EXPECT(left.leftDown == true && left.rightDown == false);
    OpenVanilla::SimplexShiftSides right = OpenVanilla::SidesFromModifierBits(OpenVanilla::kDeviceRightShiftBit, 0);
    EXPECT(right.leftDown == false && right.rightDown == true);
    OpenVanilla::SimplexShiftSides both = OpenVanilla::SidesFromModifierBits(
        OpenVanilla::kDeviceLeftShiftBit | OpenVanilla::kDeviceRightShiftBit, OpenVanilla::kLeftShiftKeyCode);
    EXPECT(both.leftDown == true && both.rightDown == true);

    uint32_t generic = OpenVanilla::kGenericShiftBit;
    OpenVanilla::SimplexShiftSides fallbackLeft = OpenVanilla::SidesFromModifierBits(generic, OpenVanilla::kLeftShiftKeyCode);
    EXPECT(fallbackLeft.leftDown == true && fallbackLeft.rightDown == false);
    OpenVanilla::SimplexShiftSides fallbackRight = OpenVanilla::SidesFromModifierBits(generic, OpenVanilla::kRightShiftKeyCode);
    EXPECT(fallbackRight.leftDown == false && fallbackRight.rightDown == true);
    OpenVanilla::SimplexShiftSides unknown = OpenVanilla::SidesFromModifierBits(generic, 0);
    EXPECT(unknown.leftDown == false && unknown.rightDown == false);

    uint32_t rightHeld = OpenVanilla::kGenericShiftBit | OpenVanilla::kDeviceRightShiftBit;
    OpenVanilla::SimplexShiftSides releasedLeft = OpenVanilla::SidesFromModifierBits(rightHeld, OpenVanilla::kLeftShiftKeyCode);
    EXPECT(releasedLeft.leftDown == false && releasedLeft.rightDown == true);

    OpenVanilla::SimplexShiftSides up = OpenVanilla::SidesFromModifierBits(0, OpenVanilla::kLeftShiftKeyCode);
    EXPECT(up.leftDown == false && up.rightDown == false);

    EXPECT(OpenVanilla::kLeftShiftKeyCode == 0x38);
    EXPECT(OpenVanilla::kRightShiftKeyCode == 0x3C);
    EXPECT(OpenVanilla::kGenericShiftBit == (1u << 17));
}

static void TestSharedSession()
{
    OpenVanilla::SimplexShiftToggle &first = OpenVanilla::SharedSimplexShiftToggle();
    OpenVanilla::SimplexShiftToggle &second = OpenVanilla::SharedSimplexShiftToggle();
    EXPECT(&first == &second);
    first.cancelGesture();
    if (first.englishMode()) {
        first.toggleEnglishMode();
    }
    EXPECT(first.englishMode() == false);
}

int main()
{
    TestBareTaps();
    TestLongPressAndKeyChord();
    TestBothShiftsAndBlockingModifiers();
    TestCapsLockHoldAndClock();
    TestFocusLossKeepsMode();
    TestTargetsAndRoutes();
    TestPendingPreservedAndNotDeferred();
    TestPunctuationAndCapsLock();
    TestModifierBits();
    TestSharedSession();
    if (g_failed) {
        fprintf(stderr, "FAILED %d\n", g_failed);
        return 1;
    }
    puts("PASS");
    return 0;
}
