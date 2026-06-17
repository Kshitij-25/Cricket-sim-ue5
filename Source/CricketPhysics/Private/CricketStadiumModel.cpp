#include "CricketStadiumModel.h"

namespace
{
	// Horizontal (X,Y) part of a vector.
	FORCEINLINE FVector Flat(const FVector& V) { return FVector(V.X, V.Y, 0.0); }
}

double FCricketStadiumModel::BoundaryRadiusAtAngleM(const FCricketGroundDimensions& Dims, double AngleRad)
{
	// Ellipse: semi-axis a along the pitch axis (straight), b perpendicular (square).
	const double a = FMath::Max(Dims.StraightBoundaryM, 1.0);
	const double b = FMath::Max(Dims.SquareBoundaryM, 1.0);
	const double c = FMath::Cos(AngleRad), s = FMath::Sin(AngleRad);
	return 1.0 / FMath::Sqrt((c * c) / (a * a) + (s * s) / (b * b));
}

double FCricketStadiumModel::SignedDistanceInsideM(const FCricketGroundDimensions& Dims, const FVector& PointM)
{
	const FVector V = Flat(PointM - Dims.CenterM);
	const double Dist = V.Size();
	if (Dist < KINDA_SMALL_NUMBER) { return FMath::Min(Dims.StraightBoundaryM, Dims.SquareBoundaryM); }

	// Radius along this exact radial direction (decompose onto axis / side).
	const FVector Axis = Dims.PitchAxis.GetSafeNormal(KINDA_SMALL_NUMBER, FVector(1, 0, 0));
	const FVector Side = Dims.SideAxis();
	const FVector U = V / Dist;
	const double Along = FVector::DotProduct(U, Axis);
	const double Across = FVector::DotProduct(U, Side);
	const double a = FMath::Max(Dims.StraightBoundaryM, 1.0);
	const double b = FMath::Max(Dims.SquareBoundaryM, 1.0);
	const double R = 1.0 / FMath::Sqrt((Along * Along) / (a * a) + (Across * Across) / (b * b));
	return R - Dist;
}

FVector FCricketStadiumModel::BoundaryPointM(const FCricketGroundDimensions& Dims, const FVector& DirectionM)
{
	const FVector Dir = Flat(DirectionM).GetSafeNormal(KINDA_SMALL_NUMBER, Dims.PitchAxis);
	const FVector Axis = Dims.PitchAxis.GetSafeNormal(KINDA_SMALL_NUMBER, FVector(1, 0, 0));
	const double Angle = FMath::Atan2(FVector::DotProduct(Dir, Dims.SideAxis()), FVector::DotProduct(Dir, Axis));
	const double R = BoundaryRadiusAtAngleM(Dims, Angle);
	return Dims.CenterM + Dir * R + FVector(0, 0, Dims.RopeHeightM);
}

ECricketBoundaryResult FCricketStadiumModel::ClassifyBoundary(
	const FCricketGroundDimensions& Dims, const TArray<FVector>& PathM,
	int32 FirstBounceIdx, FVector& OutCrossingPointM)
{
	OutCrossingPointM = FVector::ZeroVector;
	if (PathM.Num() == 0) { return ECricketBoundaryResult::None; }

	for (int32 i = 0; i < PathM.Num(); ++i)
	{
		if (!IsInsideBoundary(Dims, PathM[i]))
		{
			OutCrossingPointM = PathM[i];
			// Did it bounce INSIDE before crossing here? If so it's a four; if it
			// reached the rope without an inside bounce it cleared it on the full.
			const bool bBouncedInside = (FirstBounceIdx != INDEX_NONE && FirstBounceIdx < i);
			return bBouncedInside ? ECricketBoundaryResult::Four : ECricketBoundaryResult::Six;
		}
	}
	return ECricketBoundaryResult::InPlay;
}

