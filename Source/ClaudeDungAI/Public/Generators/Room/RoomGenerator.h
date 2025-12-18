// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/Grid/GridData.h"
#include "Data/Room/RoomData.h"
#include "RoomGenerator.generated.h"

/**
 * RoomGenerator - Pure logic class for room generation
 * Handles grid creation, mesh placement algorithms, and room data processing
 * Does NOT spawn actors or interact with the world
 */
UCLASS()
class CLAUDEDUNGAI_API URoomGenerator : public UObject
{
	GENERATED_BODY()

public:
	// ============================================================================
	// INITIALIZATION
	// ============================================================================

	/**
	 * Initialize the room generator with room data
	 * @param InRoomData - The room configuration data asset
	 * @return True if initialization successful
	 */
	bool Initialize(URoomData* InRoomData);

	/**
	 * Check if generator is properly initialized
	 */
	bool IsInitialized() const { return bIsInitialized; }

	// ============================================================================
	// GRID MANAGEMENT
	// ============================================================================

	/**
	 * Create the grid based on RoomData dimensions
	 * Initializes all cells to Empty state
	 */
	void CreateGrid();

	/**
	 * Clear the grid (reset all cells to Empty)
	 */
	void ClearGrid();

	/**
	 * Get the current grid state
	 */
	const TArray<EGridCellType>& GetGridState() const { return GridState; }

	/**
	 * Get the grid size
	 */
	FIntPoint GetGridSize() const { return GridSize; }

	/**
	 * Get the cell size
	 */
	float GetCellSize() const { return CellSize; }

	/**
	 * Get cell state at specific coordinate
	 * @param GridCoord - The grid coordinate to query
	 * @return Cell type at that coordinate, or Empty if out of bounds
	 */
	EGridCellType GetCellState(FIntPoint GridCoord) const;

	/**
	 * Set cell state at specific coordinate
	 * @param GridCoord - The grid coordinate to set
	 * @param NewState - The new state for this cell
	 * @return True if successful, false if out of bounds
	 */
	bool SetCellState(FIntPoint GridCoord, EGridCellType NewState);

	/**
	 * Check if a grid coordinate is valid
	 */
	bool IsValidGridCoordinate(FIntPoint GridCoord) const;

	/**
	 * Check if a rectangular area is available (all cells Empty)
	 * @param StartCoord - Top-left corner of area
	 * @param Size - Size of area in cells (X, Y)
	 * @return True if all cells in area are Empty
	 */
	bool IsAreaAvailable(FIntPoint StartCoord, FIntPoint Size) const;

	/**
	 * Mark a rectangular area as occupied
	 * @param StartCoord - Top-left corner of area
	 * @param Size - Size of area in cells (X, Y)
	 * @param CellType - Type to mark cells as
	 * @return True if successful
	 */
	bool MarkArea(FIntPoint StartCoord, FIntPoint Size, EGridCellType CellType);

	/**
	 * Clear a rectangular area (set to Empty)
	 * @param StartCoord - Top-left corner of area
	 * @param Size - Size of area in cells (X, Y)
	 * @return True if successful
	 */
	bool ClearArea(FIntPoint StartCoord, FIntPoint Size);

	// ============================================================================
	// COORDINATE CONVERSION HELPERS
	// ============================================================================

	/**
	 * Convert grid coordinates to local position (center of cell)
	 * @param GridCoord - Grid coordinate (X, Y)
	 * @return Local position in cm from room origin
	 */
	FVector GridToLocalPosition(FIntPoint GridCoord) const;

	/**
	 * Convert local position to grid coordinates
	 * @param LocalPos - Local position in cm from room origin
	 * @return Grid coordinate (floored)
	 */
	FIntPoint LocalToGridPosition(FVector LocalPos) const;

	/**
	 * Get rotated footprint based on rotation angle
	 * @param OriginalFootprint - Original mesh footprint
	 * @param Rotation - Rotation angle (0, 90, 180, 270)
	 * @return Rotated footprint (swaps X/Y for 90/270)
	 */
	static FIntPoint GetRotatedFootprint(FIntPoint OriginalFootprint, int32 Rotation);

	// ============================================================================
	// STATISTICS
	// ============================================================================

	/**
	 * Get count of cells by type
	 */
	int32 GetCellCountByType(EGridCellType CellType) const;

	/**
	 * Get percentage of grid occupied
	 */
	float GetOccupancyPercentage() const;

	/**
	 * Get total cell count
	 */
	int32 GetTotalCellCount() const { return GridSize.X * GridSize.Y; }

private:
	// ============================================================================
	// INTERNAL DATA
	// ============================================================================

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

	// ============================================================================
	// INTERNAL HELPERS
	// ============================================================================

	/**
	 * Convert 2D grid coordinate to 1D array index
	 */
	int32 GridCoordToIndex(FIntPoint GridCoord) const;

	/**
	 * Convert 1D array index to 2D grid coordinate
	 */
	FIntPoint IndexToGridCoord(int32 Index) const;
};