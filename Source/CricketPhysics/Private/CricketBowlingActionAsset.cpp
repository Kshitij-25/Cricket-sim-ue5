#include "CricketBowlingActionAsset.h"

namespace
{
	FCricketDeliveryPreset MakePreset(const TCHAR* Name, ECricketMovement Movement, ECricketLength Length,
		ECricketLine Line, double Pace01, double SwingAmount, double SpinAmount)
	{
		FCricketDeliveryPreset P;
		P.DisplayName = Name;
		P.Movement = Movement;
		P.Length = Length;
		P.Line = Line;
		P.Pace01 = Pace01;
		P.SwingAmount = SwingAmount;
		P.SpinAmount = SpinAmount;
		return P;
	}
}

UCricketBowlingActionAsset::UCricketBowlingActionAsset()
{
	Action = MakeExpressQuick();
}

FCricketBowlingAction UCricketBowlingActionAsset::MakeExpressQuick()
{
	FCricketBowlingAction A;
	A.BowlerName = TEXT("Express Quick");
	A.Arm = ECricketBowlingArm::RightArm;
	A.PrimaryStyle = ECricketBowlingStyle::Pace;
	A.ReleaseHeightM = CricketField::DefaultPaceReleaseHeightM;
	A.ReleaseWidthM = 0.35;
	A.ReleaseToStumpsM = CricketField::DefaultReleaseToStumpsM;
	A.ArmSlotDeg = 16.0;
	A.MinPaceKmh = 132.0;
	A.MaxPaceKmh = 150.0;
	A.MaxSpinRPM = 600.0;     // a quick can scramble a few revs, not real spin
	A.StockBackspinRPM = 1400.0;
	A.HeldSeamStability = 0.96;
	A.Presets =
	{
		MakePreset(TEXT("Yorker"),      ECricketMovement::SeamUp, ECricketLength::Yorker,       ECricketLine::OffStump, 0.95, 0.0, 0.0),
		MakePreset(TEXT("Good Length"), ECricketMovement::SeamUp, ECricketLength::GoodLength,   ECricketLine::OffStump, 0.85, 0.0, 0.0),
		MakePreset(TEXT("Short Ball"),  ECricketMovement::SeamUp, ECricketLength::Short,        ECricketLine::OffStump, 0.90, 0.0, 0.0),
		MakePreset(TEXT("Bouncer"),     ECricketMovement::SeamUp, ECricketLength::Bouncer,      ECricketLine::Middle,   0.97, 0.0, 0.0),
	};
	return A;
}

FCricketBowlingAction UCricketBowlingActionAsset::MakeSwingBowler()
{
	FCricketBowlingAction A;
	A.BowlerName = TEXT("Swing Bowler");
	A.Arm = ECricketBowlingArm::RightArm;
	A.PrimaryStyle = ECricketBowlingStyle::Swing;
	A.ReleaseHeightM = 2.05;
	A.ReleaseWidthM = 0.32;
	A.ReleaseToStumpsM = CricketField::DefaultReleaseToStumpsM;
	A.ArmSlotDeg = 20.0;
	A.MinPaceKmh = 125.0;
	A.MaxPaceKmh = 142.0;
	A.MaxSpinRPM = 500.0;
	A.StockBackspinRPM = 1250.0;
	A.HeldSeamStability = 0.97;
	A.Presets =
	{
		MakePreset(TEXT("Outswinger"),    ECricketMovement::Outswing,     ECricketLength::GoodLength, ECricketLine::OutsideOff, 0.82, 0.75, 0.0),
		MakePreset(TEXT("Inswinger"),     ECricketMovement::Inswing,      ECricketLength::Full,       ECricketLine::Middle,     0.82, 0.75, 0.0),
		MakePreset(TEXT("Reverse Swing"), ECricketMovement::ReverseSwing, ECricketLength::Yorker,     ECricketLine::Middle,     0.98, 0.85, 0.0),
		MakePreset(TEXT("Wobble Seam"),   ECricketMovement::WobbleSeam,   ECricketLength::GoodLength, ECricketLine::OffStump,   0.86, 0.0,  0.0),
	};
	return A;
}

FCricketBowlingAction UCricketBowlingActionAsset::MakeOffSpinner()
{
	FCricketBowlingAction A;
	A.BowlerName = TEXT("Off Spinner");
	A.Arm = ECricketBowlingArm::RightArm;
	A.PrimaryStyle = ECricketBowlingStyle::OffSpin;
	A.ReleaseHeightM = CricketField::DefaultSpinReleaseHeightM;
	A.ReleaseWidthM = 0.30;
	A.ReleaseToStumpsM = CricketField::DefaultReleaseToStumpsM;
	A.ArmSlotDeg = 35.0;
	A.MinPaceKmh = 80.0;
	A.MaxPaceKmh = 95.0;
	A.MaxSpinRPM = 2400.0;
	A.StockBackspinRPM = 300.0;
	A.HeldSeamStability = 0.9;
	A.Presets =
	{
		MakePreset(TEXT("Off Break"),        ECricketMovement::OffBreak, ECricketLength::GoodLength, ECricketLine::OffStump,   0.45, 0.0, 0.80),
		MakePreset(TEXT("Off Break (Full)"), ECricketMovement::OffBreak, ECricketLength::Full,       ECricketLine::OffStump,   0.40, 0.0, 0.85),
		MakePreset(TEXT("Arm Ball"),         ECricketMovement::SeamUp,   ECricketLength::GoodLength, ECricketLine::OffStump,   0.60, 0.0, 0.0),
		MakePreset(TEXT("Off Break (Quick)"),ECricketMovement::OffBreak, ECricketLength::BackOfLength,ECricketLine::Middle,    0.85, 0.0, 0.70),
	};
	return A;
}

FCricketBowlingAction UCricketBowlingActionAsset::MakeLegSpinner()
{
	FCricketBowlingAction A;
	A.BowlerName = TEXT("Leg Spinner");
	A.Arm = ECricketBowlingArm::RightArm;
	A.PrimaryStyle = ECricketBowlingStyle::LegSpin;
	A.ReleaseHeightM = CricketField::DefaultSpinReleaseHeightM;
	A.ReleaseWidthM = 0.28;
	A.ReleaseToStumpsM = CricketField::DefaultReleaseToStumpsM;
	A.ArmSlotDeg = 38.0;
	A.MinPaceKmh = 78.0;
	A.MaxPaceKmh = 92.0;
	A.MaxSpinRPM = 2600.0;
	A.StockBackspinRPM = 300.0;
	A.HeldSeamStability = 0.9;
	A.Presets =
	{
		MakePreset(TEXT("Leg Break"),        ECricketMovement::LegBreak, ECricketLength::GoodLength, ECricketLine::OffStump, 0.45, 0.0, 0.85),
		MakePreset(TEXT("Leg Break (Full)"), ECricketMovement::LegBreak, ECricketLength::Full,       ECricketLine::Middle,   0.40, 0.0, 0.90),
		MakePreset(TEXT("Slider"),           ECricketMovement::SeamUp,   ECricketLength::GoodLength, ECricketLine::OffStump, 0.65, 0.0, 0.0),
		MakePreset(TEXT("Leg Break (Quick)"),ECricketMovement::LegBreak, ECricketLength::BackOfLength,ECricketLine::Middle,  0.85, 0.0, 0.80),
	};
	return A;
}