FCricketFieldPositionDef FCricketStadiumModel::DefaultPositionDef(ECricketFieldPosition Position)
{
	auto Def = [](double Angle, double Depth) { FCricketFieldPositionDef D; D.AngleDeg = Angle; D.DepthFrac = Depth; return D; };
	switch (Position)
	{
	case ECricketFieldPosition::WicketKeeper:  return Def(180.0, 0.04);
	case ECricketFieldPosition::Slip:          return Def(168.0, 0.07);
	case ECricketFieldPosition::Gully:         return Def(130.0, 0.20);
	case ECricketFieldPosition::Point:         return Def(95.0,  0.50);
	case ECricketFieldPosition::Cover:         return Def(55.0,  0.50);
	case ECricketFieldPosition::MidOff:        return Def(25.0,  0.50);
	case ECricketFieldPosition::MidOn:         return Def(-25.0, 0.50);
	case ECricketFieldPosition::Midwicket:     return Def(-58.0, 0.50);
	case ECricketFieldPosition::SquareLeg:     return Def(-95.0, 0.50);
	case ECricketFieldPosition::FineLeg:       return Def(-148.0, 0.90);
	case ECricketFieldPosition::ThirdMan:      return Def(150.0, 0.90);
	case ECricketFieldPosition::LongOff:       return Def(22.0,  0.92);
	case ECricketFieldPosition::LongOn:        return Def(-22.0, 0.92);
	case ECricketFieldPosition::DeepCover:     return Def(58.0,  0.90);
	case ECricketFieldPosition::DeepMidwicket: return Def(-58.0, 0.90);
	case ECricketFieldPosition::DeepPoint:     return Def(95.0,  0.90);
	case ECricketFieldPosition::DeepSquareLeg: return Def(-95.0, 0.90);
	default:                                   return Def(0.0, 0.5);
	}
}

FVector FCricketStadiumModel::FieldPositionWorldM(
	const FCricketGroundDimensions& Dims, ECricketFieldPosition Position, bool bRightHanded)
{
	const FCricketFieldPositionDef D = DefaultPositionDef(Position);
	double Theta = FMath::DegreesToRadians(D.AngleDeg);
	if (!bRightHanded) { Theta = -Theta; } // mirror off/leg for a left-hander

	const FVector Axis = Dims.PitchAxis.GetSafeNormal(KINDA_SMALL_NUMBER, FVector(1, 0, 0));
	const FVector Off = Dims.SideAxis();
	const FVector Dir = (Axis * FMath::Cos(Theta) + Off * FMath::Sin(Theta)).GetSafeNormal();

	// Depth scales with the boundary radius in that direction -> positions track ground size.
	const double Radius = BoundaryRadiusAtAngleM(Dims, Theta) * FMath::Clamp(D.DepthFrac, 0.0, 0.98);
	FVector Pos = Dims.StrikerStumpsM() + Dir * Radius;

	// Keep the fielder inside the rope. Positions are measured from the striker, who
	// is offset from the ground centre, so deep BEHIND positions (third man / fine
	// leg) can otherwise spill over the boundary; pull them a safe margin inside.
	const double MinMarginM = 3.0;
	const double Inside = SignedDistanceInsideM(Dims, Pos);
	if (Inside < MinMarginM)
	{
		const FVector V = FVector(Pos.X - Dims.CenterM.X, Pos.Y - Dims.CenterM.Y, 0.0);
		const double Dist = V.Size();
		if (Dist > KINDA_SMALL_NUMBER)
		{
			const FVector U = V / Dist;
			const double NewDist = Dist + Inside - MinMarginM; // = boundaryRadius - MinMargin
			Pos = FVector(Dims.CenterM.X, Dims.CenterM.Y, Pos.Z) + U * NewDist;
		}
	}
	return Pos;
}

FCricketFieldPlacement FCricketStadiumModel::DefaultField()
{
	FCricketFieldPlacement F;
	F.Name = TEXT("Balanced (default)");
	F.Positions = {
		ECricketFieldPosition::WicketKeeper, ECricketFieldPosition::Slip,
		ECricketFieldPosition::Point,        ECricketFieldPosition::Cover,
		ECricketFieldPosition::MidOff,       ECricketFieldPosition::MidOn,
		ECricketFieldPosition::Midwicket,    ECricketFieldPosition::SquareLeg,
		ECricketFieldPosition::FineLeg,      ECricketFieldPosition::ThirdMan,
		ECricketFieldPosition::LongOn
	};
	return F;
}
