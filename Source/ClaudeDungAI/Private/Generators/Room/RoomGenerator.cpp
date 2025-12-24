// Fill out your copyright notice in the Description page of Project Settings.

#include "Generators/Room/RoomGenerator.h"
#include "Utilities/Helpers/DungeonGenerationHelpers.h" 
#include "Data/Grid/GridData.h"
#include "Data/Room/FloorData.h"
#include "Data/Room/WallData.h"

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

#pragma region Room Grid Management
void URoomGenerator::CreateGrid()
{
	if (!bIsInitialized) { UE_LOG(LogTemp, Error, TEXT("URoomGenerator::CreateGrid - Generator not initialized!")); return; }

	// Calculate total cells needed
	int32 TotalCells = GridSize.X * GridSize.Y;

	// Initialize grid with all cells set to Empty
	GridState.Empty(TotalCells);
	GridState.AddUninitialized(TotalCells);

	for (int32 i = 0; i < TotalCells; ++i) { GridState[i] = EGridCellType::ECT_Empty; }
	UE_LOG(LogTemp, Log, TEXT("URoomGenerator::CreateGrid - Created grid with %d cells"), TotalCells);
}

void URoomGenerator:: ClearGrid()
{
	GridState.Empty();
	PlacedFloorMeshes. Empty();
	PlacedWalls.Empty();
	PlacedBaseWallSegments.Empty();

	// Reset statistics
	LargeTilesPlaced = 0;
	MediumTilesPlaced = 0;
	SmallTilesPlaced = 0;
	FillerTilesPlaced = 0;
	
	bIsInitialized = false;

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator::ClearGrid - Grid cleared"));
}

