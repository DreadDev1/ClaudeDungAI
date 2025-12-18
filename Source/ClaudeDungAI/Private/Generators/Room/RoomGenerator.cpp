// Fill out your copyright notice in the Description page of Project Settings.

#include "Generators/Room/RoomGenerator.h"

// ============================================================================
// INITIALIZATION
// ============================================================================

bool URoomGenerator::Initialize(URoomData* InRoomData)
{
	if (!InRoomData)
	{
		UE_LOG(LogTemp, Error, TEXT("URoomGenerator::Initialize - InRoomData is null! "));
		return false;
	}

	RoomData = InRoomData;
	GridSize = RoomData->GridSize;
	CellSize = CELL_SIZE;
	bIsInitialized = true;

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator::Initialize - Initialized with GridSize (%d, %d), CellSize %.2f"), 
		GridSize.X, GridSize.Y, CellSize);

	return true;
}

// ============================================================================
// GRID MANAGEMENT
// ============================================================================

void URoomGenerator::CreateGrid()
{
	if (!bIsInitialized)
	{
		UE_LOG(LogTemp, Error, TEXT("URoomGenerator::CreateGrid - Generator not initialized!"));
		return;
	}

	// Calculate total cells needed
	int32 TotalCells = GridSize.X * GridSize.Y;

	// Initialize grid with all cells set to Empty
	GridState.Empty(TotalCells);
	GridState.AddUninitialized(TotalCells);

	for (int32 i = 0; i < TotalCells; ++i)
	{
		GridState[i] = EGridCellType::ECT_Empty;
	}

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator::CreateGrid - Created grid with %d cells"), TotalCells);
}

void URoomGenerator:: ClearGrid()
{
	if (!bIsInitialized)
	{
		UE_LOG(LogTemp, Error, TEXT("URoomGenerator::ClearGrid - Generator not initialized!"));
		return;
	}

	// Reset all cells to Empty
	for (EGridCellType& Cell : GridState)
	{
		Cell = EGridCellType::ECT_Empty;
	}

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator::ClearGrid - Grid cleared"));
}

EGridCellType URoomGenerator:: GetCellState(FIntPoint GridCoord) const
{
	if (!IsValidGridCoordinate(GridCoord))
	{
		return EGridCellType::ECT_Empty;
	}

	int32 Index = GridCoordToIndex(GridCoord);
	return GridState[Index];
}

bool URoomGenerator::SetCellState(FIntPoint GridCoord, EGridCellType NewState)
{
	if (!IsValidGridCoordinate(GridCoord))
	{
		return false;
	}

	int32 Index = GridCoordToIndex(GridCoord);
	GridState[Index] = NewState;
	return true;
}

bool URoomGenerator::IsValidGridCoordinate(FIntPoint GridCoord) const
{
	return GridCoord.X >= 0 && GridCoord.X < GridSize.X &&
	       GridCoord.Y >= 0 && GridCoord.Y < GridSize.Y;
}

bool URoomGenerator::IsAreaAvailable(FIntPoint StartCoord, FIntPoint Size) const
{
	// Check if entire area fits within grid
	if (StartCoord.X + Size.X > GridSize. X || StartCoord.Y + Size.Y > GridSize.Y)
	{
		return false;
	}

	// Check if any cell in the area is occupied
	for (int32 X = 0; X < Size.X; ++X)
	{
		for (int32 Y = 0; Y < Size.Y; ++Y)
		{
			FIntPoint CheckCoord(StartCoord.X + X, StartCoord.Y + Y);
			if (GetCellState(CheckCoord) != EGridCellType::ECT_Empty)
			{
				return false;
			}
		}
	}

	return true;
}

bool URoomGenerator::MarkArea(FIntPoint StartCoord, FIntPoint Size, EGridCellType CellType)
{
	// Validate that area is available
	if (!IsAreaAvailable(StartCoord, Size))
	{
		return false;
	}

	// Mark all cells in area
	for (int32 X = 0; X < Size.X; ++X)
	{
		for (int32 Y = 0; Y < Size.Y; ++Y)
		{
			FIntPoint CellCoord(StartCoord. X + X, StartCoord. Y + Y);
			SetCellState(CellCoord, CellType);
		}
	}

	return true;
}

bool URoomGenerator::ClearArea(FIntPoint StartCoord, FIntPoint Size)
{
	// Validate coordinates
	if (StartCoord.X + Size. X > GridSize.X || StartCoord.Y + Size.Y > GridSize.Y)
	{
		return false;
	}

	// Clear all cells in area
	for (int32 X = 0; X < Size.X; ++X)
	{
		for (int32 Y = 0; Y < Size.Y; ++Y)
		{
			FIntPoint CellCoord(StartCoord.X + X, StartCoord.Y + Y);
			SetCellState(CellCoord, EGridCellType::ECT_Empty);
		}
	}

	return true;
}

// ============================================================================
// COORDINATE CONVERSION
// ============================================================================

FVector URoomGenerator::GridToLocalPosition(FIntPoint GridCoord) const
{
	// Calculate center of cell
	float LocalX = GridCoord.X * CellSize + (CellSize * 0.5f);
	float LocalY = GridCoord.Y * CellSize + (CellSize * 0.5f);
	
	return FVector(LocalX, LocalY, 0.0f);
}

FIntPoint URoomGenerator::LocalToGridPosition(FVector LocalPos) const
{
	// Floor division to get grid coordinate
	int32 GridX = FMath::FloorToInt(LocalPos.X / CellSize);
	int32 GridY = FMath:: FloorToInt(LocalPos. Y / CellSize);
	
	return FIntPoint(GridX, GridY);
}

FIntPoint URoomGenerator::GetRotatedFootprint(FIntPoint OriginalFootprint, int32 Rotation)
{
	// Normalize rotation to 0-359 range
	Rotation = Rotation % 360;
	if (Rotation < 0) Rotation += 360;

	// For 90 and 270 degree rotations, swap X and Y
	if (Rotation == 90 || Rotation == 270)
	{
		return FIntPoint(OriginalFootprint.Y, OriginalFootprint.X);
	}

	// For 0 and 180 degree rotations, keep original
	return OriginalFootprint;
}

// ============================================================================
// STATISTICS
// ============================================================================

int32 URoomGenerator::GetCellCountByType(EGridCellType CellType) const
{
	int32 Count = 0;
	for (const EGridCellType& Cell : GridState)
	{
		if (Cell == CellType)
		{
			++Count;
		}
	}
	return Count;
}

float URoomGenerator::GetOccupancyPercentage() const
{
	int32 TotalCells = GetTotalCellCount();
	if (TotalCells == 0) return 0.0f;

	int32 OccupiedCells = GetCellCountByType(EGridCellType::ECT_FloorMesh);
	return (static_cast<float>(OccupiedCells) / static_cast<float>(TotalCells)) * 100.0f;
}

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

int32 URoomGenerator::GridCoordToIndex(FIntPoint GridCoord) const
{
	// Row-major order: Index = Y * Width + X
	return GridCoord.Y * GridSize.X + GridCoord.X;
}

FIntPoint URoomGenerator:: IndexToGridCoord(int32 Index) const
{
	// Reverse row-major order
	int32 X = Index % GridSize.X;
	int32 Y = Index / GridSize.X;
	return FIntPoint(X, Y);
}