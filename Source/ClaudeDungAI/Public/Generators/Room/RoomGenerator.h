// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/Grid/GridData.h"
#include "Data/Room/RoomData.h"
#include "RoomGenerator.generated.h"

/* Struct to track placed mesh information */
USTRUCT()
struct FPlacedMeshInfo
{
	GENERATED_BODY()

	// Grid position (top-left cell)
	UPROPERTY()
	FIntPoint GridPosition;

	// Size in cells
	UPROPERTY()
	FIntPoint Size;

	// Rotation angle (0, 90, 180, 270)
	UPROPERTY()
	int32 Rotation;

	// Mesh placement info from data asset
	UPROPERTY()
	FMeshPlacementInfo MeshInfo;

	// World transform for spawning
	UPROPERTY()
	FTransform WorldTransform;

	FPlacedMeshInfo() : GridPosition(FIntPoint:: ZeroValue), Size(FIntPoint::ZeroValue), Rotation(0) {}
};

// Wall segment tracking structure (matches MasterRoom's FWallSegmentInfo)
USTRUCT()
struct FGeneratorWallSegment
{
	GENERATED_BODY()

	EWallEdge Edge;
	int32 StartCell;
	int32 SegmentLength;
	FTransform BaseTransform;
	UStaticMesh* BaseMesh;
	const FWallModule* WallModule;  // Reference to module for Middle/Top

	FGeneratorWallSegment() : Edge(EWallEdge::North), StartCell(0), SegmentLength(0), BaseMesh(nullptr), WallModule(nullptr) {}
};

/* RoomGenerator - Pure logic class for room generation Handles grid creation, mesh placement algorithms, and room data processing */
UCLASS()
class CLAUDEDUNGAI_API URoomGenerator : public UObject
{
	GENERATED_BODY()

public:
	/* Initialize the room generator with room data */
	bool Initialize(URoomData* InRoomData, FIntPoint InGridSize);

#pragma region Room Grid Management

	UFUNCTION(BlueprintCallable, Category = "Room Generator")
	void CreateGrid();
	UFUNCTION(BlueprintCallable, Category = "Room Generator")
	void ClearGrid();
	UFUNCTION(BlueprintCallable, Category = "Room Generator")
	void ResetGridCellStates();
	UFUNCTION(BlueprintPure, Category = "Room Generator")
	bool IsInitialized() const { return bIsInitialized; }
	
	const TArray<EGridCellType>& GetGridState() const { return GridState; }
	FIntPoint GetGridSize() const { return GridSize; }
	float GetCellSize() const { return CellSize; }
	EGridCellType GetCellState(FIntPoint GridCoord) const;
	bool SetCellState(FIntPoint GridCoord, EGridCellType NewState);
	bool IsValidGridCoordinate(FIntPoint GridCoord) const;
	bool IsAreaAvailable(FIntPoint StartCoord, FIntPoint Size) const;

	/* Mark a rectangular area as occupied
	 * @param StartCoord - Top-left corner of area @param Size - Size of area in cells (X, Y)
	 * @param CellType - Type to mark cells as @return True if successful */
	bool MarkArea(FIntPoint StartCoord, FIntPoint Size, EGridCellType CellType);

	/*Clear a rectangular area (set to Empty) 
	 * @param StartCoord - Top-left corner of area @param Size - Size of area in cells (X, Y)
	 * @return True if successful*/
	bool ClearArea(FIntPoint StartCoord, FIntPoint Size);
	
#pragma endregion
	
#pragma region Floor Generation

	/* Generate floor meshes using sequential weighted fill algorithm */
	bool GenerateFloor();

	/* Get list of placed floor meshes */
	const TArray<FPlacedMeshInfo>& GetPlacedFloorMeshes() const { return PlacedFloorMeshes; }

	/* Clear all placed floor meshes */
	void ClearPlacedFloorMeshes();

	/* Get floor generation statistics */
	void GetFloorStatistics(int32& OutLargeTiles, int32& OutMediumTiles, int32& OutSmallTiles, int32& OutFillerTiles) const;

	/* Execute forced placements from RoomData
	 * Places designer-specified meshes at exact coordinates before random fill */
	int32 ExecuteForcedPlacements();

	/* Fill remaining empty cells with meshes from the pool */
	int32 FillRemainingGaps(const TArray<FMeshPlacementInfo>& TilePool);

	/**
	 * Expand forced empty regions into individual cell list
	 * Combines rectangular regions and individual cells into unified list */
	TArray<FIntPoint> ExpandForcedEmptyRegions() const;

	/* Mark forced empty cells as reserved (blocked from placement) */
	void MarkForcedEmptyCells(const TArray<FIntPoint>& EmptyCells);
#pragma endregion

#pragma region Wall Generation

	/* Generate walls for all four edges Uses greedy bin packing (largest modules first) */
	bool GenerateWalls();

