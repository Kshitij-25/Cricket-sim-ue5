#include "Modules/ModuleManager.h"

// Standard runtime module. Physics has no startup side effects by design —
// all state lives in plain structs owned by callers, never in module globals.
IMPLEMENT_MODULE(FDefaultModuleImpl, CricketPhysics);
