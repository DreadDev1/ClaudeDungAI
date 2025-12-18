// Fill out your copyright notice in the Description page of Project Settings. 

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Generators/Room/RoomGenerator.h"
#include "Utilities/Debugging/DebugHelpers.h"
#include "Data/Room/RoomData.h"
#include "RoomSpawner.generated.h"

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

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// ============================================================================
	// COMPONENTS
	// ============================================================================

	// Debug visualization component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UDebugHelpers* DebugHelpers;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	// Room configuration data asset
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room Configuration")
	URoomData* RoomData;

	// ============================================================================
	// EDITOR FUNCTIONS (CallInEditor)
	// ============================================================================

#if WITH_EDITOR
	/**
	 * Generate the room grid (visualization only at this stage)
	 * Creates empty grid and displays it with coordinates
	 */
	UFUNCTION(CallInEditor, Category = "Room Generation")
	void GenerateRoomGrid();

	/**
	 * Clear the room grid and all visualizations
	 */
	UFUNCTION(CallInEditor, Category = "Room Generation")
	void ClearRoomGrid();

	/**
	 * Refresh visualization (useful after changing debug settings)
	 */
	UFUNCTION(CallInEditor, Category = "Room Generation")
	void RefreshVisualization();

	/**
	 * Toggle coordinate display
	 */
	UFUNCTION(CallInEditor, Category = "Room Generation|Debug")
	void ToggleCoordinates();

	/**
	 * Toggle grid outline display
	 */
	UFUNCTION(CallInEditor, Category = "Room Generation|Debug")
	void ToggleGrid();

	/**
	 * Toggle cell state visualization
	 */
	UFUNCTION(CallInEditor, Category = "Room Generation|Debug")
	void ToggleCellStates();
#endif

	// ============================================================================
	// PUBLIC ACCESSORS
	// ============================================================================

	/**
	 * Get the room generator instance
	 */
	URoomGenerator* GetRoomGenerator() const { return RoomGenerator; }

	/**
	 * Check if room is generated
	 */
	bool IsRoomGenerated() const { return bIsGenerated; }

private:
	// ============================================================================
	// INTERNAL DATA
	// ============================================================================

	// Room generator instance (logic layer)
	UPROPERTY()
	URoomGenerator* RoomGenerator;

	// Flag to track if room is generated
	bool bIsGenerated;

	// ============================================================================
	// INTERNAL HELPERS
	// ============================================================================

	/**
	 * Initialize the room generator
	 */
	bool InitializeGenerator();

	/**
	 * Update visualization based on current grid state
	 */
	void UpdateVisualization();

	/**
	 * Log room statistics to output
	 */
	void LogRoomStatistics();
};