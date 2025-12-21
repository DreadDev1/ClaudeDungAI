// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FloorData.generated.h"

struct FMeshPlacementInfo;

UCLASS()
class CLAUDEDUNGAI_API UFloorData : public UDataAsset
{
	GENERATED_BODY()

public:
	// Pool of floor tiles with placement weights and footprints
	// 
	// DESIGNER NOTE: Include tiles of ALL sizes in this pool: 
	//   - Large tiles (4x4, 2x4, 4x2) for main floor coverage
	//   - Medium tiles (2x2) for mid-sized gaps
	//   - Small tiles (1x2, 2x1) for narrow gaps
	//   - Filler tiles (1x1) for single-cell gaps
	//
	// The generator will automatically select appropriate sizes based on available space. 
	// Use PlacementWeight to control how often each tile appears.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Floor Tiles")
	TArray<FMeshPlacementInfo> FloorTilePool;

	// --- Floor Clutter / Detail Meshes ---
	
	// A separate pool for smaller details or clutter placed randomly on top of the main tiles.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Floor Clutter")
	TArray<FMeshPlacementInfo> ClutterMeshPool;

	// The likelihood (0.0 to 1.0) of attempting to place a clutter mesh in an empty floor cell.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Floor Clutter")
	float ClutterPlacementChance = 0.25f;

};
