// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/Grid/GridData.h"
#include "Engine/DataAsset.h"
#include "RoomData.generated.h"

struct FForcedEmptyRegion;
class UFloorData;
class UWallData;
class UDoorData;
class UCeilingData;


UCLASS()
class CLAUDEDUNGAI_API URoomData : public UDataAsset
{
	GENERATED_BODY()

public:
	// --- General Dungeon Layout Parameters ---
	// --- Style Data Asset References (Designer Swaps) ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Styles")
	TSoftObjectPtr<UFloorData> FloorStyleData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Styles")
	TSoftObjectPtr<UWallData> WallStyleData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room Properties")
	float WallThickness = 25.0f;

	// Wall thickness for rooms using this data (in cm)
	// Used for hallway alignment - ensures proper connection between rooms with different wall meshes
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Styles")
	TSoftObjectPtr<UDoorData> DoorStyleData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Styles")
	TSoftObjectPtr<UCeilingData> CeilingStyleData;
	
	// --- Interior Mesh Randomization Pool ---

	// Meshes used to fill the interior of the room grid (clutter, furniture, etc.)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interior Meshes")
	TArray<FMeshPlacementInfo> InteriorMeshPool;

	// --- Designer Override Controls ---

	// Array of rectangular regions the designer wants to force empty
	// Use this to create L-shapes, T-shapes, courtyards, or irregular room layouts
	UPROPERTY(EditAnywhere, Category = "Designer Overrides|Floor")
	TArray<FForcedEmptyRegion> ForcedEmptyRegions;
	
	// Array of specific 100cm cell coordinates the designer wants to force empty
	// Use this for one-off cells or fine-tuning after regions are defined
	UPROPERTY(EditAnywhere, Category = "Designer Overrides|Floor")
	TArray<FIntPoint> ForcedEmptyFloorCells;
	
	// Map of specific meshes to force-place at coordinates (Highest Priority)
	// Key = Grid coordinate (top-left cell), Value = Mesh to place
	UPROPERTY(EditAnywhere, Category = "Designer Overrides|Floor")
	TMap<FIntPoint, FMeshPlacementInfo> ForcedFloorPlacements;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Designer Overrides|Walls")
	TArray<FForcedWallPlacement> ForcedWallPlacements;
};
