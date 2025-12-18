// Fill out your copyright notice in the Description page of Project Settings.

#include "Utilities/Debugging/DebugHelpers.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"

// Sets default values for this component's properties
UDebugHelpers::UDebugHelpers()
{
	// This component doesn't need to tick every frame
	PrimaryComponentTick.bCanEverTick = false;
}

// Called when the game starts
void UDebugHelpers:: BeginPlay()
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
	if (! bEnableDebug) return false;
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
	if (! ShouldLog(Level)) return;

	FString FullMessage = FString::Printf(TEXT("%s %s"), *GetCategoryPrefix(), *Message);
	UE_LOG(LogTemp, Log, TEXT("%s"), *FullMessage);
}

void UDebugHelpers::LogCritical(const FString& Message)
{
	Log(EDebugLogLevel::Critical, Message);
}

void UDebugHelpers::LogImportant(const FString& Message)
{
	Log(EDebugLogLevel:: Important, Message);
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
	
	FString Message = FString::Printf(TEXT("%s:  %.2f"), *Label, Value);
	Log(EDebugLogLevel::Important, Message);
}

void UDebugHelpers::LogStatistic(const FString& Label, const FString& Value)
{
	if (!ShouldLog(EDebugLogLevel::Important)) return;
	
	FString Message = FString::Printf(TEXT("%s: %s"), *Label, *Value);
	Log(EDebugLogLevel::Important, Message);
}

// ============================================================================
// GRID VISUALIZATION IMPLEMENTATION
// ============================================================================

void UDebugHelpers:: DrawGrid(FIntPoint GridSize, const TArray<EGridCellType>& CellStates, float CellSize, FVector OriginLocation)
{
	if (!bEnableDebug || !GetWorld()) return;

	// Draw grid outline
	if (bShowGrid)
	{
		DrawGridOutline(GridSize, CellSize, OriginLocation);
	}

	// Draw cell states
	if (bShowCellStates && CellStates.Num() > 0)
	{
		DrawCellStates(GridSize, CellStates, CellSize, OriginLocation);
	}

	// Draw coordinates
	if (bShowCoordinates)
	{
		DrawGridCoordinates(GridSize, CellSize, OriginLocation);
	}
}

void UDebugHelpers::DrawGridOutline(FIntPoint GridSize, float CellSize, FVector OriginLocation)
{
	if (!bEnableDebug || !GetWorld()) return;

	UWorld* World = GetWorld();
	
	// Calculate grid dimensions in world units
	float GridWidth = GridSize.X * CellSize;
	float GridHeight = GridSize.Y * CellSize;

	// Draw outer boundary (4 lines forming rectangle)
	// Bottom edge (South)
	DrawDebugLine(World, OriginLocation, 
		OriginLocation + FVector(GridWidth, 0, 0), 
		GridColor, false, DebugDrawDuration, 0, LineThickness);

	// Right edge (East)
	DrawDebugLine(World, OriginLocation + FVector(GridWidth, 0, 0), 
		OriginLocation + FVector(GridWidth, GridHeight, 0), 
		GridColor, false, DebugDrawDuration, 0, LineThickness);

	// Top edge (North)
	DrawDebugLine(World, OriginLocation + FVector(GridWidth, GridHeight, 0), 
		OriginLocation + FVector(0, GridHeight, 0), 
		GridColor, false, DebugDrawDuration, 0, LineThickness);

	// Left edge (West)
	DrawDebugLine(World, OriginLocation + FVector(0, GridHeight, 0), 
		OriginLocation, 
		GridColor, false, DebugDrawDuration, 0, LineThickness);

	// Draw internal grid lines
	// Vertical lines (along X-axis)
	for (int32 X = 1; X < GridSize.X; ++X)
	{
		FVector StartPos = OriginLocation + FVector(X * CellSize, 0, 0);
		FVector EndPos = OriginLocation + FVector(X * CellSize, GridHeight, 0);
		DrawDebugLine(World, StartPos, EndPos, GridColor, false, DebugDrawDuration, 0, LineThickness * 0.5f);
	}

	// Horizontal lines (along Y-axis)
	for (int32 Y = 1; Y < GridSize.Y; ++Y)
	{
		FVector StartPos = OriginLocation + FVector(0, Y * CellSize, 0);
		FVector EndPos = OriginLocation + FVector(GridWidth, Y * CellSize, 0);
		DrawDebugLine(World, StartPos, EndPos, GridColor, false, DebugDrawDuration, 0, LineThickness * 0.5f);
	}
}

