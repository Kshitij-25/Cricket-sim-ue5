#include "CricketGameMode.h"

ACricketGameMode::ACricketGameMode()
{
	// MVP defaults; no default pawn yet — bowling/batting pawns arrive in their
	// roadmap phases. GameState/PlayerController wired up alongside them.
	PrimaryActorTick.bCanEverTick = false;
}
