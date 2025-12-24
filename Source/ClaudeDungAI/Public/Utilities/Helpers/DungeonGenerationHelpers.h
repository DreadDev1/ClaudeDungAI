// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Data/Grid/GridData.h"
#include "DungeonGenerationHelpers.generated.h"

/**
 * DungeonGenerationHelpers - Static utility functions for dungeon generation
 * 
 * This library contains pure helper functions used across dungeon generation systems. 
 * All functions are static and stateless for maximum reusability.
 */
UCLASS()
class CLAUDEDUNGAI_API UDungeonGenerationHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
	public:
	// ========================================================================
	// GRID & CELL OPERATIONS
	// ========================================================================

	/**
	 * Get cell indices for a specific wall edge
	 * @param Edge - Which edge to get cells for
	 * @param GridSize - Size of the grid
	 * @return Array of cell indices along that edge
	 */
	UFUNCTION(BlueprintPure, Category = "Dungeon Generation|Grid")
	static TArray<FIntPoint> GetEdgeCellIndices(EWallEdge Edge, FIntPoint GridSize);
	

	/**
	 * Check if a coordinate is within grid bounds
	 * @param Coord - Coordinate to check
	 * @param GridSize - Size of the grid
	 * @return True if coordinate is valid
	 */
	UFUNCTION(BlueprintPure, Category = "Dungeon Generation|Grid")
	static bool IsValidGridCoordinate(FIntPoint Coord, FIntPoint GridSize);

	/**
	 * Convert 1D index to 2D grid coordinate
	 * @param Index - 1D array index
	 * @param GridWidth - Width of the grid (X dimension)
	 * @return 2D grid coordinate
	 */
	UFUNCTION(BlueprintPure, Category = "Dungeon Generation|Grid")
	static FIntPoint IndexToCoordinate(int32 Index, int32 GridWidth);

	/**
	 * Convert 2D grid coordinate to 1D index
	 * @param Coord - 2D grid coordinate
	 * @param GridWidth - Width of the grid (X dimension)
	 * @return 1D array index
	 */
	UFUNCTION(BlueprintPure, Category = "Dungeon Generation|Grid")
	static int32 CoordinateToIndex(FIntPoint Coord, int32 GridWidth);

	// ========================================================================
	// ROTATION & FOOTPRINT OPERATIONS
	// ========================================================================

	/**
	 * Calculate rotated footprint for a mesh
	 * @param OriginalFootprint - Original footprint dimensions
	 * @param RotationDegrees - Rotation in degrees (0, 90, 180, 270)
	 * @return Rotated footprint dimensions
	 */
	UFUNCTION(BlueprintPure, Category = "Dungeon Generation|Rotation")
	static FIntPoint GetRotatedFootprint(FIntPoint OriginalFootprint, int32 RotationDegrees);

	/**
	 * Check if rotation swaps X and Y dimensions
	 * @param RotationDegrees - Rotation in degrees
	 * @return True if 90° or 270° rotation
	 */
	UFUNCTION(BlueprintPure, Category = "Dungeon Generation|Rotation")
	static bool DoesRotationSwapDimensions(int32 RotationDegrees);

	// ========================================================================
	// WALL EDGE OPERATIONS
	// ========================================================================

	/**
	 * Get rotation for walls on a specific edge (all face inward)
	 * @param Edge - Wall edge
	 * @return Rotation for walls on that edge
	 */
	UFUNCTION(BlueprintPure, Category = "Dungeon Generation|Walls")
	static FRotator GetWallRotationForEdge(EWallEdge Edge);

	/**
	 * Calculate world position for a wall segment
	 * @param Edge - Wall edge
	 * @param StartCell - Starting cell index
	 * @param SpanLength - Number of cells the wall spans
	 * @param GridSize - Size of the grid
	 * @param CellSize - Size of each cell in world units
	 * @param NorthOffset - North wall offset (X-axis)
	 * @param SouthOffset - South wall offset (X-axis)
	 * @param EastOffset - East wall offset (Y-axis)
	 * @param WestOffset - West wall offset (Y-axis)
	 * @return World position for the wall
	 */
	UFUNCTION(BlueprintPure, Category = "Dungeon Generation|Walls")
	static FVector CalculateWallPosition( EWallEdge Edge, int32 StartCell, int32 SpanLength, FIntPoint GridSize, float CellSize,
	float NorthOffset, float SouthOffset, float EastOffset, float WestOffset);

	/* Calculate doorway center position (for frame/actor placement) */
	UFUNCTION(BlueprintPure, Category = "Dungeon Generation|Doorways")
	static FVector CalculateDoorwayPosition(EWallEdge Edge, int32 StartCell, 
	int32 WidthInCells, FIntPoint GridSize, float CellSize);
	
	// ========================================================================
	// MESH OPERATIONS
	// ========================================================================

	/**
	 * Load and validate a static mesh with error logging
	 * @param MeshAsset - Soft object pointer to mesh
	 * @param ContextName - Name for logging context (e.g., "FloorTile", "WallSegment")
	 * @param bLogWarning - Whether to log warning if mesh fails to load
	 * @return Loaded mesh or nullptr if failed
	 */
	static UStaticMesh* LoadAndValidateMesh(
		const TSoftObjectPtr<UStaticMesh>& MeshAsset,
		const FString& ContextName,
		bool bLogWarning = true);

	// ========================================================================
	// SOCKET OPERATIONS
	// ========================================================================

	/**
	 * Get socket transform from a static mesh
	 * @param Mesh - Mesh to query
	 * @param SocketName - Name of socket
	 * @param OutLocation - Socket location (output)
	 * @param OutRotation - Socket rotation (output)
	 * @return True if socket was found
	 */
	UFUNCTION(BlueprintPure, Category = "Dungeon Generation|Mesh")
	static bool GetMeshSocketTransform(
		UStaticMesh* Mesh,
		FName SocketName,
		FVector& OutLocation,
		FRotator& OutRotation);

	/**
	 * Get socket transform with fallback
	 * @param Mesh - Mesh to query
	 * @param SocketName - Name of socket
	 * @param OutLocation - Socket location (output)
	 * @param OutRotation - Socket rotation (output)
	 * @param FallbackLocation - Fallback location if socket not found
	 * @param FallbackRotation - Fallback rotation if socket not found
	 * @return True if socket was found (false if using fallback)
	 */
	static bool GetMeshSocketTransformWithFallback(
		UStaticMesh* Mesh,
		FName SocketName,
		FVector& OutLocation,
		FRotator& OutRotation,
		FVector FallbackLocation = FVector::ZeroVector,
		FRotator FallbackRotation = FRotator::ZeroRotator);

	/**
	 * Calculate world transform for a mesh stacked on a socket
	 * @param Mesh - Mesh with socket
	 * @param SocketName - Name of socket
	 * @param ParentTransform - Transform of parent mesh
	 * @param FallbackOffset - Fallback offset if socket not found
	 * @return World transform for child mesh
	 */
	static FTransform CalculateSocketWorldTransform(
		UStaticMesh* Mesh,
		FName SocketName,
		const FTransform& ParentTransform,
		FVector FallbackOffset = FVector::ZeroVector);

	// ========================================================================
	// WEIGHTED RANDOM SELECTION
	// ========================================================================

	/**
	 * Select random item from array using weighted selection
	 * @param Items - Array of items
	 * @param GetWeightFunc - Lambda to extract weight from item
	 * @return Pointer to selected item or nullptr if array empty
	 */
	template<typename T>
	static const T* SelectWeightedRandom(
		const TArray<T>& Items,
		TFunction<float(const T&)> GetWeightFunc);

	/**
	 * Select random wall module using weighted selection
	 * @param Modules - Array of wall modules
	 * @return Pointer to selected module or nullptr
	 */
	static const FWallModule* SelectWeightedWallModule(const TArray<FWallModule>& Modules);

	/**
	 * Select random mesh placement info using weighted selection
	 * @param MeshPool - Array of mesh placement info
	 * @return Pointer to selected mesh info or nullptr
	 */
	static const FMeshPlacementInfo* SelectWeightedMeshPlacement(const TArray<FMeshPlacementInfo>& MeshPool);
};

// ========================================================================
// TEMPLATE IMPLEMENTATIONS (Must be in header)
// ========================================================================

template<typename T>
const T* UDungeonGenerationHelpers::SelectWeightedRandom(
	const TArray<T>& Items,
	TFunction<float(const T&)> GetWeightFunc)
{
	if (Items.Num() == 0)
	{
		return nullptr;
	}

	// Calculate total weight
	float TotalWeight = 0.0f;
	for (const T& Item :  Items)
	{
		TotalWeight += GetWeightFunc(Item);
	}

	// If all weights are zero, select uniformly
	if (TotalWeight <= 0.0f)
	{
		int32 RandomIndex = FMath::RandRange(0, Items.Num() - 1);
		return &Items[RandomIndex];
	}

	// Weighted random selection
	float RandomValue = FMath::FRandRange(0.0f, TotalWeight);
	float CurrentWeight = 0.0f;

	for (const T& Item :  Items)
	{
		CurrentWeight += GetWeightFunc(Item);
		if (RandomValue <= CurrentWeight)
		{
			return &Item;
		}
	}

	// Fallback (should never reach here)
	return &Items. Last();
};
