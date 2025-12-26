// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CeilingData.generated.h"

// Struct for ceiling tile definitions (supports different sizes for efficient coverage)
USTRUCT(BlueprintType)
struct FCeilingTile
{
	GENERATED_BODY()

	// The static mesh for this ceiling tile
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ceiling Tile")
	TSoftObjectPtr<UStaticMesh> Mesh;

	// Size of this tile in grid cells (1 = 100x100, 4 = 400x400)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ceiling Tile")
	int32 TileSize = 1;

	// Placement weight for weighted random selection (0.0 to 10.0)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ceiling Tile", meta=(ClampMin="0.0", ClampMax="10.0"))
	float PlacementWeight = 1.0f;
};

UCLASS()
class CLAUDEDUNGAI_API UCeilingData : public UDataAsset
{
	GENERATED_BODY()

public:
	// Large ceiling tiles (400x400) - used to fill majority of ceiling
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ceiling Tiles")
	TArray<FCeilingTile> LargeTilePool;

	// Medium ceiling tiles (200x200 = 2x2 cells)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ceiling Tiles")
	TArray<FCeilingTile> MediumTilePool;
	
	// Small ceiling tiles (100x100) - used to fill gaps and edges
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ceiling Tiles")
	TArray<FCeilingTile> SmallTilePool;

	// Height of the ceiling above the floor (Z offset)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ceiling Settings")
	float CeilingHeight = 500.0f;

	// Rotation offset for all ceiling tiles (0, 180, 0) to flip floor tiles upside down for ceiling
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ceiling Settings")
	FRotator CeilingRotation = FRotator(0.0f, 180.0f, 0.0f);
};
