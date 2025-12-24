// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DoorData.generated.h"



UCLASS()
class CLAUDEDUNGAI_API UDoorData : public UDataAsset
{
	GENERATED_BODY()

	public:
	// --- Door Frame Components (Static Geometry) ---
	
	// Door frame mesh (single mesh for complete frame)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Frame Geometry")
	TSoftObjectPtr<UStaticMesh> FrameSideMesh;
		
	// Footprint in cells (2 = 200cm, 4 = 400cm))
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Frame Geometry")
	int32 FrameFootprintY = 2;
	
	// Rotation offset for mesh alignment
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Frame Geometry")
	FRotator FrameRotationOffset = FRotator::ZeroRotator;
	
	// --- Door Variety Pool (Hybrid System) ---
	
	// Door variety pool (for multiple door styles)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door Varieties")
	TArray<UDoorData*> DoorStylePool;
	
	// --- Functional Door Actor ---

	// The actual Blueprint Class of the Door Actor (e.g., an actor that handles opening/closing/replication)
	//UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door Functionality")
	//TSubclassOf<ADoorway> DoorwayClass;

	// --- Connection Logic ---

	// Connection box extent (for hallway connections)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connection")
	FVector ConnectionBoxExtent = FVector(50.0f, 50.0f, 200.0f);
	
	// Placement weight for this door style (if multiple are available)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connection")
	float PlacementWeight = 1.0f;
};
