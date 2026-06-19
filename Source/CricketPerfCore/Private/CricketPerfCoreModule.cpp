#include "Modules/ModuleManager.h"

// CricketPerfCore is a plain runtime module — no custom startup. It loads at the
// PreDefault phase (alongside CricketPhysics) so the profiler singleton is available
// to every higher module before any gameplay ticks.
IMPLEMENT_MODULE(FDefaultModuleImpl, CricketPerfCore);
