// Fill out your copyright notice in the Description page of Project Settings.

#include "Generators/Room/RoomGenerator.h"
#include "Data/Grid/GridData.h"

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

	// Initialize statistics
	LargeTilesPlaced = 0;
	MediumTilesPlaced = 0;
	SmallTilesPlaced = 0;
	FillerTilesPlaced = 0;

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

	// Clear placed meshes
	ClearPlacedFloorMeshes();
	
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
// FLOOR GENERATION
// ============================================================================

bool URoomGenerator::GenerateFloor()
{
	if (!bIsInitialized)
	{
		UE_LOG(LogTemp, Error, TEXT("URoomGenerator::GenerateFloor - Generator not initialized!"));
		return false;
	}

	if (! RoomData || !RoomData->FloorStyleData)
	{
		UE_LOG(LogTemp, Error, TEXT("URoomGenerator:: GenerateFloor - FloorData not assigned!"));
		return false;
	}

	// Load FloorData and keep strong reference throughout function
	UFloorData* FloorStyleData = RoomData->FloorStyleData.LoadSynchronous();
	if (!FloorStyleData)
	{
		UE_LOG(LogTemp, Error, TEXT("URoomGenerator::GenerateFloor - Failed to load FloorStyleData!"));
		return false;
	}

	// Validate FloorTilePool exists
	if (FloorStyleData->FloorTilePool. Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("URoomGenerator::GenerateFloor - No floor meshes defined in FloorTilePool!"));
		return false;
	}
	
	// Clear previous placement data
	ClearPlacedFloorMeshes();

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator::GenerateFloor - Starting floor generation"));

	// ========================================================================
	// PHASE 0:  FORCED EMPTY REGIONS (Mark cells as reserved)
	// ========================================================================
	TArray<FIntPoint> ForcedEmptyCells = ExpandForcedEmptyRegions();
	if (ForcedEmptyCells.Num() > 0)
	{
		MarkForcedEmptyCells(ForcedEmptyCells);
		UE_LOG(LogTemp, Log, TEXT("  Phase 0: Marked %d forced empty cells"), ForcedEmptyCells. Num());
	}

	// ========================================================================
	// PHASE 1: FORCED PLACEMENTS (Designer overrides - highest priority)
	// ========================================================================
	int32 ForcedCount = ExecuteForcedPlacements();
	UE_LOG(LogTemp, Log, TEXT("  Phase 1: Placed %d forced meshes"), ForcedCount);
	
	// ========================================================================
	// PHASE 2: GREEDY FILL (Large → Medium → Small)
	// ========================================================================
	// Use the FloorData pointer we loaded at the top (safer than re-accessing)
	const TArray<FMeshPlacementInfo>& FloorMeshes = FloorStyleData->FloorTilePool;

	UE_LOG(LogTemp, Log, TEXT("  Phase 2: Greedy fill with %d tile options"), FloorMeshes.Num());

	// Large tiles (400x400, 200x400, 400x200)
	FillWithTileSize(FloorMeshes, FIntPoint(4, 4)); // 400x400
	FillWithTileSize(FloorMeshes, FIntPoint(2, 4)); // 200x400
	FillWithTileSize(FloorMeshes, FIntPoint(4, 2)); // 400x200

	// Medium tiles (200x200)
	FillWithTileSize(FloorMeshes, FIntPoint(2, 2)); // 200x200

	// Small tiles (100x200, 200x100, 100x100)
	FillWithTileSize(FloorMeshes, FIntPoint(1, 2)); // 100x200
	FillWithTileSize(FloorMeshes, FIntPoint(2, 1)); // 200x100
	FillWithTileSize(FloorMeshes, FIntPoint(1, 1)); // 100x100

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator::GenerateFloor - Floor generation complete.  Placed %d total meshes"), 
		PlacedFloorMeshes.Num());

	return true;
}

void URoomGenerator::FillWithTileSize(const TArray<FMeshPlacementInfo>& TilePool, FIntPoint TargetSize)
{
	// Filter tiles that match target size
	TArray<FMeshPlacementInfo> MatchingTiles;

	for (const FMeshPlacementInfo& MeshInfo : TilePool)
	{
		FIntPoint Footprint = CalculateFootprint(MeshInfo);

		// Check if footprint matches target size (or rotated version)
		if ((Footprint.X == TargetSize.X && Footprint.Y == TargetSize. Y) ||
			(Footprint.X == TargetSize.Y && Footprint.Y == TargetSize.X))
		{
			MatchingTiles.Add(MeshInfo);
		}
	}

	if (MatchingTiles. Num() == 0)
	{
		return; // No tiles of this size
	}

	UE_LOG(LogTemp, Verbose, TEXT("URoomGenerator:: FillWithTileSize - Filling with %dx%d tiles (%d options)"), 
		TargetSize.X, TargetSize.Y, MatchingTiles.Num());

	// Try to place tiles of this size across the grid
	for (int32 Y = 0; Y < GridSize.Y; ++Y)
	{
		for (int32 X = 0; X < GridSize.X; ++X)
		{
			FIntPoint StartCoord(X, Y);

			// Check if area is available
			if (IsAreaAvailable(StartCoord, TargetSize))
			{
				// Select weighted random mesh
				FMeshPlacementInfo SelectedMesh = SelectWeightedMesh(MatchingTiles);

				// Try to place mesh
				if (TryPlaceMesh(StartCoord, TargetSize, SelectedMesh, 0))
				{
					// Update statistics
					int32 TileArea = TargetSize.X * TargetSize.Y;
					if (TileArea >= 16) LargeTilesPlaced++;
					else if (TileArea >= 4) MediumTilesPlaced++;
					else if (TileArea >= 2) SmallTilesPlaced++;
					else FillerTilesPlaced++;
				}
			}
		}
	}
}

