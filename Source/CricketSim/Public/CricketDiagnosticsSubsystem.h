#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CricketScoringTypes.h"
#include "CricketDiagnosticsSubsystem.generated.h"

CRICKETSIM_API DECLARE_LOG_CATEGORY_EXTERN(LogCricketDiag, Log, All);

/**
 * UCricketDiagnosticsSubsystem — the release telemetry & diagnostics hub.
 *
 * Auto-creates with the GameInstance in every Game/PIE world (no level wiring),
 * in editor and packaged builds alike. Three responsibilities:
 *
 *   1. Crash reporting hooks — binds FCoreDelegates::OnHandleSystemError so a
 *      breadcrumb (build, config, last match context) is flushed to the log the
 *      instant the process faults, complementing UE's CrashReportClient.
 *   2. Error logging — RecordError() funnels gameplay-level faults through the
 *      LogCricketDiag category with a consistent, greppable format.
 *   3. Match analytics logging — RecordMatchStart/RecordMatchResult append a row
 *      to Saved/Analytics/Matches.csv for every completed match.
 *
 * Reactive and side-effect only: it never influences gameplay or physics.
 */
UCLASS()
class CRICKETSIM_API UCricketDiagnosticsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Convenience accessor from any world context. Null-safe. */
	static UCricketDiagnosticsSubsystem* Get(const UObject* WorldContext);

	/** Record the start of a match (sets the crash breadcrumb context). */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Diagnostics")
	void RecordMatchStart(const FString& HomeTeam, const FString& AwayTeam, int32 OversPerInnings);

	/** Record a completed match: appends an analytics row and logs the result. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Diagnostics")
	void RecordMatchResult(const FCricketMatchResult& Result, int32 HomeScore, int32 HomeWickets,
		int32 AwayScore, int32 AwayWickets);

	/** Structured, greppable error logging for gameplay-level faults. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Diagnostics")
	void RecordError(const FString& Context, const FString& Detail);

private:
	void InstallCrashHooks();
	void RemoveCrashHooks();
	static FString BuildBanner();
	FString AnalyticsCsvPath() const;

	/** Last-known context, embedded in the crash breadcrumb. */
	FString CurrentMatchContext = TEXT("(no match in progress)");
	int32 MatchesRecorded = 0;
	FDelegateHandle SystemErrorHandle;
};