	/* Get list of placed walls */
	const TArray<FPlacedWallInfo>& GetPlacedWalls() const { return PlacedWalls; }

	int32 ExecuteForcedWallPlacements();

	bool IsCellRangeOccupied(EWallEdge Edge, int32 StartCell, int32 Length) const;
	
	/* Clear all placed walls */
	void ClearPlacedWalls();

	/* Called after base walls are placed */
	void SpawnMiddleWallLayers();

	/* Called after middle walls are placed */
	void SpawnTopWallLayer();

#pragma endregion
	
#pragma region Corner Generation

	/* Generate corner pieces for all 4 corners */
	bool GenerateCorners();

	/* Get list of placed corners */
	const TArray<FPlacedCornerInfo>& GetPlacedCorners() const { return PlacedCorners; }

	/* Clear all placed corners */
	void ClearPlacedCorners();

#pragma endregion
	
#pragma region Coordinate Conversion
	/* Convert grid coordinates to local position (center of cell) */
	FVector GridToLocalPosition(FIntPoint GridCoord) const;

	/* Convert local position to grid coordinates */
	FIntPoint LocalToGridPosition(FVector LocalPos) const;

	/* Get rotated footprint based on rotation angle */
	static FIntPoint GetRotatedFootprint(FIntPoint OriginalFootprint, int32 Rotation);
#pragma endregion

#pragma region Room Statistics

	/* Get count of cells by type */
	int32 GetCellCountByType(EGridCellType CellType) const;

	/* Get percentage of grid occupied */
	float GetOccupancyPercentage() const;

	/* Get total cell count */
	int32 GetTotalCellCount() const { return GridSize.X * GridSize.Y; }
#pragma endregion
	
private:
#pragma region Internal Data

	// Reference to room configuration data
	UPROPERTY()
	URoomData* RoomData;

	// Grid state array (row-major order:  Index = Y * GridSize.X + X)
	UPROPERTY()
	TArray<EGridCellType> GridState;

	// Grid dimensions in cells
	FIntPoint GridSize;

	// Cell size in cm (from CELL_SIZE constant)
	float CellSize;

	// Initialization flag
	bool bIsInitialized;
	
	// Placed floor meshes
	UPROPERTY()
	TArray<FPlacedMeshInfo> PlacedFloorMeshes;

	// Statistics tracking
	int32 LargeTilesPlaced;
	int32 MediumTilesPlaced;
	int32 SmallTilesPlaced;
	int32 FillerTilesPlaced;

	// Placed walls
	UPROPERTY()
	TArray<FPlacedWallInfo> PlacedWalls;

	// Tracked base wall segments for Middle/Top spawning
	UPROPERTY()
	TArray<FGeneratorWallSegment> PlacedBaseWallSegments;
	
	// Placed corners
	UPROPERTY()
	TArray<FPlacedCornerInfo> PlacedCorners;
	
	
#pragma endregion

#pragma region Internal Floor Generation Functions
	// ============================================================================
	// INTERNAL FLOOR GENERATION HELPERS
	// ============================================================================

	/**
	 * Fill grid with tiles of specific size
	 * @param TilePool - Pool of meshes to choose from
	 * @param TargetSize - Target size to match (for filtering)
	 */
	void FillWithTileSize(const TArray<FMeshPlacementInfo>& TilePool, FIntPoint TargetSize);

	/**
	 * Select a mesh from pool using weighted random selection
	 * @param Pool - Pool of meshes with placement weights
	 * @return Selected mesh info
	 */
	FMeshPlacementInfo SelectWeightedMesh(const TArray<FMeshPlacementInfo>& Pool);

	/**
	 * Try to place a mesh at specified location
	 * @param StartCoord - Grid coordinate to place mesh
	 * @param Size - Size of mesh in cells
	 * @param MeshInfo - Mesh placement info
	 * @param Rotation - Rotation angle
	 * @return True if placement successful
	 */
	bool TryPlaceMesh(FIntPoint StartCoord, FIntPoint Size, const FMeshPlacementInfo& MeshInfo, int32 Rotation = 0);

	/**
	 * Calculate footprint size in cells from mesh bounds
	 * @param MeshInfo - Mesh placement info
	 * @return Footprint size in cells
	 */
	FIntPoint CalculateFootprint(const FMeshPlacementInfo& MeshInfo) const;
#pragma endregion

#pragma region Internal Helpers
	/* Convert 2D grid coordinate to 1D array index */
	int32 GridCoordToIndex(FIntPoint GridCoord) const;

	/* Convert 1D array index to 2D grid coordinate */
	FIntPoint IndexToGridCoord(int32 Index) const;

	/* Fill one edge with wall modules using greedy bin packing */
	void FillWallEdge(EWallEdge Edge);
#pragma endregion
	
};