FMeshPlacementInfo URoomGenerator::SelectWeightedMesh(const TArray<FMeshPlacementInfo>& Pool)
{
	if (Pool.Num() == 0)
	{
		return FMeshPlacementInfo(); // Return empty if no options
	}

	if (Pool.Num() == 1)
	{
		return Pool[0]; // Only one option
	}

	// Calculate total weight
	float TotalWeight = 0.0f;
	for (const FMeshPlacementInfo& MeshInfo : Pool)
	{
		TotalWeight += MeshInfo.PlacementWeight;
	}

	// Random value between 0 and total weight
	float RandomValue = FMath::FRandRange(0.0f, TotalWeight);

	// Select mesh based on weight
	float CurrentWeight = 0.0f;
	for (const FMeshPlacementInfo& MeshInfo : Pool)
	{
		CurrentWeight += MeshInfo.PlacementWeight;
		if (RandomValue <= CurrentWeight)
		{
			return MeshInfo;
		}
	}

	// Fallback to last mesh
	return Pool. Last();
}

bool URoomGenerator::TryPlaceMesh(FIntPoint StartCoord, FIntPoint Size, const FMeshPlacementInfo& MeshInfo, int32 Rotation)
{
	// Check if area is available
	if (! IsAreaAvailable(StartCoord, Size))
	{
		return false;
	}

	// Mark area as occupied
	if (!MarkArea(StartCoord, Size, EGridCellType::ECT_FloorMesh))
	{
		return false;
	}

	// Create placed mesh info
	FPlacedMeshInfo PlacedMesh;
	PlacedMesh.GridPosition = StartCoord;
	PlacedMesh.Size = Size;
	PlacedMesh.Rotation = Rotation;
	PlacedMesh.MeshInfo = MeshInfo;

	// FIXED: Calculate center of entire mesh footprint, not just first cell
	// Formula: TopLeftCorner + (MeshSize * CellSize * 0.5)
	// Example: 2x2 mesh at (0,0) = (0,0) + (200*0.5, 200*0.5) = (100, 100)
	float OffsetX = (Size.X * CellSize) * 0.5f;
	float OffsetY = (Size.Y * CellSize) * 0.5f;
	
	FVector LocalPos = FVector(
		StartCoord. X * CellSize + OffsetX,
		StartCoord.Y * CellSize + OffsetY,
		0.0f
	);

	PlacedMesh.WorldTransform = FTransform(
		FRotator(0, Rotation, 0),
		LocalPos,
		FVector::OneVector
	);

	// Store placed mesh
	PlacedFloorMeshes.Add(PlacedMesh);

	return true;
}

FIntPoint URoomGenerator::CalculateFootprint(const FMeshPlacementInfo& MeshInfo) const
{
	// If footprint is explicitly defined, use it
	if (MeshInfo.GridFootprint.X > 0 && MeshInfo. GridFootprint.Y > 0)
	{
		return MeshInfo.GridFootprint;
	}

	// Otherwise, calculate from mesh bounds
	if (! MeshInfo.MeshAsset. IsNull())
	{
		// For now, return default 1x1 if bounds not available
		// TODO: Load mesh and calculate actual bounds
		return FIntPoint(1, 1);
	}

	// Fallback
	return FIntPoint(1, 1);
}

void URoomGenerator::ClearPlacedFloorMeshes()
{
	PlacedFloorMeshes. Empty();
	LargeTilesPlaced = 0;
	MediumTilesPlaced = 0;
	SmallTilesPlaced = 0;
	FillerTilesPlaced = 0;
}

void URoomGenerator::GetFloorStatistics(int32& OutLargeTiles, int32& OutMediumTiles, int32& OutSmallTiles, int32& OutFillerTiles) const
{
	OutLargeTiles = LargeTilesPlaced;
	OutMediumTiles = MediumTilesPlaced;
	OutSmallTiles = SmallTilesPlaced;
	OutFillerTiles = FillerTilesPlaced;
}

