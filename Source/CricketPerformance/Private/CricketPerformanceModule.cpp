#include "Modules/ModuleManager.h"

// CricketPerformance is a plain runtime module — the manager is a world subsystem
// that auto-creates, so there is no custom startup. Loaded at the Default phase,
// after every simulation module it observes.
IMPLEMENT_MODULE(FDefaultModuleImpl, CricketPerformance);
