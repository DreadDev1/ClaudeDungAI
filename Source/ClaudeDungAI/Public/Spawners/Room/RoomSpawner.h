// Fill out your copyright notice in the Description page of Project Settings. 

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Generators/Room/RoomGenerator.h"
#include "Utilities/Debugging/DebugHelpers.h"
#include "Data/Room/RoomData.h"
#include "RoomSpawner.generated.h"

class UWallData;
class UTextRenderComponent;
class UInstancedStaticMeshComponent;
/**
 * RoomSpawner - Actor responsible for spawning and visualizing rooms in the level
 * Holds RoomGenerator for logic and DebugHelpers for visualization
 * Provides CallInEditor functions for designer workflow
 */
UCLASS()
class CLAUDEDUNGAI_API ARoomSpawner : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ARoomSpawner();

#pragma region Debug Components
	// Debug visualization component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UDebugHelpers* DebugHelpers;
#pragma endregion 

#pragma region Room Generation Properties

	// Room configuration data asset
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room Configuration")
	URoomData* RoomData;
#pragma endregion

#pragma region Editor Functions
#if WITH_EDITOR
	/* Generate the room grid (visualization only at this stage)
	 * Creates empty grid and displays it with coordinates */
	UFUNCTION(CallInEditor, Category = "Room Generation")
	void GenerateRoomGrid();
	
	/* Clear the room grid and all visualizations */
	UFUNCTION(CallInEditor, Category = "Room Generation")
	void ClearRoomGrid();

	/** Generate floor meshes based on RoomData FloorData */
	UFUNCTION(CallInEditor, Category = "Room Generation")
	void GenerateFloorMeshes();

	/* Clear all spawned floor meshes */
	UFUNCTION(CallInEditor, Category = "Room Generation")
	void ClearFloorMeshes();
	
	/* Refresh visualization (useful after changing debug settings) */
	UFUNCTION(CallInEditor, Category = "Room Generation")
	void RefreshVisualization();
	
	/** Generate wall meshes based on RoomData WallData */
	UFUNCTION(CallInEditor, Category = "Room Generation")
	void GenerateWallMeshes();

	/* Clear all spawned wall meshes */
	UFUNCTION(CallInEditor, Category = "Room Generation")
	void ClearWallMeshes();
	
	/** Generate corner meshes for all 4 corners */
	UFUNCTION(CallInEditor, Category = "Room Generation")
	void GenerateCornerMeshes();

	/* Clear all spawned corner meshes */
	UFUNCTION(CallInEditor, Category = "Room Generation")
	void ClearCornerMeshes();

#pragma region Debug Functions
#pragma region Grid Coordinate Text Rendering
	/* Toggle coordinate display */
	UFUNCTION(CallInEditor, Category = "Room Generation|Debug")
	void ToggleCoordinates();
	
	/* Create a text render component at specified world location Called by DebugHelpers via delegate */
	UTextRenderComponent* CreateTextRenderComponent(FVector WorldPosition, FString Text, FColor Color, float Scale);

	/* Destroy a text render component Called by DebugHelpers via delegate */
	void DestroyTextRenderComponent(UTextRenderComponent* TextComp);
#pragma endregion
	/* Toggle grid outline display */
	UFUNCTION(CallInEditor, Category = "Room Generation|Debug")
	void ToggleGrid();

	/* Toggle cell state visualization */
	UFUNCTION(CallInEditor, Category = "Room Generation|Debug")
	void ToggleCellStates();
#pragma endregion
	
#endif
#pragma endregion
	
	/* Get the room generator instance  */
	URoomGenerator* GetRoomGenerator() const { return RoomGenerator; }

	/* Check if room is generated */
	bool IsRoomGenerated() const { return bIsGenerated; }

private:

	// Room generator instance (logic layer)
	UPROPERTY()
	URoomGenerator* RoomGenerator;

	
	// Flag to track if room is generated
	bool bIsGenerated;
	
	// Ensure RoomGenerator is created and initialized (lightweight)
	bool EnsureGeneratorReady();
	
	// Track spawned floor mesh instances
	TMap<TSoftObjectPtr<UStaticMesh>, UInstancedStaticMeshComponent*> FloorMeshComponents;

	// Track spawned wall mesh instances
	TMap<TSoftObjectPtr<UStaticMesh>, UInstancedStaticMeshComponent*> WallMeshComponents;
	
	// Track spawned corner mesh instances
	TMap<TSoftObjectPtr<UStaticMesh>, UInstancedStaticMeshComponent*> CornerMeshComponents;
	
	// Helper functions
	void SpawnWallSegment(const FPlacedWallInfo& PlacedWall, const FVector& RoomOrigin);
	
#pragma region Debug Functions
	/* Update visualization based on current grid state */
	void UpdateVisualization();

	/* Log room statistics to output */
	void LogRoomStatistics();

	/* Log floor statistics to output */
	void LogFloorStatistics();
#pragma endregion
};