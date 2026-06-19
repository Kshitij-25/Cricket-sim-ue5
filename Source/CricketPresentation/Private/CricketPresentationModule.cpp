#include "Modules/ModuleManager.h"

// CricketPresentation is a plain runtime module — no custom startup. Loaded at the
// Default phase, at the top of the graph, after every gameplay/physics/match/audio
// system whose results it presents (and never modifies).
IMPLEMENT_MODULE(FDefaultModuleImpl, CricketPresentation);
