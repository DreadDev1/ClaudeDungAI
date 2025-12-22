// Fill out your copyright notice in the Description page of Project Settings.

#include "Generators/Room/RoomGenerator.h"
#include "Data/Grid/GridData.h"
#include "Data/Room/WallData.h"
#include "Engine/StaticMeshSocket.h"

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

#pragma endregion

#pragma region Floor Generation
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

	// ========================================================================
	// PHASE 3: GAP FILL (Fill remaining empty cells with any available mesh)
	// ========================================================================
	int32 GapFillCount = FillRemainingGaps(FloorMeshes);
	UE_LOG(LogTemp, Log, TEXT("  Phase 3: Filled %d remaining gaps"), GapFillCount);

	// ========================================================================
	// FINAL STATISTICS
	// ========================================================================
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
		if (MeshInfo.MeshAsset. IsNull())
		{
			UE_LOG(LogTemp, Warning, TEXT("  Forced placement at (%d,%d) has null mesh asset - skipping"), 
				StartCoord.X, StartCoord.Y);
			continue;
		}

		// Calculate original footprint
		FIntPoint OriginalFootprint = CalculateFootprint(MeshInfo);

		UE_LOG(LogTemp, Verbose, TEXT("  Attempting forced placement at (%d,%d) with footprint %dx%d"), 
			StartCoord.X, StartCoord.Y, OriginalFootprint.X, OriginalFootprint.Y);

		// ✅ NEW: Try to find a rotation that fits the available space
		int32 BestRotation = -1;
		FIntPoint BestFootprint;

		if (MeshInfo.AllowedRotations.Num() > 0)
		{
			// Try each allowed rotation to find one that fits
			for (int32 Rotation :  MeshInfo.AllowedRotations)
			{
				FIntPoint RotatedFootprint = GetRotatedFootprint(OriginalFootprint, Rotation);

				// Check if this rotation fits within grid bounds
				if (StartCoord.X + RotatedFootprint.X <= GridSize. X && 
				    StartCoord.Y + RotatedFootprint.Y <= GridSize.Y)
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
	{
		UE_LOG(LogTemp, Warning, TEXT("URoomGenerator:: FillRemainingGaps - No meshes in tile pool! "));
		return 0;
	}

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
			if ((Footprint.X == TargetSize.X && Footprint.Y == TargetSize.Y) ||
				(Footprint.X == TargetSize.Y && Footprint. Y == TargetSize.X))
			{
				MatchingTiles.Add(MeshInfo);
			}
		}

		if (MatchingTiles.Num() == 0)
		{
			continue; // No tiles of this size, try next
		}

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
#pragma endregion

#pragma region Wall Generation
bool URoomGenerator::GenerateWalls()
{
	if (!bIsInitialized)
	{
		UE_LOG(LogTemp, Error, TEXT("URoomGenerator::GenerateWalls - Generator not initialized! "));
		return false;
	}

	if (!RoomData || RoomData->WallStyleData.IsNull())
	{
		UE_LOG(LogTemp, Error, TEXT("URoomGenerator::GenerateWalls - WallStyleData not assigned!"));
		return false;
	}

	UWallData* WallData = RoomData->WallStyleData. LoadSynchronous();
	if (!WallData || WallData->AvailableWallModules.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("URoomGenerator::GenerateWalls - No wall modules defined!"));
		return false;
	}

	// Clear previous data
	ClearPlacedWalls();
	PlacedBaseWallSegments.Empty();  // ✅ Clear tracking array

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator::GenerateWalls - Starting wall generation"));

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

void URoomGenerator::ClearPlacedWalls()
{
	PlacedWalls.Empty();
}

void URoomGenerator::SpawnMiddleWallLayers()
{
	if (!RoomData || RoomData->WallStyleData.IsNull())
		return;

	int32 Middle1Spawned = 0;
	int32 Middle2Spawned = 0;

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator::SpawnMiddleWallLayers - Processing %d base segments"), 
		PlacedBaseWallSegments.Num());

	for (const FGeneratorWallSegment& Segment : PlacedBaseWallSegments)
	{
		if (! Segment.WallModule)
			continue;

		// --- MIDDLE 1 LAYER ---
		UStaticMesh* Middle1Mesh = Segment.WallModule->MiddleMesh1.LoadSynchronous();

		if (Middle1Mesh)
		{
			// Get socket from Base mesh
			FVector SocketLocation;
			FRotator SocketRotation;
			bool bHasSocket = GetSocketTransform(Segment.BaseMesh, FName("TopBackCenter"), 
				SocketLocation, SocketRotation);

			if (!bHasSocket)
			{
				// Fallback:  Use WallHeight
				if (RoomData && RoomData->WallStyleData.IsValid())
				{
					UWallData* WallData = RoomData->WallStyleData. LoadSynchronous();
					if (WallData)
					{
						SocketLocation = FVector(0, 0, WallData->WallHeight);
						SocketRotation = FRotator:: ZeroRotator;
					}
				}
			}

			// Calculate Middle1 world transform
			FTransform SocketTransform(SocketRotation, SocketLocation);
			FTransform Middle1WorldTransform = SocketTransform * Segment.BaseTransform;

			// Store for later use (Middle2/Top stacking)
			FPlacedWallInfo PlacedWall;
			PlacedWall.Edge = Segment.Edge;
			PlacedWall.StartCell = Segment.StartCell;
			PlacedWall.SpanLength = Segment.SegmentLength;
			PlacedWall.WallModule = *Segment.WallModule;
			PlacedWall. BottomTransform = Segment.BaseTransform;
			PlacedWall.Middle1Transform = Middle1WorldTransform;

			PlacedWalls.Add(PlacedWall);
			Middle1Spawned++;

			// --- MIDDLE 2 LAYER (only if Middle1 exists) ---
			UStaticMesh* Middle2Mesh = Segment.WallModule->MiddleMesh2.LoadSynchronous();

			if (Middle2Mesh)
			{
				// Get socket from Middle1 mesh
				FVector Middle2SocketLocation;
				FRotator Middle2SocketRotation;
				bool bHasMiddle1Socket = GetSocketTransform(Middle1Mesh, FName("TopBackCenter"), 
					Middle2SocketLocation, Middle2SocketRotation);

				if (!bHasMiddle1Socket)
				{
					// Fallback: Use mesh bounds
					FBoxSphereBounds Bounds = Middle1Mesh->GetBounds();
					Middle2SocketLocation = FVector(0, 0, Bounds.BoxExtent.Z * 2.0f);
					Middle2SocketRotation = FRotator::ZeroRotator;
				}

				// Chain transforms: Base → Middle1 → Middle2
				FTransform Middle1SocketTransform(Middle2SocketRotation, Middle2SocketLocation);
				FTransform Middle2WorldTransform = Middle1SocketTransform * Middle1WorldTransform;

				// Update stored wall info
				PlacedWalls.Last().Middle2Transform = Middle2WorldTransform;
				Middle2Spawned++;
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator:: SpawnMiddleWallLayers - Middle1: %d, Middle2: %d"), 
		Middle1Spawned, Middle2Spawned);
}

void URoomGenerator::SpawnTopWallLayer()
{
	if (!RoomData || RoomData->WallStyleData.IsNull())
		return;

	int32 TopSpawned = 0;

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator:: SpawnTopWallLayer - Processing %d wall segments"), 
		PlacedWalls.Num());

	for (FPlacedWallInfo& Wall : PlacedWalls)
	{
		UStaticMesh* TopMesh = Wall.WallModule. TopMesh.LoadSynchronous();
		if (! TopMesh)
			continue;

		// Determine which layer Top should stack on (priority: Middle2 > Middle1 > Base)
		FTransform StackBaseTransform;
		UStaticMesh* SnapToMesh = nullptr;

		// Load middle meshes to determine stack point
		UStaticMesh* Middle2Mesh = Wall.WallModule.MiddleMesh2.LoadSynchronous();
		UStaticMesh* Middle1Mesh = Wall.WallModule. MiddleMesh1.LoadSynchronous();

		if (Middle2Mesh)
		{
			// Stack on Middle2
			StackBaseTransform = Wall.Middle2Transform;
			SnapToMesh = Middle2Mesh;
		}
		else if (Middle1Mesh)
		{
			// Stack on Middle1
			StackBaseTransform = Wall.Middle1Transform;
			SnapToMesh = Middle1Mesh;
		}
		else
		{
			// Stack directly on Base
			StackBaseTransform = Wall. BottomTransform;
			SnapToMesh = Wall.WallModule.BaseMesh. LoadSynchronous();
		}

		// Get socket from snap-to mesh
		FVector SocketLocation;
		FRotator SocketRotation;
		bool bHasSocket = GetSocketTransform(SnapToMesh, FName("TopBackCenter"), 
			SocketLocation, SocketRotation);

		if (!bHasSocket)
		{
			// Fallback: Use WallHeight
			if (RoomData && RoomData->WallStyleData.IsValid())
			{
				UWallData* WallData = RoomData->WallStyleData.LoadSynchronous();
				if (WallData)
				{
					SocketLocation = FVector(0, 0, WallData->WallHeight);
					SocketRotation = FRotator::ZeroRotator;
				}
			}
		}

		// Calculate Top world transform
		FTransform SocketTransform(SocketRotation, SocketLocation);
		FTransform TopWorldTransform = SocketTransform * StackBaseTransform;

		// Store transform
		Wall.TopTransform = TopWorldTransform;
		TopSpawned++;
	}

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator::SpawnTopWallLayer - Top meshes: %d"), TopSpawned);
}

bool URoomGenerator::GetSocketTransform(UStaticMesh* Mesh, FName SocketName, FVector& OutLocation, FRotator& OutRotation) const
{
	if (!Mesh)
		return false;

	// Find socket in mesh
	UStaticMeshSocket* Socket = Mesh->FindSocket(SocketName);
	if (Socket)
	{
		OutLocation = Socket->RelativeLocation;
		OutRotation = Socket->RelativeRotation;
		return true;
	}

	return false;
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

	if (MatchingTiles.Num() == 0)
	{
		return; // No tiles of this size
	}

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
	if (Rotation == 90 || Rotation == 270)
	{
		return FIntPoint(OriginalFootprint.Y, OriginalFootprint.X);
	}

	// For 0 and 180 degree rotations, keep original
	return OriginalFootprint;
}
#pragma endregion

#pragma region Room Statistics

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

TArray<int32> URoomGenerator::GetEdgeCells(EWallEdge Edge) const
{
	TArray<int32> Cells;

	switch (Edge)
	{
	case EWallEdge::North:  // +X edge (top of grid)
		for (int32 Y = 0; Y < GridSize.Y; ++Y)
			Cells.Add(Y);
		break;

	case EWallEdge:: South:  // -X edge (bottom of grid)
		for (int32 Y = 0; Y < GridSize.Y; ++Y)
			Cells.Add(Y);
		break;

	case EWallEdge::East:    // +Y edge (right side of grid)
		for (int32 X = 0; X < GridSize.X; ++X)
			Cells.Add(X);
		break;

	case EWallEdge::West:   // -Y edge (left side of grid)
		for (int32 X = 0; X < GridSize.X; ++X)
			Cells.Add(X);
		break;

	default:
		break;
	}

	return Cells;
}

FRotator URoomGenerator::GetWallRotation(EWallEdge Edge) const
{
	// All walls face INWARD toward room interior
	switch (Edge)
	{
	case EWallEdge::North:  // X = Max, face South (-X, into room)
		return FRotator(0.0f, 180.0f, 0.0f);

	case EWallEdge::South:  // X = 0, face North (+X, into room)
		return FRotator(0.0f, 0.0f, 0.0f);

	case EWallEdge::East:   // Y = Max, face West (-Y, into room)
		return FRotator(0.0f, 270.0f, 0.0f);

	case EWallEdge::West:   // Y = 0, face East (+Y, into room)
		return FRotator(0.0f, 90.0f, 0.0f);

	default:
		return FRotator::ZeroRotator;
	}
}

FVector URoomGenerator::CalculateWallPosition(EWallEdge Edge, int32 StartCell, int32 SpanLength) const
{
	if (! RoomData || RoomData->WallStyleData.IsNull())
		return FVector::ZeroVector;

	UWallData* WallData = RoomData->WallStyleData. LoadSynchronous();
	if (!WallData)
		return FVector::ZeroVector;

	// Calculate half-span for centering wall segment
	float HalfSpan = (SpanLength * CellSize) * 0.5f;
	FVector Position = FVector::ZeroVector;

	switch (Edge)
	{
	case EWallEdge::North:  // North wall:  X = GridSize.X
		Position = FVector(
			(GridSize.X * CellSize) + WallData->NorthWallOffsetX,
			(StartCell * CellSize) + HalfSpan,
			0.0f
		);
		break;

	case EWallEdge::South:  // South wall: X = 0
		Position = FVector(
			0.0f + WallData->SouthWallOffsetX,
			(StartCell * CellSize) + HalfSpan,
			0.0f
		);
		break;

	case EWallEdge::East:   // East wall: Y = GridSize.Y
		Position = FVector(
			(StartCell * CellSize) + HalfSpan,
			(GridSize.Y * CellSize) + WallData->EastWallOffsetY,
			0.0f
		);
		break;

	case EWallEdge::West:   // West wall: Y = 0
		Position = FVector(
			(StartCell * CellSize) + HalfSpan,
			0.0f + WallData->WestWallOffsetY,
			0.0f
		);
		break;

	default:
		break;
	}

	return Position;
}

void URoomGenerator::FillWallEdge(EWallEdge Edge)
{
	if (!RoomData || RoomData->WallStyleData. IsNull())
		return;

	UWallData* WallData = RoomData->WallStyleData.LoadSynchronous();
	if (!WallData || WallData->AvailableWallModules. Num() == 0)
		return;

	TArray<int32> EdgeCells = GetEdgeCells(Edge);
	if (EdgeCells.Num() == 0)
		return;

	FRotator WallRotation = GetWallRotation(Edge);

	UE_LOG(LogTemp, Verbose, TEXT("  Filling edge %d with %d cells"), (int32)Edge, EdgeCells.Num());

	// Greedy bin packing:  Fill with largest modules first (BASE LAYER ONLY)
	int32 CurrentCell = 0;

	while (CurrentCell < EdgeCells. Num())
	{
		// Find largest module that fits remaining space
		const FWallModule* BestModule = nullptr;
		int32 SpaceLeft = EdgeCells. Num() - CurrentCell;

		for (const FWallModule& Module : WallData->AvailableWallModules)
		{
			if (Module.Y_AxisFootprint <= SpaceLeft)
			{
				if (! BestModule || Module.Y_AxisFootprint > BestModule->Y_AxisFootprint)
				{
					BestModule = &Module;
				}
			}
		}

		if (! BestModule)
		{
			UE_LOG(LogTemp, Warning, TEXT("    No wall module fits remaining %d cells on edge %d"), SpaceLeft, (int32)Edge);
			break;
		}

		// Load base mesh
		UStaticMesh* BaseMesh = BestModule->BaseMesh.LoadSynchronous();
		if (!BaseMesh)
		{
			UE_LOG(LogTemp, Warning, TEXT("    Failed to load base mesh for wall module"));
			break;
		}

		// Calculate position for this wall segment
		FVector BasePosition = CalculateWallPosition(Edge, CurrentCell, BestModule->Y_AxisFootprint);

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

		UE_LOG(LogTemp, VeryVerbose, TEXT("    Tracked %d-cell base wall at cell %d"), 
			BestModule->Y_AxisFootprint, CurrentCell);

		// Advance to next segment
		CurrentCell += BestModule->Y_AxisFootprint;
	}
}
#pragma endregion
