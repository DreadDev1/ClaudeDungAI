// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "DungeonSpawnerHelpers.generated.h"

/**
 * DungeonSpawnerHelpers - Static utility functions for mesh spawning
 * This library contains spawning-specific helper functions used across spawner systems.
 */
UCLASS()
class CLAUDEDUNGAI_API UDungeonSpawnerHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
  
	// INSTANCED STATIC MESH COMPONENT MANAGEMENT
	/**
	 * Get or create an ISM component for a specific mesh Creates new component if one doesn't exist for this mesh
	 * @param Owner - Actor that will own the component
	 * @param MeshAsset - Mesh to create ISM for
	 * @param ComponentMap - Map tracking mesh → ISM component associations
	 * @param ComponentNamePrefix - Prefix for component name (e.g., "FloorISM_", "WallISM_")
	 * @param bLogWarnings - Whether to log warnings on failure
	 * @return ISM component or nullptr if mesh failed to load */
	static UInstancedStaticMeshComponent* GetOrCreateISMComponent( AActor* Owner,const TSoftObjectPtr<UStaticMesh>& MeshAsset,
	TMap<TSoftObjectPtr<UStaticMesh>, UInstancedStaticMeshComponent*>& ComponentMap,const FString& ComponentNamePrefix,
	bool bLogWarnings = true);

	/*Clear all ISM components in a component map Destroys components and clears the map */
	static void ClearISMComponentMap(TMap<TSoftObjectPtr<UStaticMesh>, UInstancedStaticMeshComponent*>& ComponentMap);

  	// MESH INSTANCE SPAWNING
  	/** Spawn a mesh instance with local-to-world transform conversion
	 * @param ISMComponent - Component to add instance to
	 * @param LocalTransform - Transform in component/room space
	 * @param WorldOffset - World offset to apply (typically actor location)
	 * @return Index of spawned instance or -1 on failure */
	UFUNCTION(BlueprintCallable, Category = "Dungeon Spawner|Mesh")
	static int32 SpawnMeshInstance( UInstancedStaticMeshComponent* ISMComponent, const FTransform& LocalTransform,
	const FVector& WorldOffset);

	/** Spawn multiple mesh instances from an array
	 * @param ISMComponent - Component to add instances to
	 * @param LocalTransforms - Array of transforms in component/room space
	 * @param WorldOffset - World offset to apply (typically actor location)
	 * @return Number of instances successfully spawned */
	static int32 SpawnMeshInstances(UInstancedStaticMeshComponent* ISMComponent, const TArray<FTransform>& LocalTransforms,
	const FVector& WorldOffset);
  
	// TRANSFORM UTILITIES
	/** Convert local (component-space) transform to world transform
	 * @param LocalTransform - Transform relative to room/component origin
	 * @param WorldOffset - World offset to apply (typically actor location)
	 * @return World-space transform
	 */
	UFUNCTION(BlueprintPure, Category = "Dungeon Spawner|Transform")
	static FTransform LocalToWorldTransform(const FTransform& LocalTransform, const FVector& WorldOffset);

	/** Convert array of local transforms to world transforms
	 * @param LocalTransforms - Transforms relative to room/component origin
	 * @param WorldOffset - World offset to apply
	 * @return Array of world-space transforms */
	static TArray<FTransform> LocalToWorldTransforms(const TArray<FTransform>& LocalTransforms, const FVector& WorldOffset);
};