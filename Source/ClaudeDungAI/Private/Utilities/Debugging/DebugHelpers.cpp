// Fill out your copyright notice in the Description page of Project Settings.

#include "Utilities/Debugging/DebugHelpers.h"
#include "DrawDebugHelpers.h"

// Sets default values for this component's properties
UDebugHelpers::UDebugHelpers()
{
	// This component doesn't need to tick every frame
	PrimaryComponentTick.bCanEverTick = false;
}

// Called when the game starts
void UDebugHelpers::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void UDebugHelpers::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

// ============================================================================
// LOGGING IMPLEMENTATION
// ============================================================================

bool UDebugHelpers::ShouldLog(EDebugLogLevel Level) const
{
	if (!bEnableDebug) return false;
	return static_cast<uint8>(Level) <= static_cast<uint8>(LogLevel);
}

FString UDebugHelpers::GetCategoryPrefix() const
{
	AActor* Owner = GetOwner();
	if (Owner)
	{
		return FString::Printf(TEXT("[%s]"), *Owner->GetName());
	}
	return TEXT("[DebugHelpers]");
}

void UDebugHelpers::Log(EDebugLogLevel Level, const FString& Message)
{
	if (!ShouldLog(Level)) return;

	FString FullMessage = FString::Printf(TEXT("%s %s"), *GetCategoryPrefix(), *Message);
	UE_LOG(LogTemp, Log, TEXT("%s"), *FullMessage);
}

void UDebugHelpers::LogCritical(const FString& Message)
{
	Log(EDebugLogLevel::Critical, Message);
}

void UDebugHelpers::LogImportant(const FString& Message)
{
	Log(EDebugLogLevel::Important, Message);
}

void UDebugHelpers::LogVerbose(const FString& Message)
{
	Log(EDebugLogLevel::Verbose, Message);
}

void UDebugHelpers::LogEverything(const FString& Message)
{
	Log(EDebugLogLevel::Everything, Message);
}

void UDebugHelpers::LogWithCategory(EDebugLogLevel Level, const FString& Category, const FString& Message)
{
	if (!ShouldLog(Level)) return;

	FString FullMessage = FString::Printf(TEXT("%s [%s] %s"), *GetCategoryPrefix(), *Category, *Message);
	UE_LOG(LogTemp, Log, TEXT("%s"), *FullMessage);
}

void UDebugHelpers::LogSectionHeader(const FString& Title)
{
	if (!ShouldLog(EDebugLogLevel::Important)) return;

	FString HeaderLine = TEXT("========================================");
	UE_LOG(LogTemp, Warning, TEXT("%s"), *HeaderLine);
	UE_LOG(LogTemp, Warning, TEXT("%s %s"), *GetCategoryPrefix(), *Title);
	UE_LOG(LogTemp, Warning, TEXT("%s"), *HeaderLine);
}

void UDebugHelpers::LogStatistic(const FString& Label, int32 Value)
{
	if (!ShouldLog(EDebugLogLevel::Important)) return;
	
	FString Message = FString::Printf(TEXT("%s: %d"), *Label, Value);
	Log(EDebugLogLevel::Important, Message);
}

void UDebugHelpers::LogStatistic(const FString& Label, float Value)
{
	if (!ShouldLog(EDebugLogLevel::Important)) return;
	
	FString Message = FString::Printf(TEXT("%s: %.2f"), *Label, Value);
	Log(EDebugLogLevel::Important, Message);
}

void UDebugHelpers::LogStatistic(const FString& Label, const FString& Value)
{
	if (!ShouldLog(EDebugLogLevel::Important)) return;
	
	FString Message = FString::Printf(TEXT("%s: %s"), *Label, *Value);
	Log(EDebugLogLevel::Important, Message);
}

// ============================================================================
// VISUALIZATION IMPLEMENTATION
// ============================================================================

void UDebugHelpers::DrawGrid(UWorld* World, const FVector& Origin, FIntPoint GridSize, float CellSize)
{
	if (!bEnableDebug || !bShowGrid || !World) return;

	const float Duration = DebugDrawDuration;

	// Draw vertical lines (X-axis)
	for (int32 X = 0; X <= GridSize.X; ++X)
	{
		FVector Start = Origin + FVector(X * CellSize, 0, 0);
		FVector End = Origin + FVector(X * CellSize, GridSize.Y * CellSize, 0);
		DrawDebugLine(World, Start, End, GridColor, false, Duration, 0, 5.0f);
	}

	// Draw horizontal lines (Y-axis)
	for (int32 Y = 0; Y <= GridSize.Y; ++Y)
	{
		FVector Start = Origin + FVector(0, Y * CellSize, 0);
		FVector End = Origin + FVector(GridSize.X * CellSize, Y * CellSize, 0);
		DrawDebugLine(World, Start, End, GridColor, false, Duration, 0, 5.0f);
	}
}