void URoomGenerator::ResetGridCellStates()
{
	if (! bIsInitialized)
	{ UE_LOG(LogTemp, Warning, TEXT("URoomGenerator::ResetGridCellStates - Not initialized! ")); return; }

	// Reset all cells to empty
	int32 CellsReset = 0;
	for (EGridCellType& Cell : GridState)
	{
		if (Cell != EGridCellType::ECT_Empty)
		{
			Cell = EGridCellType:: ECT_Empty;
			CellsReset++;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator::ResetGridCellStates - Reset %d cells to empty (Total: %d)"), 
		CellsReset, GridState.Num());
}

EGridCellType URoomGenerator:: GetCellState(FIntPoint GridCoord) const
{
	if (!IsValidGridCoordinate(GridCoord)) return EGridCellType::ECT_Empty;
	int32 Index = GridCoordToIndex(GridCoord); return GridState[Index];
}

bool URoomGenerator::SetCellState(FIntPoint GridCoord, EGridCellType NewState)
{
	if (!IsValidGridCoordinate(GridCoord))	return false;

	int32 Index = GridCoordToIndex(GridCoord);
	GridState[Index] = NewState; return true;
}

bool URoomGenerator::IsValidGridCoordinate(FIntPoint GridCoord) const
{
	return GridCoord.X >= 0 && GridCoord.X < GridSize.X && GridCoord.Y >= 0 && GridCoord.Y < GridSize.Y;
}

bool URoomGenerator::IsAreaAvailable(FIntPoint StartCoord, FIntPoint Size) const
{
	// Check if entire area fits within grid
	if (StartCoord.X + Size.X > GridSize. X || StartCoord.Y + Size.Y > GridSize.Y) return false;

	// Check if any cell in the area is occupied
	for (int32 X = 0; X < Size.X; ++X)
	{
		for (int32 Y = 0; Y < Size.Y; ++Y)
		{
			FIntPoint CheckCoord(StartCoord.X + X, StartCoord.Y + Y);
			if (GetCellState(CheckCoord) != EGridCellType::ECT_Empty) return false;
		}
	}
	return true;
}

bool URoomGenerator::MarkArea(FIntPoint StartCoord, FIntPoint Size, EGridCellType CellType)
{
	// Validate that area is available
	if (!IsAreaAvailable(StartCoord, Size))	return false;

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
	if (StartCoord.X + Size. X > GridSize.X || StartCoord.Y + Size.Y > GridSize.Y) return false;

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


#pragma endregion

#pragma region Floor Generation
bool URoomGenerator::GenerateFloor()
{
	if (!bIsInitialized)
	{ UE_LOG(LogTemp, Error, TEXT("URoomGenerator::GenerateFloor - Generator not initialized!")); return false; }

	if (! RoomData || !RoomData->FloorStyleData)
	{ UE_LOG(LogTemp, Error, TEXT("URoomGenerator:: GenerateFloor - FloorData not assigned!")); return false; }

	// Load FloorData and keep strong reference throughout function
	UFloorData* FloorStyleData = RoomData->FloorStyleData.LoadSynchronous();
	if (!FloorStyleData)
	{ UE_LOG(LogTemp, Error, TEXT("URoomGenerator::GenerateFloor - Failed to load FloorStyleData!")); return false; }

	// Validate FloorTilePool exists
	if (FloorStyleData->FloorTilePool. Num() == 0)
	{ UE_LOG(LogTemp, Warning, TEXT("URoomGenerator::GenerateFloor - No floor meshes defined in FloorTilePool!")); return false;}
	
	// Clear previous placement data
	ClearPlacedFloorMeshes();

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator::GenerateFloor - Starting floor generation"));

 
	// PHASE 0:  FORCED EMPTY REGIONS (Mark cells as reserved)
 	TArray<FIntPoint> ForcedEmptyCells = ExpandForcedEmptyRegions();
	if (ForcedEmptyCells.Num() > 0)
	{
		MarkForcedEmptyCells(ForcedEmptyCells);
		UE_LOG(LogTemp, Log, TEXT("  Phase 0: Marked %d forced empty cells"), ForcedEmptyCells. Num());
	}
	
	// PHASE 1: FORCED PLACEMENTS (Designer overrides - highest priority)
 	int32 ForcedCount = ExecuteForcedPlacements();
	UE_LOG(LogTemp, Log, TEXT("  Phase 1: Placed %d forced meshes"), ForcedCount);
	
	// PHASE 2: GREEDY FILL (Large → Medium → Small)
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
	
	// PHASE 3: GAP FILL (Fill remaining empty cells with any available mesh)
 	int32 GapFillCount = FillRemainingGaps(FloorMeshes);
	UE_LOG(LogTemp, Log, TEXT("  Phase 3: Filled %d remaining gaps"), GapFillCount);
 
	// FINAL STATISTICS
	int32 RemainingEmpty = GetCellCountByType(EGridCellType::ECT_Empty);
	UE_LOG(LogTemp, Log, TEXT("URoomGenerator::GenerateFloor - Floor generation complete"));
	UE_LOG(LogTemp, Log, TEXT("  Total meshes placed: %d"), PlacedFloorMeshes.Num());
	UE_LOG(LogTemp, Log, TEXT("  Remaining empty cells: %d"), RemainingEmpty);

	return true;
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
	if (! bIsInitialized || !RoomData) 
	{ UE_LOG(LogTemp, Error, TEXT("URoomGenerator::ExecuteForcedPlacements - Not initialized! ")); return 0;}

	int32 SuccessfulPlacements = 0;
	const TMap<FIntPoint, FMeshPlacementInfo>& ForcedPlacements = RoomData->ForcedFloorPlacements;

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator::ExecuteForcedPlacements - Processing %d forced placements"), ForcedPlacements. Num());
	for (const auto& Pair : ForcedPlacements)
	{
		const FIntPoint StartCoord = Pair.Key;
		const FMeshPlacementInfo& MeshInfo = Pair.Value;

		// Validate mesh asset
		if (MeshInfo.MeshAsset. IsNull())
		{
			UE_LOG(LogTemp, Warning, TEXT("  Forced placement at (%d,%d) has null mesh asset - skipping"), StartCoord.X, StartCoord.Y);
			continue;
		}

		// Calculate original footprint
		FIntPoint OriginalFootprint = CalculateFootprint(MeshInfo);

		UE_LOG(LogTemp, Verbose, TEXT("  Attempting forced placement at (%d,%d) with footprint %dx%d"), 
		StartCoord.X, StartCoord.Y, OriginalFootprint.X, OriginalFootprint.Y);

		// Try to find a rotation that fits the available space
		int32 BestRotation = -1;
		FIntPoint BestFootprint;

		if (MeshInfo.AllowedRotations.Num() > 0)
		{
			// Try each allowed rotation to find one that fits
			for (int32 Rotation :  MeshInfo.AllowedRotations)
			{
				FIntPoint RotatedFootprint = GetRotatedFootprint(OriginalFootprint, Rotation);

				// Check if this rotation fits within grid bounds
				if (StartCoord.X + RotatedFootprint.X <= GridSize. X && StartCoord.Y + RotatedFootprint.Y <= GridSize.Y)
				{
					// Check if area is available
					if (IsAreaAvailable(StartCoord, RotatedFootprint))
					{
						BestRotation = Rotation;
						BestFootprint = RotatedFootprint;
						UE_LOG(LogTemp, Verbose, TEXT("    Found valid rotation %d° (footprint %dx%d)"), 
							Rotation, RotatedFootprint.X, RotatedFootprint.Y);
						break; // Use first valid rotation
					}
				}
			}
		}
		else
		{
			// No allowed rotations defined, try default (0°)
			BestRotation = 0;
			BestFootprint = OriginalFootprint;
		}

		// Check if we found a valid rotation
		if (BestRotation == -1)
		{
			UE_LOG(LogTemp, Warning, TEXT("  Forced placement at (%d,%d) cannot fit with any allowed rotation - skipping"), 
				StartCoord.X, StartCoord.Y);
			continue;
		}

		// Validate bounds with best rotation
		if (StartCoord.X + BestFootprint.X > GridSize.X || 
		    StartCoord.Y + BestFootprint. Y > GridSize.Y)
		{
			UE_LOG(LogTemp, Warning, TEXT("  Forced placement at (%d,%d) is out of bounds (size %dx%d) - skipping"), 
				StartCoord.X, StartCoord.Y, BestFootprint. X, BestFootprint.Y);
			continue;
		}

		// Final check if area is available
		if (! IsAreaAvailable(StartCoord, BestFootprint))
		{
			UE_LOG(LogTemp, Warning, TEXT("  Forced placement at (%d,%d) overlaps existing placement - skipping"), 
				StartCoord.X, StartCoord.Y);
			continue;
		}

		// Place the mesh with best rotation
		if (TryPlaceMesh(StartCoord, BestFootprint, MeshInfo, BestRotation))
		{
			SuccessfulPlacements++;
			UE_LOG(LogTemp, Log, TEXT("  ✓ Placed forced mesh at (%d,%d) size %dx%d rotation %d°"), 
				StartCoord.X, StartCoord.Y, BestFootprint. X, BestFootprint.Y, BestRotation);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("  Failed to place forced mesh at (%d,%d) - TryPlaceMesh returned false"), 
				StartCoord.X, StartCoord.Y);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator::ExecuteForcedPlacements - Placed %d/%d forced meshes"), 
		SuccessfulPlacements, ForcedPlacements.Num());

	return SuccessfulPlacements;
}

int32 URoomGenerator::FillRemainingGaps(const TArray<FMeshPlacementInfo>& TilePool)
{
if (TilePool.Num() == 0)
	{ UE_LOG(LogTemp, Warning, TEXT("URoomGenerator:: FillRemainingGaps - No meshes in tile pool! ")); return 0;}

	int32 PlacedCount = 0;

	// Define sizes to try (largest to smallest for efficiency)
	TArray<FIntPoint> SizesToTry = {
		FIntPoint(1, 4), // 100x400
		FIntPoint(4, 1), // 400x100
		FIntPoint(1, 2), // 100x200
		FIntPoint(2, 1), // 200x100
		FIntPoint(1, 1)  // 100x100
	};

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator::FillRemainingGaps - Starting gap fill"));

	// Try each size in order
	for (const FIntPoint& TargetSize : SizesToTry)
	{
		// Filter tiles that match this size
		TArray<FMeshPlacementInfo> MatchingTiles;

		for (const FMeshPlacementInfo& MeshInfo : TilePool)
		{
			FIntPoint Footprint = CalculateFootprint(MeshInfo);

			// Check if footprint matches target size (or rotated version)
			if ((Footprint.X == TargetSize.X && Footprint.Y == TargetSize.Y) || (Footprint.X == TargetSize.Y && Footprint. Y == TargetSize.X))
			{
				MatchingTiles.Add(MeshInfo);
			}
		}

		if (MatchingTiles.Num() == 0) continue; // No tiles of this size, try next

		int32 SizePlacedCount = 0;

		// Try to place tiles of this size in all empty spaces
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
					FIntPoint OriginalFootprint = CalculateFootprint(SelectedMesh);

					// Find rotation that matches target size
					int32 BestRotation = 0;
					
					if (SelectedMesh.AllowedRotations.Num() > 0)
					{
						// Build list of rotations that would fit the target size
						TArray<int32> ValidRotations;

						for (int32 Rotation : SelectedMesh.AllowedRotations)
						{
							FIntPoint RotatedFootprint = GetRotatedFootprint(OriginalFootprint, Rotation);
							
							if (RotatedFootprint.X == TargetSize.X && RotatedFootprint.Y == TargetSize.Y)
							{
								ValidRotations.Add(Rotation);
							}
						}

						// Select random rotation from valid options
						if (ValidRotations.Num() > 0)
						{
							int32 RandomIndex = FMath::RandRange(0, ValidRotations.Num() - 1);
							BestRotation = ValidRotations[RandomIndex];
						}
					}

					// Try to place mesh with rotation
					if (TryPlaceMesh(StartCoord, TargetSize, SelectedMesh, BestRotation))
					{
						SizePlacedCount++;
						PlacedCount++;

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

		if (SizePlacedCount > 0)
		{
			UE_LOG(LogTemp, Verbose, TEXT("  Filled %d gaps with %dx%d tiles"), SizePlacedCount, TargetSize. X, TargetSize.Y);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator::FillRemainingGaps - Placed %d gap-fill meshes"), PlacedCount);

	return PlacedCount;
}

TArray<FIntPoint> URoomGenerator::ExpandForcedEmptyRegions() const
{
	TArray<FIntPoint> ExpandedCells;

	if (! RoomData) return ExpandedCells;

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
		if (Cell.X >= 0 && Cell.X < GridSize.X && Cell.Y >= 0 && Cell.Y < GridSize.Y) {	ExpandedCells.AddUnique(Cell); }
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
#pragma endregion

#pragma region Wall Generation
bool URoomGenerator::GenerateWalls()
{
	if (!bIsInitialized)
	{ UE_LOG(LogTemp, Error, TEXT("URoomGenerator::GenerateWalls - Generator not initialized! ")); return false;}

	if (!RoomData || RoomData->WallStyleData.IsNull())
	{ UE_LOG(LogTemp, Error, TEXT("URoomGenerator::GenerateWalls - WallStyleData not assigned!")); return false;}

	UWallData* WallData = RoomData->WallStyleData. LoadSynchronous();
	if (!WallData || WallData->AvailableWallModules.Num() == 0)
	{ UE_LOG(LogTemp, Error, TEXT("URoomGenerator::GenerateWalls - No wall modules defined!"));	return false;}

	// Clear previous data
	ClearPlacedWalls();
	PlacedBaseWallSegments.Empty();  // ✅ Clear tracking array

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator::GenerateWalls - Starting wall generation"));

	// PASS 0: FORCED WALL PLACEMENTS
	int32 ForcedCount = ExecuteForcedWallPlacements();
	if (ForcedCount > 0) UE_LOG(LogTemp, Log, TEXT("  Phase 0: Placed %d forced walls"), ForcedCount);
	
	// PASS 1: Generate base walls for each edge
	FillWallEdge(EWallEdge::North);
	FillWallEdge(EWallEdge::South);
	FillWallEdge(EWallEdge::East);
	FillWallEdge(EWallEdge::West);

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator::GenerateWalls - Base walls tracked:  %d segments"), 
		PlacedBaseWallSegments.Num());

	// PASS 2: Spawn middle layers using socket-based stacking
	SpawnMiddleWallLayers();

	// PASS 3: Spawn top layer using socket-based stacking
	SpawnTopWallLayer();

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator::GenerateWalls - Complete.  Total wall records: %d"), 
		PlacedWalls.Num());

	return true;
}

int32 URoomGenerator::ExecuteForcedWallPlacements()
{
	if (!bIsInitialized || !RoomData)
	{ UE_LOG(LogTemp, Error, TEXT("URoomGenerator::ExecuteForcedWallPlacements - Not initialized!")); return 0;	}

	// Check if there are any forced placements
	if (RoomData->ForcedWallPlacements.Num() == 0)
	{ UE_LOG(LogTemp, Verbose, TEXT("URoomGenerator:: ExecuteForcedWallPlacements - No forced walls to place")); return 0;}

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator::ExecuteForcedWallPlacements - Processing %d forced walls"), 
		RoomData->ForcedWallPlacements.Num());

	int32 SuccessfulPlacements = 0;
	int32 FailedPlacements = 0;

	// Get wall offsets once (used for all walls)
	float NorthOffset = 0.0f;
	float SouthOffset = 0.0f;
	float EastOffset = 0.0f;
	float WestOffset = 0.0f;

	if (RoomData->WallStyleData.IsValid())
	{
		UWallData* WallData = RoomData->WallStyleData.LoadSynchronous();
		if (WallData)
		{
			NorthOffset = WallData->NorthWallOffsetX;
			SouthOffset = WallData->SouthWallOffsetX;
			EastOffset = WallData->EastWallOffsetY;
			WestOffset = WallData->WestWallOffsetY;
		}
	}

	for (int32 i = 0; i < RoomData->ForcedWallPlacements.Num(); ++i)
	{
		const FForcedWallPlacement& ForcedWall = RoomData->ForcedWallPlacements[i];
		const FWallModule& Module = ForcedWall.WallModule;

		UE_LOG(LogTemp, Verbose, TEXT("  Forced Wall [%d]: Edge=%s, StartCell=%d, Footprint=%d"), i, 
		*UEnum::GetValueAsString(ForcedWall.Edge), ForcedWall.StartCell, Module.Y_AxisFootprint);
	 
		// VALIDATION:  Load Base Mesh
		UStaticMesh* BaseMesh = UDungeonGenerationHelpers::LoadAndValidateMesh( Module.BaseMesh,
			FString:: Printf(TEXT("ForcedWall[%d]"), i), true);

		if (!BaseMesh)
		{
			UE_LOG(LogTemp, Warning, TEXT("    SKIPPED: BaseMesh failed to load"));
			FailedPlacements++;
			continue;
		}

	 
		// VALIDATION: Check Edge Cells
		TArray<int32> EdgeCells = UDungeonGenerationHelpers::GetEdgeCellIndices(ForcedWall.Edge, GridSize);

		if (EdgeCells.Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("    SKIPPED: No cells on edge %s"), *UEnum::GetValueAsString(ForcedWall.Edge));
			FailedPlacements++;
			continue;
		}
	 
		// VALIDATION: Check Bounds
	 	int32 Footprint = Module.Y_AxisFootprint;
		if (ForcedWall.StartCell < 0 || ForcedWall.StartCell + Footprint > EdgeCells.Num())
		{
			UE_LOG(LogTemp, Warning, TEXT("    SKIPPED: Out of bounds (StartCell=%d, Footprint=%d, EdgeLength=%d)"),
				ForcedWall.StartCell, Footprint, EdgeCells. Num());
			FailedPlacements++;
			continue;
		}

		// NOTE: Overlap checking with other forced walls would happen here
		// For now, we assume designers don't create overlapping forced placements
	 
		// PLACEMENT: Calculate Position & Rotation Using Helpers
		FVector WallPosition = UDungeonGenerationHelpers:: CalculateWallPosition(
		ForcedWall.Edge, ForcedWall.StartCell, Footprint, GridSize, CellSize,
		NorthOffset, SouthOffset, EastOffset, WestOffset);

		FRotator WallRotation = UDungeonGenerationHelpers::GetWallRotationForEdge(ForcedWall.Edge);
	 
		// PLACEMENT: Create Base Wall Transform
		FTransform BaseTransform(WallRotation, WallPosition, FVector:: OneVector);
	 
		// TRACKING: Store Segment for Middle/Top Spawning
		FGeneratorWallSegment Segment;
		Segment.Edge = ForcedWall.Edge;
		Segment.StartCell = ForcedWall.StartCell;
		Segment. SegmentLength = Footprint;
		Segment.BaseTransform = BaseTransform;
		Segment.BaseMesh = BaseMesh;
		Segment.WallModule = &Module;  // Store pointer to module data

		PlacedBaseWallSegments.Add(Segment);

		UE_LOG(LogTemp, Verbose, TEXT("    ✓ Forced wall tracked: Edge=%s, StartCell=%d, Footprint=%d"),
		*UEnum::GetValueAsString(ForcedWall.Edge), ForcedWall.StartCell, Footprint);
		SuccessfulPlacements++;
	}

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator::ExecuteForcedWallPlacements - Placed %d/%d forced walls (%d failed)"),
	SuccessfulPlacements, RoomData->ForcedWallPlacements. Num(), FailedPlacements);
	return SuccessfulPlacements;
}

bool URoomGenerator::IsCellRangeOccupied(EWallEdge Edge, int32 StartCell, int32 Length) const
{
	// Check if any forced wall overlaps with this range
	for (const FGeneratorWallSegment& Segment : PlacedBaseWallSegments)
	{
		if (Segment.Edge != Edge) continue;

		// Check for overlap:  [Start1, End1) overlaps [Start2, End2) if Start1 < End2 AND Start2 < End1
		int32 SegmentEnd = Segment.StartCell + Segment.SegmentLength;
		int32 RangeEnd = StartCell + Length;

		if (StartCell < SegmentEnd && Segment.StartCell < RangeEnd) return true;  // Overlap detected
	}
	return false;
}

void URoomGenerator::ClearPlacedWalls()
{
	PlacedWalls.Empty();
}

void URoomGenerator::SpawnMiddleWallLayers()
{
	if (!RoomData || RoomData->WallStyleData.IsNull()) return;

	// Get fallback height from WallData
	float FallbackHeight = 100.0f;
	UWallData* WallData = RoomData->WallStyleData.LoadSynchronous();
	if (WallData) { FallbackHeight = WallData->WallHeight;}

	int32 Middle1Spawned = 0;
	int32 Middle2Spawned = 0;

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator::SpawnMiddleWallLayers - Processing %d base segments"), PlacedBaseWallSegments.Num());

	for (const FGeneratorWallSegment& Segment : PlacedBaseWallSegments)
	{
		if (!Segment.WallModule) continue;

		// MIDDLE 1 LAYER
		UStaticMesh* Middle1Mesh = Segment.WallModule->MiddleMesh1.LoadSynchronous();

		if (Middle1Mesh)
		{
			// ✅ Single line replaces 15+ lines of socket querying
			FTransform Middle1WorldTransform = UDungeonGenerationHelpers::CalculateSocketWorldTransform(
				Segment.BaseMesh,
				FName("TopBackCenter"),
				Segment.BaseTransform,
				FVector(0, 0, FallbackHeight));

			// Store wall info
			FPlacedWallInfo PlacedWall;
			PlacedWall.Edge = Segment.Edge;
			PlacedWall.StartCell = Segment.StartCell;
			PlacedWall.SpanLength = Segment.SegmentLength;
			PlacedWall.WallModule = *Segment.WallModule;
			PlacedWall.BottomTransform = Segment.BaseTransform;
			PlacedWall.Middle1Transform = Middle1WorldTransform;

			PlacedWalls.Add(PlacedWall);
			Middle1Spawned++;

			// MIDDLE 2 LAYER
			UStaticMesh* Middle2Mesh = Segment.WallModule->MiddleMesh2.LoadSynchronous();

			if (Middle2Mesh)
			{
				// ✅ Single line replaces 10+ lines of socket querying
				FTransform Middle2WorldTransform = UDungeonGenerationHelpers::CalculateSocketWorldTransform(
				Middle1Mesh, FName("TopBackCenter"), Middle1WorldTransform, FVector(0, 0, FallbackHeight));
				PlacedWalls.Last().Middle2Transform = Middle2WorldTransform;
				Middle2Spawned++;
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator::SpawnMiddleWallLayers - Middle1: %d, Middle2: %d"), Middle1Spawned, Middle2Spawned);
}

void URoomGenerator::SpawnTopWallLayer()
{
	if (! RoomData || RoomData->WallStyleData.IsNull()) return;

	// Get fallback height from WallData
	float FallbackHeight = 100.0f;  // Default fallback
	UWallData* WallData = RoomData->WallStyleData.LoadSynchronous();
	if (WallData) {	FallbackHeight = WallData->WallHeight;}

	int32 TopSpawned = 0;

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator:: SpawnTopWallLayer - Processing %d wall segments"), PlacedWalls.Num());

	for (FPlacedWallInfo& Wall :  PlacedWalls)
	{
		// Load top mesh (required)
		UStaticMesh* TopMesh = Wall.WallModule. TopMesh.LoadSynchronous();
		if (!TopMesh)continue;
	 
		// DETERMINE WHICH LAYER TO STACK ON (Priority:  Middle2 > Middle1 > Base)
		// Load middle meshes to determine stack point
		UStaticMesh* Middle2Mesh = Wall.WallModule.MiddleMesh2.LoadSynchronous();
		UStaticMesh* Middle1Mesh = Wall.WallModule. MiddleMesh1.LoadSynchronous();

		UStaticMesh* SnapToMesh = nullptr;
		FTransform StackBaseTransform;

		// Priority 1: Stack on Middle2 (if it exists)
		if (Middle2Mesh)
		{
			SnapToMesh = Middle2Mesh;
			StackBaseTransform = Wall.Middle2Transform;
		}
		// Priority 2: Stack on Middle1 (if Middle2 doesn't exist)
		else if (Middle1Mesh)
		{
			SnapToMesh = Middle1Mesh;
			StackBaseTransform = Wall.Middle1Transform;
		}
		// Priority 3: Stack directly on Base
		else
		{
			SnapToMesh = Wall.WallModule.BaseMesh. LoadSynchronous();
			StackBaseTransform = Wall. BottomTransform;
		}
	 
		// CALCULATE TOP TRANSFORM USING SOCKET HELPER
		FTransform TopWorldTransform = UDungeonGenerationHelpers::CalculateSocketWorldTransform(
		SnapToMesh, FName("TopBackCenter"), StackBaseTransform, FVector(0, 0, FallbackHeight));
		// Store transform
		Wall.TopTransform = TopWorldTransform;
		TopSpawned++;
	}

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator::SpawnTopWallLayer - Top meshes: %d"), TopSpawned);
}
#pragma endregion
#pragma region Corner Generation
bool URoomGenerator::GenerateCorners()
{
	 if (!bIsInitialized)
    {
        UE_LOG(LogTemp, Error, TEXT("URoomGenerator:: GenerateCorners - Generator not initialized! "));
        return false;
    }

    if (! RoomData || RoomData->WallStyleData. IsNull())
    {
        UE_LOG(LogTemp, Error, TEXT("URoomGenerator:: GenerateCorners - WallStyleData not assigned!"));
        return false;
    }

    UWallData* WallData = RoomData->WallStyleData.LoadSynchronous();
    if (!WallData)
    {
        UE_LOG(LogTemp, Error, TEXT("URoomGenerator::GenerateCorners - Failed to load WallStyleData!"));
        return false;
    }

    // Clear previous corners
    ClearPlacedCorners();

    UE_LOG(LogTemp, Log, TEXT("URoomGenerator::GenerateCorners - Starting corner generation"));

    // Load corner mesh (required)
    if (WallData->DefaultCornerMesh.IsNull())
    {
        UE_LOG(LogTemp, Warning, TEXT("URoomGenerator::GenerateCorners - No default corner mesh defined, skipping corners"));
        return true; // Not an error, just no corners to place
    }

    UStaticMesh* CornerMesh = WallData->DefaultCornerMesh.LoadSynchronous();
    if (!CornerMesh)
    {
        UE_LOG(LogTemp, Warning, TEXT("URoomGenerator::GenerateCorners - Failed to load corner mesh"));
        return false;
    }

    // ========================================================================
    // Define corner data (matching MasterRoom's clockwise order:  SW, SE, NE, NW)
    // ========================================================================
    
    struct FCornerData
    {
        ECornerPosition Position;
        FVector BasePosition;  // Grid corner position (before offset)
        FRotator Rotation;     // From WallData
        FVector Offset;        // Per-corner offset from WallData
        FString Name;          // For logging
    };

    TArray<FCornerData> Corners = {
        { 
            ECornerPosition::SouthWest, 
            FVector(0.0f, 0.0f, 0.0f),  // Bottom-left
            WallData->SouthWestCornerRotation, 
            WallData->SouthWestCornerOffset,
            TEXT("SouthWest")
        },
        { 
            ECornerPosition::SouthEast, 
            FVector(0.0f, GridSize.Y * CellSize, 0.0f),  // Bottom-right
            WallData->SouthEastCornerRotation, 
            WallData->SouthEastCornerOffset,
            TEXT("SouthEast")
        },
        { 
            ECornerPosition::NorthEast, 
            FVector(GridSize.X * CellSize, GridSize.Y * CellSize, 0.0f),  // Top-right
            WallData->NorthEastCornerRotation, 
            WallData->NorthEastCornerOffset,
            TEXT("NorthEast")
        },
        { 
            ECornerPosition:: NorthWest, 
            FVector(GridSize.X * CellSize, 0.0f, 0.0f),  // Top-left
            WallData->NorthWestCornerRotation, 
            WallData->NorthWestCornerOffset,
            TEXT("NorthWest")
        }
    };

    // ========================================================================
    // Generate each corner
    // ========================================================================
    
    for (const FCornerData& CornerData : Corners)
    {
        // Apply designer offset to base position
        FVector FinalPosition = CornerData. BasePosition + CornerData. Offset;

        // Create transform (local/component space)
        FTransform CornerTransform(CornerData.Rotation, FinalPosition, FVector:: OneVector);

        // Create placed corner info
        FPlacedCornerInfo PlacedCorner;
        PlacedCorner.Corner = CornerData. Position;
        PlacedCorner.CornerMesh = WallData->DefaultCornerMesh;
        PlacedCorner.Transform = CornerTransform;

        PlacedCorners.Add(PlacedCorner);

        UE_LOG(LogTemp, Verbose, TEXT("  Placed %s corner at position %s with rotation (%.0f, %.0f, %.0f)"),
            *CornerData.Name,
            *FinalPosition.ToString(),
            CornerData. Rotation.Roll,
            CornerData.Rotation. Pitch,
            CornerData.Rotation.Yaw);
    }

    UE_LOG(LogTemp, Log, TEXT("URoomGenerator::GenerateCorners - Complete.  Placed %d corners"), PlacedCorners.Num());

    return true;
}

void URoomGenerator::ClearPlacedCorners()
{
	PlacedCorners.Empty();
}
#pragma endregion

#pragma region Internal Floor Generation

void URoomGenerator::FillWithTileSize(const TArray<FMeshPlacementInfo>& TilePool, FIntPoint TargetSize)
{
	// Filter tiles that match target size
	TArray<FMeshPlacementInfo> MatchingTiles;

	for (const FMeshPlacementInfo& MeshInfo : TilePool)
	{
		FIntPoint Footprint = CalculateFootprint(MeshInfo);

		// Check if footprint matches target size (or rotated version)
		if ((Footprint.X == TargetSize.X && Footprint.Y == TargetSize.Y) ||
			(Footprint.X == TargetSize.Y && Footprint. Y == TargetSize.X))
		{
			MatchingTiles.Add(MeshInfo);
		}
	}

	if (MatchingTiles.Num() == 0) return; // No tiles of this size

	UE_LOG(LogTemp, Verbose, TEXT("URoomGenerator::FillWithTileSize - Filling with %dx%d tiles (%d options)"), 
		TargetSize.X, TargetSize.Y, MatchingTiles.Num());

	// Try to place tiles of this size across the grid
	for (int32 Y = 0; Y < GridSize.Y; ++Y)
	{
		for (int32 X = 0; X < GridSize.X; ++X)
		{
			FIntPoint StartCoord(X, Y);

			// Check if area is available for target size
			if (IsAreaAvailable(StartCoord, TargetSize))
			{
				// Select weighted random mesh
				FMeshPlacementInfo SelectedMesh = SelectWeightedMesh(MatchingTiles);
				FIntPoint OriginalFootprint = CalculateFootprint(SelectedMesh);

				// Find rotation that matches target size
				int32 BestRotation = 0;
				
				if (SelectedMesh.AllowedRotations.Num() > 0)
				{
					// Build list of rotations that would fit the target size
					TArray<int32> ValidRotations;

					for (int32 Rotation :  SelectedMesh.AllowedRotations)
					{
						FIntPoint RotatedFootprint = GetRotatedFootprint(OriginalFootprint, Rotation);
						
						if (RotatedFootprint.X == TargetSize.X && RotatedFootprint.Y == TargetSize.Y)
						{
							ValidRotations.Add(Rotation);
						}
					}

					// Select random rotation from valid options
					if (ValidRotations.Num() > 0)
					{
						int32 RandomIndex = FMath::RandRange(0, ValidRotations.Num() - 1);
						BestRotation = ValidRotations[RandomIndex];
					}
				}

				// Try to place mesh with selected rotation
				if (TryPlaceMesh(StartCoord, TargetSize, SelectedMesh, BestRotation))
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
	if (Pool.Num() == 0) return FMeshPlacementInfo(); // Return empty if no options

	if (Pool.Num() == 1) return Pool[0]; // Only one option

	// Calculate total weight
	float TotalWeight = 0.0f;
	for (const FMeshPlacementInfo& MeshInfo : Pool) { TotalWeight += MeshInfo.PlacementWeight; }

	// Random value between 0 and total weight
	float RandomValue = FMath::FRandRange(0.0f, TotalWeight);

	// Select mesh based on weight
	float CurrentWeight = 0.0f;
	for (const FMeshPlacementInfo& MeshInfo : Pool)
	{
		CurrentWeight += MeshInfo.PlacementWeight;
		if (RandomValue <= CurrentWeight) return MeshInfo;
	}

	// Fallback to last mesh
	return Pool. Last();
}

bool URoomGenerator::TryPlaceMesh(FIntPoint StartCoord, FIntPoint Size, const FMeshPlacementInfo& MeshInfo, int32 Rotation)
{
	// Check if area is available
	if (! IsAreaAvailable(StartCoord, Size)) return false;

	// Mark area as occupied
	if (!MarkArea(StartCoord, Size, EGridCellType::ECT_FloorMesh)) return false;
	
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
	
	FVector LocalPos = FVector( StartCoord. X * CellSize + OffsetX, StartCoord.Y * CellSize + OffsetY, 0.0f);

	PlacedMesh.WorldTransform = FTransform(FRotator(0, Rotation, 0), 
		LocalPos,FVector::OneVector );

	// Store placed mesh
	PlacedFloorMeshes.Add(PlacedMesh);

	return true;
}

FIntPoint URoomGenerator::CalculateFootprint(const FMeshPlacementInfo& MeshInfo) const
{
	// If footprint is explicitly defined, use it
	if (MeshInfo.GridFootprint.X > 0 && MeshInfo. GridFootprint.Y > 0) return MeshInfo.GridFootprint;

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
#pragma endregion

#pragma region Coordinate Conversion

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
	if (Rotation == 90 || Rotation == 270) return FIntPoint(OriginalFootprint.Y, OriginalFootprint.X);

	// For 0 and 180 degree rotations, keep original
	return OriginalFootprint;
}
#pragma endregion

#pragma region Room Statistics

int32 URoomGenerator::GetCellCountByType(EGridCellType CellType) const
{
	int32 Count = 0;
	for (const EGridCellType& Cell : GridState)
	{ if (Cell == CellType) ++Count; } return Count;
}

float URoomGenerator::GetOccupancyPercentage() const
{
	int32 TotalCells = GetTotalCellCount();
	if (TotalCells == 0) return 0.0f;

	int32 OccupiedCells = GetCellCountByType(EGridCellType::ECT_FloorMesh);
	return (static_cast<float>(OccupiedCells) / static_cast<float>(TotalCells)) * 100.0f;
}
#pragma endregion

#pragma region Internal Helpers

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

void URoomGenerator::FillWallEdge(EWallEdge Edge)
{
	if (!RoomData || RoomData->WallStyleData. IsNull()) return;

	UWallData* WallData = RoomData->WallStyleData.LoadSynchronous();
	if (!WallData || WallData->AvailableWallModules. Num() == 0) return;

	TArray<int32> EdgeCells = UDungeonGenerationHelpers:: GetEdgeCellIndices(Edge, GridSize);  // ✅ New
	if (EdgeCells.Num() == 0) return;

	FRotator WallRotation = UDungeonGenerationHelpers::GetWallRotationForEdge(Edge);  // ✅ New
	UE_LOG(LogTemp, Verbose, TEXT("  Filling edge %d with %d cells"),	(int32)Edge, EdgeCells.Num());

	// Greedy bin packing:  Fill with largest modules first (BASE LAYER ONLY)
	int32 CurrentCell = 0;

	while (CurrentCell < EdgeCells. Num())
	{
		// Skip cells occupied by forced walls
		if (IsCellRangeOccupied(Edge, CurrentCell, 1))
		{
			UE_LOG(LogTemp, VeryVerbose, TEXT("    Skipping cell %d (occupied by forced wall)"), CurrentCell);
			CurrentCell++;
			continue;
		}
		
		// Find largest module that fits remaining space
		const FWallModule* BestModule = nullptr;
		int32 SpaceLeft = EdgeCells. Num() - CurrentCell;

		for (const FWallModule& Module : WallData->AvailableWallModules)
		{
			if (Module.Y_AxisFootprint <= SpaceLeft && 
				! IsCellRangeOccupied(Edge, CurrentCell, Module.Y_AxisFootprint))
			{
				if (! BestModule || Module.Y_AxisFootprint > BestModule->Y_AxisFootprint) BestModule = &Module;
			}
		}

		if (! BestModule)
		{
			UE_LOG(LogTemp, Warning, TEXT("    No wall module fits remaining %d cells on edge %d"), 
			SpaceLeft, (int32)Edge); CurrentCell++;  // Skip this cell and try next
			continue;
		}

		// Load base mesh
		UStaticMesh* BaseMesh = BestModule->BaseMesh.LoadSynchronous();
		if (!BaseMesh) { UE_LOG(LogTemp, Warning, TEXT("    Failed to load base mesh for wall module")); break; }

		// Calculate position for this wall segment
		FVector BasePosition = UDungeonGenerationHelpers::CalculateWallPosition(
		Edge, CurrentCell,BestModule->Y_AxisFootprint, GridSize, CellSize,
		WallData->NorthWallOffsetX, WallData->SouthWallOffsetX,	WallData->EastWallOffsetY,WallData->WestWallOffsetY);  // ✅ New

		// Create base wall transform
		FTransform BaseTransform(WallRotation, BasePosition, FVector:: OneVector);

		// Store segment info for Middle/Top spawning (CRITICAL - matches MasterRoom)
		FGeneratorWallSegment Segment;
		Segment.Edge = Edge;
		Segment. StartCell = CurrentCell;
		Segment.SegmentLength = BestModule->Y_AxisFootprint;
		Segment. BaseTransform = BaseTransform;
		Segment.BaseMesh = BaseMesh;
		Segment.WallModule = BestModule;  // Store pointer to module data

		PlacedBaseWallSegments.Add(Segment);

		UE_LOG(LogTemp, VeryVerbose, TEXT("    Tracked %d-cell base wall at cell %d"), BestModule->Y_AxisFootprint, CurrentCell);

		// Advance to next segment
		CurrentCell += BestModule->Y_AxisFootprint;
	}
}
#pragma endregion