void UDebugHelpers:: DrawGridCoordinates(FIntPoint GridSize, float CellSize, FVector OriginLocation)
{
	if (!bEnableDebug || !GetWorld()) return;

	UWorld* World = GetWorld();

	// Draw coordinate label at center of each cell
	for (int32 X = 0; X < GridSize.X; ++X)
	{
		for (int32 Y = 0; Y < GridSize.Y; ++Y)
		{
			FIntPoint GridCoord(X, Y);
			FVector CellCenter = GridToWorldPosition(GridCoord, CellSize, OriginLocation);
			
			// Offset text slightly above the grid
			CellCenter. Z += CoordinateTextHeight;

			// Format coordinate string
			FString CoordText = FString::Printf(TEXT("(%d,%d)"), X, Y);

			// Draw the text
			DrawDebugString(World, CellCenter, CoordText, nullptr, CoordinateTextColor, 
				DebugDrawDuration, false, CoordinateTextScale);
		}
	}
}

void UDebugHelpers::DrawCell(FIntPoint GridCoord, FColor Color, float CellSize, FVector OriginLocation)
{
	if (!bEnableDebug || !GetWorld()) return;

	UWorld* World = GetWorld();
	
	// Calculate cell center in world space
	FVector CellCenter = GridToWorldPosition(GridCoord, CellSize, OriginLocation);
	
	// Calculate box extent (half of cell size)
	FVector BoxExtent(CellSize * 0.5f, CellSize * 0.5f, 5.0f);

	// Draw box at cell location
	DrawDebugBox(World, CellCenter, BoxExtent, Color, false, DebugDrawDuration, 0, LineThickness);
}

void UDebugHelpers::DrawCellStates(FIntPoint GridSize, const TArray<EGridCellType>& CellStates, float CellSize, FVector OriginLocation)
{
	if (!bEnableDebug || !GetWorld()) return;

	// Validate array size
	int32 ExpectedSize = GridSize.X * GridSize.Y;
	if (CellStates.Num() != ExpectedSize)
	{
		LogCritical(FString::Printf(TEXT("DrawCellStates: CellStates array size mismatch!  Expected %d, got %d"), 
			ExpectedSize, CellStates.Num()));
		return;
	}

	// Draw each cell with its state color
	for (int32 X = 0; X < GridSize.X; ++X)
	{
		for (int32 Y = 0; Y < GridSize.Y; ++Y)
		{
			int32 Index = Y * GridSize.X + X;
			EGridCellType CellType = CellStates[Index];
			FColor CellColor = GetColorForCellType(CellType);
			
			DrawCell(FIntPoint(X, Y), CellColor, CellSize, OriginLocation);
		}
	}
}

void UDebugHelpers::ClearDebugDrawings()
{
	if (!GetWorld()) return;
	
	FlushPersistentDebugLines(GetWorld());
	LogImportant(TEXT("Debug drawings cleared"));
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

FColor UDebugHelpers:: GetColorForCellType(EGridCellType CellType) const
{
	switch (CellType)
	{
		case EGridCellType::ECT_Empty: 
			return EmptyCellColor;
		case EGridCellType::ECT_FloorMesh:
			return OccupiedCellColor;
		case EGridCellType:: ECT_Wall:
			return WallCellColor;
		case EGridCellType::ECT_Doorway:
			return DoorCellColor;
		default:
			return FColor::White;
	}
}

FVector UDebugHelpers::GridToWorldPosition(FIntPoint GridCoord, float CellSize, FVector OriginLocation) const
{
	// Calculate center of cell
	float CenterOffsetX = GridCoord.X * CellSize + (CellSize * 0.5f);
	float CenterOffsetY = GridCoord.Y * CellSize + (CellSize * 0.5f);
	
	return OriginLocation + FVector(CenterOffsetX, CenterOffsetY, 0.0f);
}