int32 URoomGenerator::ExecuteForcedPlacements()
{
	if (!bIsInitialized || !RoomData)
	{
		UE_LOG(LogTemp, Error, TEXT("URoomGenerator::ExecuteForcedPlacements - Not initialized! "));
		return 0;
	}

	int32 SuccessfulPlacements = 0;
	const TMap<FIntPoint, FMeshPlacementInfo>& ForcedPlacements = RoomData->ForcedInteriorPlacements;

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator::ExecuteForcedPlacements - Processing %d forced placements"), ForcedPlacements. Num());

	for (const auto& Pair : ForcedPlacements)
	{
		const FIntPoint StartCoord = Pair.Key;
		const FMeshPlacementInfo& MeshInfo = Pair.Value;

		// Validate mesh asset
		if (MeshInfo.MeshAsset.IsNull())
		{
			UE_LOG(LogTemp, Warning, TEXT("  Forced placement at (%d,%d) has null mesh asset - skipping"), 
				StartCoord.X, StartCoord.Y);
			continue;
		}

		// Calculate footprint
		FIntPoint Footprint = CalculateFootprint(MeshInfo);

		// Select rotation (use first allowed rotation for forced placements)
		int32 Rotation = 0;
		if (MeshInfo.AllowedRotations.Num() > 0)
		{
			Rotation = MeshInfo. AllowedRotations[0];
		}

		// Apply rotation to footprint
		FIntPoint RotatedFootprint = GetRotatedFootprint(Footprint, Rotation);

		// Validate bounds
		if (StartCoord.X + RotatedFootprint.X > GridSize.X || 
		    StartCoord.Y + RotatedFootprint.Y > GridSize.Y)
		{
			UE_LOG(LogTemp, Warning, TEXT("  Forced placement at (%d,%d) is out of bounds (size %dx%d) - skipping"), 
				StartCoord.X, StartCoord.Y, RotatedFootprint.X, RotatedFootprint.Y);
			continue;
		}

		// Check if area is available
		if (! IsAreaAvailable(StartCoord, RotatedFootprint))
		{
			UE_LOG(LogTemp, Warning, TEXT("  Forced placement at (%d,%d) overlaps existing placement - skipping"), 
				StartCoord.X, StartCoord.Y);
			continue;
		}

		// Place the mesh
		if (TryPlaceMesh(StartCoord, RotatedFootprint, MeshInfo, Rotation))
		{
			SuccessfulPlacements++;
			UE_LOG(LogTemp, Verbose, TEXT("  Placed forced mesh at (%d,%d) size %dx%d"), 
				StartCoord.X, StartCoord.Y, RotatedFootprint.X, RotatedFootprint.Y);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator::ExecuteForcedPlacements - Placed %d/%d forced meshes"), 
		SuccessfulPlacements, ForcedPlacements.Num());

	return SuccessfulPlacements;
}

TArray<FIntPoint> URoomGenerator::ExpandForcedEmptyRegions() const
{
	TArray<FIntPoint> ExpandedCells;

	if (! RoomData)
	{
		return ExpandedCells;
	}

	// 1. Expand all rectangular regions into individual cells
	for (const FForcedEmptyRegion& Region : RoomData->ForcedEmptyRegions)
	{
		// Calculate bounding box (handles any corner order)
		int32 MinX = FMath::Min(Region.StartCell.X, Region.EndCell. X);
		int32 MaxX = FMath::Max(Region.StartCell.X, Region.EndCell.X);
		int32 MinY = FMath::Min(Region.StartCell.Y, Region.EndCell. Y);
		int32 MaxY = FMath::Max(Region.StartCell.Y, Region.EndCell.Y);

		// Clamp to valid grid bounds
		MinX = FMath:: Clamp(MinX, 0, GridSize.X - 1);
		MaxX = FMath::Clamp(MaxX, 0, GridSize.X - 1);
		MinY = FMath::Clamp(MinY, 0, GridSize.Y - 1);
		MaxY = FMath:: Clamp(MaxY, 0, GridSize.Y - 1);

		// Add all cells within the rectangular region
		for (int32 Y = MinY; Y <= MaxY; ++Y)
		{
			for (int32 X = MinX; X <= MaxX; ++X)
			{
				FIntPoint Cell(X, Y);
				ExpandedCells.AddUnique(Cell); // AddUnique prevents duplicates
			}
		}
	}

	// 2. Add individual forced empty cells
	for (const FIntPoint& Cell : RoomData->ForcedEmptyFloorCells)
	{
		// Validate cell is within grid bounds
		if (Cell.X >= 0 && Cell.X < GridSize.X && Cell.Y >= 0 && Cell.Y < GridSize.Y)
		{
			ExpandedCells.AddUnique(Cell);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator:: ExpandForcedEmptyRegions - Expanded to %d cells"), ExpandedCells.Num());

	return ExpandedCells;
}

void URoomGenerator::MarkForcedEmptyCells(const TArray<FIntPoint>& EmptyCells)
{
	for (const FIntPoint& Cell :  EmptyCells)
	{
		// Mark as Wall type (reserved/boundary marker)
		SetCellState(Cell, EGridCellType::ECT_Wall);
	}

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator::MarkForcedEmptyCells - Marked %d cells as empty"), EmptyCells.Num());
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