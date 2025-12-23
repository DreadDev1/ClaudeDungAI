// Fill out your copyright notice in the Description page of Project Settings. 

#include "Utilities/Spawners/DungeonSpawnerHelpers.h"
#include "Utilities/Helpers/DungeonGenerationHelpers.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

 
// INSTANCED STATIC MESH COMPONENT MANAGEMENT
UInstancedStaticMeshComponent* UDungeonSpawnerHelpers::GetOrCreateISMComponent(AActor* Owner, const TSoftObjectPtr<UStaticMesh>& MeshAsset,
TMap<TSoftObjectPtr<UStaticMesh>, UInstancedStaticMeshComponent*>& ComponentMap,const FString& ComponentNamePrefix,bool bLogWarnings)
{
	if (!Owner)
	{
		if (bLogWarnings) UE_LOG(LogTemp, Warning, TEXT("GetOrCreateISMComponent: Owner is null"));
		return nullptr;
	}

	// Return null if mesh asset is not set
	if (MeshAsset.IsNull())
	{
		if (bLogWarnings) UE_LOG(LogTemp, Warning, TEXT("GetOrCreateISMComponent: MeshAsset is null"));
		return nullptr;
	}

	// Check if we already have an ISM component for this mesh
	if (ComponentMap.Contains(MeshAsset)) return ComponentMap[MeshAsset];

	// Load and validate mesh using generation helper
	UStaticMesh* StaticMesh = UDungeonGenerationHelpers:: LoadAndValidateMesh(
	MeshAsset, ComponentNamePrefix,bLogWarnings);

	if (!StaticMesh) return nullptr;

	// Create new ISM component
	FString ComponentName = FString::Printf(TEXT("%s%s"), *ComponentNamePrefix, *MeshAsset. GetAssetName());
	
	UInstancedStaticMeshComponent* NewISM = NewObject<UInstancedStaticMeshComponent>(Owner, FName(*ComponentName));

	if (!NewISM)
	{
		if (bLogWarnings) UE_LOG(LogTemp, Warning, TEXT("GetOrCreateISMComponent: Failed to create component '%s'"), *ComponentName);
		return nullptr;
	}

	// Register and attach to root
	NewISM->RegisterComponent();
	NewISM->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

	// Set mesh
	NewISM->SetStaticMesh(StaticMesh);

	// Store component for reuse
	ComponentMap. Add(MeshAsset, NewISM);
	
	return NewISM;
}

void UDungeonSpawnerHelpers::ClearISMComponentMap(TMap<TSoftObjectPtr<UStaticMesh>, UInstancedStaticMeshComponent*>& ComponentMap)
{
	// Destroy all components
	for (auto& Pair : ComponentMap)
	{
		if (Pair.Value && Pair.Value->IsValidLowLevel())
		{
			Pair.Value->ClearInstances();
			Pair.Value->DestroyComponent();
		}
	}

	// Clear the map
	ComponentMap.Empty();
}

// MESH INSTANCE SPAWNING

int32 UDungeonSpawnerHelpers::SpawnMeshInstance( UInstancedStaticMeshComponent* ISMComponent,const FTransform& LocalTransform,
const FVector& WorldOffset)
{
	if (!ISMComponent) return -1;
	// Convert local transform to world transform
	FTransform WorldTransform = LocalToWorldTransform(LocalTransform, WorldOffset);

	// Add instance
	return ISMComponent->AddInstance(WorldTransform);
}

int32 UDungeonSpawnerHelpers::SpawnMeshInstances(UInstancedStaticMeshComponent* ISMComponent, const TArray<FTransform>& LocalTransforms,
const FVector& WorldOffset)
{
	if (!ISMComponent) return 0;
	int32 SpawnedCount = 0;

	for (const FTransform& LocalTransform : LocalTransforms)
	{
		int32 InstanceIndex = SpawnMeshInstance(ISMComponent, LocalTransform, WorldOffset);
		if (InstanceIndex >= 0) SpawnedCount++;
	}

	return SpawnedCount;
}
  
// TRANSFORM UTILITIES
FTransform UDungeonSpawnerHelpers::LocalToWorldTransform(const FTransform& LocalTransform, const FVector& WorldOffset)
{
	FTransform WorldTransform = LocalTransform;
	WorldTransform.SetLocation(WorldOffset + LocalTransform.GetLocation());
	return WorldTransform;
}

TArray<FTransform> UDungeonSpawnerHelpers::LocalToWorldTransforms(
const TArray<FTransform>& LocalTransforms, const FVector& WorldOffset)
{
	TArray<FTransform> WorldTransforms;
	WorldTransforms.Reserve(LocalTransforms.Num());

	for (const FTransform& LocalTransform : LocalTransforms)
	{
		WorldTransforms. Add(LocalToWorldTransform(LocalTransform, WorldOffset));
	}

	return WorldTransforms;
}