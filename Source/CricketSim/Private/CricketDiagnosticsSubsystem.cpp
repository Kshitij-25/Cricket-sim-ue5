#include "CricketDiagnosticsSubsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "Misc/EngineVersion.h"

DEFINE_LOG_CATEGORY(LogCricketDiag);

void UCricketDiagnosticsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogCricketDiag, Log, TEXT("%s"), *BuildBanner());
	InstallCrashHooks();
}

void UCricketDiagnosticsSubsystem::Deinitialize()
{
	RemoveCrashHooks();
	UE_LOG(LogCricketDiag, Log, TEXT("Diagnostics shutting down. Matches recorded this session: %d"), MatchesRecorded);
	Super::Deinitialize();
}

UCricketDiagnosticsSubsystem* UCricketDiagnosticsSubsystem::Get(const UObject* WorldContext)
{
	if (!WorldContext) { return nullptr; }
	if (const UWorld* World = WorldContext->GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			return GI->GetSubsystem<UCricketDiagnosticsSubsystem>();
		}
	}
	return nullptr;
}

FString UCricketDiagnosticsSubsystem::BuildBanner()
{
	const TCHAR* Config =
#if UE_BUILD_SHIPPING
		TEXT("Shipping");
#elif UE_BUILD_TEST
		TEXT("Test");
#elif UE_BUILD_DEBUG
		TEXT("Debug");
#else
		TEXT("Development");
#endif
	const FString Platform = FString(ANSI_TO_TCHAR(FPlatformProperties::PlatformName()));
	return FString::Printf(
		TEXT("=== CricketSim Diagnostics === build=%s config=%s platform=%s engine=%s"),
		FApp::GetBuildVersion(), Config, *Platform, *FEngineVersion::Current().ToString());
}

void UCricketDiagnosticsSubsystem::InstallCrashHooks()
{
	// Breadcrumb flushed to the log at the instant of an unhandled fault, so the
	// last match context survives even when the crash dump is opaque. Complements
	// (does not replace) UE's CrashReportClient.
	SystemErrorHandle = FCoreDelegates::OnHandleSystemError.AddWeakLambda(this, [this]()
	{
		UE_LOG(LogCricketDiag, Error,
			TEXT("FATAL: unhandled system error. %s | context: %s"),
			*BuildBanner(), *CurrentMatchContext);
		if (GLog) { GLog->Flush(); }
	});
}

void UCricketDiagnosticsSubsystem::RemoveCrashHooks()
{
	if (SystemErrorHandle.IsValid())
	{
		FCoreDelegates::OnHandleSystemError.Remove(SystemErrorHandle);
		SystemErrorHandle.Reset();
	}
}

void UCricketDiagnosticsSubsystem::RecordError(const FString& Context, const FString& Detail)
{
	UE_LOG(LogCricketDiag, Error, TEXT("[%s] %s"), *Context, *Detail);
}

void UCricketDiagnosticsSubsystem::RecordMatchStart(const FString& HomeTeam, const FString& AwayTeam, int32 OversPerInnings)
{
	CurrentMatchContext = FString::Printf(TEXT("%s vs %s, T%d"), *HomeTeam, *AwayTeam, OversPerInnings);
	UE_LOG(LogCricketDiag, Log, TEXT("Match start: %s"), *CurrentMatchContext);
}

FString UCricketDiagnosticsSubsystem::AnalyticsCsvPath() const
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Analytics"), TEXT("Matches.csv"));
}

void UCricketDiagnosticsSubsystem::RecordMatchResult(const FCricketMatchResult& Result, int32 HomeScore, int32 HomeWickets,
	int32 AwayScore, int32 AwayWickets)
{
	++MatchesRecorded;
	UE_LOG(LogCricketDiag, Log, TEXT("Match result: %s [%s] (%d/%d vs %d/%d)"),
		*Result.Summary, *CurrentMatchContext, HomeScore, HomeWickets, AwayScore, AwayWickets);

	const FString Path = AnalyticsCsvPath();
	const bool bExists = IFileManager::Get().FileExists(*Path);

	FString Row;
	if (!bExists)
	{
		Row += TEXT("Timestamp,Context,Decided,Tie,Winner,HomeScore,HomeWickets,AwayScore,AwayWickets,Summary\n");
	}
	const FString Safe = Result.Summary.Replace(TEXT(","), TEXT(";"));
	Row += FString::Printf(TEXT("%s,%s,%d,%d,%s,%d,%d,%d,%d,%s\n"),
		*FDateTime::UtcNow().ToIso8601(), *CurrentMatchContext.Replace(TEXT(","), TEXT(";")),
		Result.bDecided ? 1 : 0, Result.bTie ? 1 : 0, *Result.WinningTeam.Replace(TEXT(","), TEXT(";")),
		HomeScore, HomeWickets, AwayScore, AwayWickets, *Safe);

	if (!FFileHelper::SaveStringToFile(Row, *Path,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(),
		bExists ? EFileWrite::FILEWRITE_Append : EFileWrite::FILEWRITE_None))
	{
		UE_LOG(LogCricketDiag, Warning, TEXT("Failed to write match analytics row to %s"), *Path);
	}
}
