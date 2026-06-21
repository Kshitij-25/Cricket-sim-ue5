// Headless automation tests for the gameplay-animation synchronization layer at
// the component level: the explicit batting contact window (CricketBattingComponent)
// derived from the same swing profile/timing the live game uses.
//
// Run: UnrealEditor-Cmd CricketSim.uproject \
//   -ExecCmds="Automation RunTests CricketSim.GameplayAnim; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "CricketBattingComponent.h"
#include "CricketSwingModel.h"

#if WITH_DEV_AUTOMATION_TESTS

// 1. CONTACT WINDOW: before any swing is triggered the window is closed; once a
//    swing starts, the window is centred on the profile's ideal contact instant
//    (DownswingTimeSec) and spans exactly +/- FCricketSwingModel::LooseWindowSec —
//    the same bound the timing-quality classifier already uses, now exposed as an
//    explicit, inspectable construct rather than an implicit side effect of geometry.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketGameplayContactWindowTest,
	"CricketSim.GameplayAnim.BattingContactWindow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketGameplayContactWindowTest::RunTest(const FString&)
{
	UCricketBattingComponent* Batting = NewObject<UCricketBattingComponent>();

	TestFalse(TEXT("No window before any swing is triggered"), Batting->IsContactWindowOpen());

	Batting->CurrentInput.ShotType = ECricketShotType::StraightDrive;
	Batting->CurrentInput.Footwork = ECricketFootwork::Neutral;
	Batting->CurrentInput.bRightHanded = true;
	Batting->TriggerSwing();

	const FCricketSwingProfile& P = Batting->GetActiveProfile();
	const double ExpectedOpen = FMath::Max(P.DownswingTimeSec - FCricketSwingModel::LooseWindowSec, 0.0);
	const double ExpectedClose = P.DownswingTimeSec + FCricketSwingModel::LooseWindowSec;

	TestTrue(TEXT("Window open bound matches Downswing - LooseWindow"),
		FMath::IsNearlyEqual(Batting->GetContactWindowOpenSec(), ExpectedOpen, 1e-9));
	TestTrue(TEXT("Window close bound matches Downswing + LooseWindow"),
		FMath::IsNearlyEqual(Batting->GetContactWindowCloseSec(), ExpectedClose, 1e-9));

	// Right at swing start (clock = 0) the window has not opened yet — the
	// downswing takes time, so the contact window is still ahead of the clock.
	TestFalse(TEXT("Window is not open at the very start of the downswing"), Batting->IsContactWindowOpen());

	// The window must be a real span, not a single instant or an inverted range.
	TestTrue(TEXT("Window close is after window open"), Batting->GetContactWindowCloseSec() > Batting->GetContactWindowOpenSec());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