void UDebugHelpers::DrawCell(UWorld* World, const FVector& Origin, FIntPoint CellCoord, 
                              float CellSize, FColor Color, float Thickness)
{
	if (!bEnableDebug || !bShowCellStates || !World) return;

	FVector Center = Origin + FVector(
		(CellCoord.X + 0.5f) * CellSize,
		(CellCoord.Y + 0.5f) * CellSize,
		20.0f  // Lift slightly above floor
	);

	FVector Extent = FVector(CellSize * 0.4f, CellSize * 0.4f, 10.0f);

	DrawDebugBox(World, Center, Extent, FQuat::Identity, Color, false, 
	             DebugDrawDuration, 0, Thickness);
}

void UDebugHelpers::DrawCellRegion(UWorld* World, const FVector& Origin, FIntPoint StartCell, 
                                   FIntPoint Size, float CellSize, FColor Color, float Thickness)
{
	if (!bEnableDebug || !bShowForcedPlacements || !World) return;

	// Draw outer boundary box
	FVector Center = Origin + FVector(
		(StartCell.X + Size.X * 0.5f) * CellSize,
		(StartCell.Y + Size.Y * 0.5f) * CellSize,
		25.0f
	);

	FVector OuterExtent = FVector(
		Size.X * CellSize * 0.5f,
		Size.Y * CellSize * 0.5f,
		15.0f
	);

	DrawDebugBox(World, Center, OuterExtent, FQuat::Identity, Color, false, 
	             DebugDrawDuration, 0, Thickness);

	// Draw inner boundary box (slightly smaller for visual clarity)
	FVector InnerExtent = FVector(
		Size.X * CellSize * 0.48f,
		Size.Y * CellSize * 0.48f,
		12.0f
	);

	DrawDebugBox(World, Center, InnerExtent, FQuat::Identity, Color, false, 
	             DebugDrawDuration, 0, Thickness - 1.0f);
}

void UDebugHelpers::DrawTextAtLocation(UWorld* World, const FVector& Location, 
                                       const FString& Text, FColor Color, float Scale)
{
	if (!bEnableDebug || !World) return;

	DrawDebugString(World, Location, Text, nullptr, Color, DebugDrawDuration, false, Scale);
}

void UDebugHelpers::ClearDebugDrawings(UWorld* World)
{
	if (!World) return;
	
	FlushPersistentDebugLines(World);
	FlushDebugStrings(World);
}

// ============================================================================
// STATISTICS IMPLEMENTATION
// ============================================================================

void UDebugHelpers::ResetStatistics()
{
	TotalCellsGenerated = 0;
	FloorTilesPlaced = 0;
	WallSegmentsPlaced = 0;
	DoorsPlaced = 0;
	CeilingTilesPlaced = 0;
}

void UDebugHelpers::PrintStatistics()
{
	if (!bEnableDebug) return;

	LogSectionHeader(TEXT("GENERATION STATISTICS"));
	LogStatistic(TEXT("Total Cells Generated"), TotalCellsGenerated);
	LogStatistic(TEXT("Floor Tiles Placed"), FloorTilesPlaced);
	LogStatistic(TEXT("Wall Segments Placed"), WallSegmentsPlaced);
	LogStatistic(TEXT("Doors Placed"), DoorsPlaced);
	LogStatistic(TEXT("Ceiling Tiles Placed"), CeilingTilesPlaced);
}

// ============================================================================
// PERFORMANCE TRACKING
// ============================================================================

void UDebugHelpers::StartTimer(const FString& SectionName)
{
	if (!bEnableDebug) return;

	double CurrentTime = FPlatformTime::Seconds();
	ActiveTimers.Add(SectionName, CurrentTime);
	
	LogVerbose(FString::Printf(TEXT("Started timer: %s"), *SectionName));
}

void UDebugHelpers::EndTimer(const FString& SectionName)
{
	if (!bEnableDebug) return;

	if (ActiveTimers.Contains(SectionName))
	{
		double StartTime = ActiveTimers[SectionName];
		double EndTime = FPlatformTime::Seconds();
		double Duration = (EndTime - StartTime) * 1000.0; // Convert to milliseconds

		LogImportant(FString::Printf(TEXT("%s completed in %.2f ms"), *SectionName, Duration));
		
		ActiveTimers.Remove(SectionName);
	}
	else
	{
		LogVerbose(FString::Printf(TEXT("Timer '%s' was not started"), *SectionName));
	}
}