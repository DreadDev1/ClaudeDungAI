// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DebugHelpers.generated.h"

// Log verbosity levels
UENUM(BlueprintType)
enum class EDebugLogLevel : uint8
{
	None = 0       UMETA(DisplayName = "None (No Logging)"),
	Critical = 1   UMETA(DisplayName = "Critical Only"),
	Important = 2  UMETA(DisplayName = "Important Messages"),
	Verbose = 3    UMETA(DisplayName = "Verbose"),
	Everything = 4 UMETA(DisplayName = "Everything")
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CLAUDEDUNGAI_API UDebugHelpers : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UDebugHelpers();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	// Master toggle for all debug features
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings")
	bool bEnableDebug = true;

	// Log level for console output
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings|Logging")
	EDebugLogLevel LogLevel = EDebugLogLevel::Important;

	// Enable grid visualization
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings|Visualization")
	bool bShowGrid = true;

	// Enable cell state visualization (colored boxes)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings|Visualization")
	bool bShowCellStates = true;

	// Enable forced placement visualization
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings|Visualization")
	bool bShowForcedPlacements = true;

	// Duration for debug drawings (in seconds, -1 for permanent)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings|Visualization")
	float DebugDrawDuration = 5.0f;

	// ============================================================================
	// COLOR SCHEME
	// ============================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings|Colors")
	FColor GridColor = FColor::Green;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings|Colors")
	FColor EmptyCellColor = FColor::Blue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings|Colors")
	FColor OccupiedCellColor = FColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings|Colors")
	FColor WallCellColor = FColor::Orange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings|Colors")
	FColor DoorCellColor = FColor::Cyan;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings|Colors")
	FColor ForcedPlacementColor = FColor::Magenta;

	// ============================================================================
	// LOGGING API
	// ============================================================================

	// Log a message at specified level
	void Log(EDebugLogLevel Level, const FString& Message);

	// Convenience functions for different log levels
	void LogCritical(const FString& Message);
	void LogImportant(const FString& Message);
	void LogVerbose(const FString& Message);
	void LogEverything(const FString& Message);

	// Formatted logging with category
	void LogWithCategory(EDebugLogLevel Level, const FString& Category, const FString& Message);

	// Log a section header (for visual separation)
	void LogSectionHeader(const FString& Title);

	// Log a statistic (key-value pair)
	void LogStatistic(const FString& Label, int32 Value);
	void LogStatistic(const FString& Label, float Value);
	void LogStatistic(const FString& Label, const FString& Value);

	// ============================================================================
	// VISUALIZATION API
	// ============================================================================

	// Draw grid lines
	void DrawGrid(UWorld* World, const FVector& Origin, FIntPoint GridSize, float CellSize);

	// Draw a single cell box
	void DrawCell(UWorld* World, const FVector& Origin, FIntPoint CellCoord, float CellSize, 
	              FColor Color, float Thickness = 3.0f);

	// Draw a region of cells (for forced placements, empty regions, etc.)
	void DrawCellRegion(UWorld* World, const FVector& Origin, FIntPoint StartCell, 
	                    FIntPoint Size, float CellSize, FColor Color, float Thickness = 3.0f);

	// Draw text at world location (for cell coordinates, labels, etc.)
	void DrawTextAtLocation(UWorld* World, const FVector& Location, const FString& Text, 
	                        FColor Color = FColor::White, float Scale = 1.0f);

	// Clear all persistent debug drawings
	void ClearDebugDrawings(UWorld* World);

	// ============================================================================
	// STATISTICS TRACKING
	// ============================================================================

	// Track generation statistics
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug Info|Statistics")
	int32 TotalCellsGenerated = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug Info|Statistics")
	int32 FloorTilesPlaced = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug Info|Statistics")
	int32 WallSegmentsPlaced = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug Info|Statistics")
	int32 DoorsPlaced = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug Info|Statistics")
	int32 CeilingTilesPlaced = 0;

	// Reset statistics
	UFUNCTION(BlueprintCallable, Category = "Debug")
	void ResetStatistics();

	// Print current statistics to log
	UFUNCTION(BlueprintCallable, Category = "Debug")
	void PrintStatistics();

	// ============================================================================
	// PERFORMANCE TRACKING
	// ============================================================================

	// Start timing a section
	void StartTimer(const FString& SectionName);

	// End timing a section and log duration
	void EndTimer(const FString& SectionName);

private:
	// Timer storage
	TMap<FString, double> ActiveTimers;
	
	// Check if logging is enabled for specified level
	bool ShouldLog(EDebugLogLevel Level) const;
	
	// Get category prefix for log messages
	FString GetCategoryPrefix() const;
};
