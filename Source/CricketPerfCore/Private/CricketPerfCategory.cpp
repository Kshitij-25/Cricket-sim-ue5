#include "CricketPerfCategory.h"

const TCHAR* LexToString(ECricketPerfCategory Category)
{
	switch (Category)
	{
	case ECricketPerfCategory::Frame:        return TEXT("Frame");
	case ECricketPerfCategory::GameThread:   return TEXT("GameThread");
	case ECricketPerfCategory::RenderThread: return TEXT("RenderThread");
	case ECricketPerfCategory::GPU:          return TEXT("GPU");
	case ECricketPerfCategory::Physics:      return TEXT("Physics");
	case ECricketPerfCategory::Prediction:   return TEXT("Prediction");
	case ECricketPerfCategory::AI:           return TEXT("AI");
	case ECricketPerfCategory::Animation:    return TEXT("Animation");
	case ECricketPerfCategory::Replay:       return TEXT("Replay");
	case ECricketPerfCategory::Other:        return TEXT("Other");
	default:                                 return TEXT("?");
	}
}
