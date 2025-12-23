// Fill out your copyright notice in the Description page of Project Settings.

#include "Spawners/Room/RoomSpawner.h"
#include "Generators/Room/RoomGenerator.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Data/Room/WallData.h" 
#include "Utilities/Helpers/DungeonGenerationHelpers.h"
#include "Utilities/Spawners/DungeonSpawnerHelpers.h" 

// Sets default values
ARoomSpawner::ARoomSpawner()
{
 	// Set this actor to call Tick() every frame (can turn off for performance)
	PrimaryActorTick.bCanEverTick = false;

	// Create debug helpers component
	DebugHelpers = CreateDefaultSubobject<UDebugHelpers>(TEXT("DebugHelpers"));

	// NEW: Bind delegate so DebugHelpers can request text components
	DebugHelpers->OnCreateTextComponent.BindUObject(this, &ARoomSpawner::CreateTextRenderComponent);

	// ✅ NEW: Bind destruction delegate
	DebugHelpers->OnDestroyTextComponent. BindUObject(this, &ARoomSpawner::DestroyTextRenderComponent);

	// Initialize flags
	bIsGenerated = false;
}


#if WITH_EDITOR
#pragma region In Editor Functions
void ARoomSpawner::GenerateRoomGrid()
{
	DebugHelpers->LogSectionHeader(TEXT("GENERATE ROOM GRID"));

	// Validate RoomData
	if (!RoomData)
	{
		DebugHelpers->LogCritical(TEXT("RoomData is not assigned!  Please assign a RoomData asset.")); return;
	}

	// Initialize generator
	if (!InitializeGenerator())
	{
		DebugHelpers->LogCritical(TEXT("Failed to initialize RoomGenerator!")); return;
	}

	// Create the grid
	RoomGenerator->CreateGrid();
	bIsGenerated = true;

	// Log statistics
	LogRoomStatistics();

	// Update visualization
	UpdateVisualization();

	DebugHelpers->LogImportant(TEXT("Room grid generated successfully!"));
	// End header
	DebugHelpers->LogSectionHeader(TEXT("GENERATE ROOM GRID"));
}

void ARoomSpawner::ClearRoomGrid()
{
	DebugHelpers->LogSectionHeader(TEXT("CLEAR ROOM GRID"));

	if (! RoomGenerator || !bIsGenerated)
	{
		DebugHelpers->LogImportant(TEXT("No room grid to clear. ")); return;
	}
	
	// Clear floor meshes first
	ClearFloorMeshes();

	// Clear the grid
	RoomGenerator->ClearGrid();
	bIsGenerated = false;
	
	// Clear coordinate text components (DebugHelpers manages them)
	DebugHelpers->ClearCoordinateTextComponents();
	
	// Clear debug drawings
	DebugHelpers->ClearDebugDrawings();

	DebugHelpers->LogImportant(TEXT("Room grid cleared. "));
}

void ARoomSpawner::GenerateFloorMeshes()
{
	DebugHelpers->LogSectionHeader(TEXT("GENERATE FLOOR MESHES"));
	
	// VALIDATION: Check RoomData
	if (!RoomData)
	{
		DebugHelpers->LogCritical(TEXT("RoomData is not assigned!  Cannot generate floor meshes."));
		DebugHelpers->LogSectionHeader(TEXT("GENERATE FLOOR MESHES"));
		return;
	}
	
	// AUTO-GENERATION: Create grid if needed
	if (! RoomGenerator || !bIsGenerated)
	{
		DebugHelpers->LogImportant(TEXT("Grid not found.  Auto-generating grid..."));
		GenerateRoomGrid();

		if (!RoomGenerator || !bIsGenerated)
		{
			DebugHelpers->LogCritical(TEXT("Auto-generation of grid failed!"));
			DebugHelpers->LogSectionHeader(TEXT("GENERATE FLOOR MESHES"));
			return;
		}
	}
	
	// CLEANUP: Clear existing floor meshes
	ClearFloorMeshes();
	
	// GENERATE FLOOR LAYOUT (This was missing!)
	DebugHelpers->LogImportant(TEXT("Generating floor layout..."));
	if (! RoomGenerator->GenerateFloor())
	{
		DebugHelpers->LogCritical(TEXT("Floor generation failed!"));
		DebugHelpers->LogSectionHeader(TEXT("GENERATE FLOOR MESHES"));
		return;
	}
	
	// SPAWNING: Get placed meshes from generator
	const TArray<FPlacedMeshInfo>& PlacedMeshes = RoomGenerator->GetPlacedFloorMeshes();
	DebugHelpers->LogImportant(FString::Printf(TEXT("Spawning %d floor mesh instances... "), PlacedMeshes.Num()));

	// Get room origin for world space conversion
	FVector RoomOrigin = GetActorLocation();
	   
	// SPAWNING: Create ISM components and add instances
	for (const FPlacedMeshInfo& PlacedMesh : PlacedMeshes)
	{
		// Get or create ISM component for this mesh
		UInstancedStaticMeshComponent* ISM = UDungeonSpawnerHelpers::GetOrCreateISMComponent(this,
		PlacedMesh.MeshInfo.MeshAsset,FloorMeshComponents,TEXT("FloorISM_"),true);

		if (ISM)
		{
			int32 InstanceIndex = UDungeonSpawnerHelpers:: SpawnMeshInstance( ISM, PlacedMesh.WorldTransform, RoomOrigin);

			if (InstanceIndex >= 0)
			{
				DebugHelpers->LogVerbose(FString::Printf( TEXT("  Spawned floor mesh at grid position (%d, %d), instance %d"),
				PlacedMesh.GridPosition.X, PlacedMesh. GridPosition.Y, InstanceIndex));
			}
			else
			{
				DebugHelpers->LogVerbose(FString::Printf(TEXT("  Failed to spawn floor mesh at grid position (%d, %d)"),
				PlacedMesh.GridPosition.X, PlacedMesh.GridPosition.Y));
			}
		}
	}

	DebugHelpers->LogImportant(FString::Printf(TEXT("Floor meshes generated: %d instances across %d unique meshes"),
	PlacedMeshes.Num(),	FloorMeshComponents.Num()));
	DebugHelpers->LogSectionHeader(TEXT("GENERATE FLOOR MESHES"));
}

void ARoomSpawner::ClearFloorMeshes()
{
	// Clear all floor ISM components
	UDungeonSpawnerHelpers:: ClearISMComponentMap(FloorMeshComponents);

	// Clear generator data
	if (RoomGenerator) RoomGenerator->ClearPlacedFloorMeshes();
	DebugHelpers->LogImportant(TEXT("Floor meshes cleared"));
}

void ARoomSpawner::SpawnFloorMesh(const FPlacedMeshInfo& PlacedMesh, const FVector& RoomOrigin)
{
	// Validate mesh asset
	if (PlacedMesh.MeshInfo.MeshAsset.IsNull())
	{
		DebugHelpers->LogVerbose(TEXT("Skipping mesh with null asset")); return;
	}

	// Get or create instanced static mesh component for this mesh type
	UInstancedStaticMeshComponent* ISMComponent = nullptr;

	if (FloorMeshComponents. Contains(PlacedMesh. MeshInfo.MeshAsset))
	{
		ISMComponent = FloorMeshComponents[PlacedMesh.MeshInfo.MeshAsset];
	}
	else
	{
		// Create new ISM component
		ISMComponent = NewObject<UInstancedStaticMeshComponent>(this);
		ISMComponent->RegisterComponent();
		ISMComponent->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

		// Load and set static mesh
		UStaticMesh* StaticMesh = PlacedMesh.MeshInfo.MeshAsset.LoadSynchronous();
		if (StaticMesh) ISMComponent->SetStaticMesh(StaticMesh);

		// Store component
		FloorMeshComponents.Add(PlacedMesh.MeshInfo. MeshAsset, ISMComponent);
	}

	if (!ISMComponent)
	{
		DebugHelpers->LogVerbose(TEXT("Failed to create ISM component")); return;
	}

	// Use the WorldTransform we calculated in TryPlaceMesh
	FVector LocalPos = PlacedMesh.WorldTransform. GetLocation();
	FVector WorldPos = RoomOrigin + LocalPos;

	// DEBUG: Log the spawn position
	UE_LOG(LogTemp, Warning, TEXT("SpawnFloorMesh: Grid(%d,%d) Size(%d,%d) LocalPos(%s) RoomOrigin(%s) -> WorldPos(%s)"),
	PlacedMesh.GridPosition.X, PlacedMesh.GridPosition.Y, PlacedMesh. Size.X, PlacedMesh.Size.Y,
	*LocalPos.ToString(), *RoomOrigin.ToString(), *WorldPos.ToString());

	FTransform InstanceTransform( FRotator(0, PlacedMesh.Rotation, 0), WorldPos,FVector::OneVector);

	// Add instance
	ISMComponent->AddInstance(InstanceTransform);
}

void ARoomSpawner::GenerateWallMeshes()
{
	DebugHelpers->LogSectionHeader(TEXT("GENERATE WALL MESHES"));

	// Validate RoomData
	if (!RoomData)
	{
		DebugHelpers->LogCritical(TEXT("RoomData is not assigned!  Cannot generate wall meshes. "));
		DebugHelpers->LogSectionHeader(TEXT("GENERATE WALL MESHES"));
		return;
	}

	if (!RoomData->WallStyleData.IsValid())
	{
		DebugHelpers->LogCritical(TEXT("WallStyleData is not assigned in RoomData!  "));
		DebugHelpers->LogSectionHeader(TEXT("GENERATE WALL MESHES"));
		return;
	}

	// Auto-generate grid if needed
	if (!  RoomGenerator || ! bIsGenerated)
	{
		DebugHelpers->LogImportant(TEXT("Grid not found.   Auto-generating grid..."));
		GenerateRoomGrid();

		if (!RoomGenerator || !  bIsGenerated)
		{
			DebugHelpers->LogCritical(TEXT("Auto-generation of grid failed!"));
			DebugHelpers->LogSectionHeader(TEXT("GENERATE WALL MESHES"));
			return;
		}
	}

	// Clear existing wall meshes
	ClearWallMeshes();

	// Generate wall layout (logic only)
	DebugHelpers->LogImportant(TEXT("Generating wall layout..."));
	if (! RoomGenerator->GenerateWalls())
	{
		DebugHelpers->LogCritical(TEXT("Wall generation failed!"));
		DebugHelpers->LogSectionHeader(TEXT("GENERATE WALL MESHES"));
		return;
	}

	// Get placed walls from generator
	const TArray<FPlacedWallInfo>& PlacedWalls = RoomGenerator->GetPlacedWalls();
	DebugHelpers->LogImportant(FString::Printf(TEXT("Spawning %d wall segments...  "), PlacedWalls.Num()));

	// ✅ ADD THIS LINE - Get room origin for world space conversion
	FVector RoomOrigin = GetActorLocation();

	// Spawn walls in world
	for (const FPlacedWallInfo& PlacedWall : PlacedWalls) { SpawnWallSegment(PlacedWall, RoomOrigin);}

	DebugHelpers->LogImportant(TEXT("Wall meshes generated successfully!"));
	DebugHelpers->LogSectionHeader(TEXT("GENERATE WALL MESHES"));
}

void ARoomSpawner::ClearWallMeshes()
{
	// Clear all wall ISM components
	UDungeonSpawnerHelpers::ClearISMComponentMap(WallMeshComponents);

	// Clear generator data
	if (RoomGenerator) { RoomGenerator->ClearPlacedWalls();	}
	DebugHelpers->LogImportant(TEXT("Wall meshes cleared"));
}

void ARoomSpawner::RefreshVisualization()
{
	DebugHelpers->LogImportant(TEXT("Refreshing visualization..."));

	if (!bIsGenerated || !RoomGenerator)
	{
		DebugHelpers->LogImportant(TEXT("No room to visualize.  Generate a room first.")); return;
	}

	// Clear existing drawings
	DebugHelpers->ClearDebugDrawings();

	// Redraw with current settings
	UpdateVisualization();

	DebugHelpers->LogImportant(TEXT("Visualization refreshed. "));
}

void ARoomSpawner::SpawnWallSegment(const FPlacedWallInfo& PlacedWall, const FVector& RoomOrigin)
{
	// SPAWN BOTTOM MESH (Required - Base Layer)
	UInstancedStaticMeshComponent* BottomISM = UDungeonSpawnerHelpers::GetOrCreateISMComponent(this,
	PlacedWall.WallModule.BaseMesh,WallMeshComponents,TEXT("WallISM_"), true);

	if (BottomISM)
	{
		// Wall uses BottomTransform (check your FPlacedWallInfo struct)
		int32 InstanceIndex = UDungeonSpawnerHelpers::SpawnMeshInstance( BottomISM, PlacedWall.BottomTransform, RoomOrigin);

		if (InstanceIndex >= 0)
		{
			DebugHelpers->LogVerbose(FString::Printf(TEXT("  Spawned base mesh at edge %d, cell %d (instance %d)"), 
			(int32)PlacedWall.Edge, PlacedWall.StartCell, InstanceIndex));
		}
	}
	// SPAWN MIDDLE1 MESH (Optional - First Middle Layer)
	if (! PlacedWall.WallModule.MiddleMesh1. IsNull())
	{
		UInstancedStaticMeshComponent* Middle1ISM = UDungeonSpawnerHelpers::GetOrCreateISMComponent( this,
		PlacedWall.WallModule.MiddleMesh1, WallMeshComponents, TEXT("WallISM_"), true);

		if (Middle1ISM)
		{
			int32 InstanceIndex = UDungeonSpawnerHelpers:: SpawnMeshInstance( Middle1ISM, PlacedWall.Middle1Transform, RoomOrigin);

			if (InstanceIndex >= 0)
			{
				DebugHelpers->LogVerbose(FString::Printf( TEXT("  Spawned middle1 mesh at edge %d, cell %d (instance %d)"), 
				(int32)PlacedWall.Edge, PlacedWall.StartCell, InstanceIndex));
			}
		}
	}

	// SPAWN MIDDLE2 MESH (Optional - Second Middle Layer)
	if (!PlacedWall.WallModule. MiddleMesh2.IsNull())
	{
		UInstancedStaticMeshComponent* Middle2ISM = UDungeonSpawnerHelpers::GetOrCreateISMComponent( this,
		PlacedWall.WallModule.MiddleMesh2, WallMeshComponents, TEXT("WallISM_"), true);

		if (Middle2ISM)
		{
			int32 InstanceIndex = UDungeonSpawnerHelpers::SpawnMeshInstance( Middle2ISM, PlacedWall.Middle2Transform, RoomOrigin);

			if (InstanceIndex >= 0)
			{
				DebugHelpers->LogVerbose(FString::Printf(TEXT("  Spawned middle2 mesh at edge %d, cell %d (instance %d)"), 
				(int32)PlacedWall.Edge, PlacedWall. StartCell, InstanceIndex));
			}
		}
	}
	
	// SPAWN TOP MESH (Optional - Top Layer/Cap)
	if (!PlacedWall.WallModule.TopMesh.IsNull())
	{
		UInstancedStaticMeshComponent* TopISM = UDungeonSpawnerHelpers:: GetOrCreateISMComponent( this,
		PlacedWall.WallModule.TopMesh, WallMeshComponents, TEXT("WallISM_"), true);

		if (TopISM)
		{
			int32 InstanceIndex = UDungeonSpawnerHelpers::SpawnMeshInstance( TopISM, PlacedWall.TopTransform, RoomOrigin);

			if (InstanceIndex >= 0)
			{
				DebugHelpers->LogVerbose(FString::Printf(TEXT("  Spawned top mesh at edge %d, cell %d (instance %d)"), 
				(int32)PlacedWall.Edge, PlacedWall.StartCell, InstanceIndex));
			}
		}
	}
}
#pragma endregion

#pragma region Debug Functions
void ARoomSpawner::ToggleCoordinates()
{
	DebugHelpers->bShowCoordinates = !DebugHelpers->bShowCoordinates;
	DebugHelpers->LogImportant(FString::Printf(TEXT("Coordinates display: %s"), 
		DebugHelpers->bShowCoordinates ? TEXT("ON") : TEXT("OFF")));
	
	RefreshVisualization();
}

void ARoomSpawner:: ToggleGrid()
{
	DebugHelpers->bShowGrid = !DebugHelpers->bShowGrid;
	DebugHelpers->LogImportant(FString::Printf(TEXT("Grid outline display: %s"), 
		DebugHelpers->bShowGrid ? TEXT("ON") : TEXT("OFF")));
	
	RefreshVisualization();
}

void ARoomSpawner:: ToggleCellStates()
{
	DebugHelpers->bShowCellStates = !DebugHelpers->bShowCellStates;
	DebugHelpers->LogImportant(FString::Printf(TEXT("Cell states display: %s"), 
		DebugHelpers->bShowCellStates ? TEXT("ON") : TEXT("OFF")));
	
	RefreshVisualization();
}

UTextRenderComponent* ARoomSpawner::CreateTextRenderComponent(FVector WorldPosition, FString Text, FColor Color, float Scale)
{
	// Create new text render component
	UTextRenderComponent* TextComp = NewObject<UTextRenderComponent>(this);
	
	if (! TextComp)
	{
		return nullptr;
	}

	// Register and attach component
	TextComp->RegisterComponent();
	TextComp->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);

	// Set text properties
	TextComp->SetText(FText::FromString(Text));
	TextComp->SetWorldSize(Scale * 10.0f); // Scale for visibility
	TextComp->SetTextRenderColor(Color);
	TextComp->SetHorizontalAlignment(EHTA_Center);
	TextComp->SetVerticalAlignment(EVRTA_TextCenter);
	
	// Set world location
	TextComp->SetWorldLocation(WorldPosition);
	
	// Rotate to face upward (readable from above in editor)
	TextComp->SetWorldRotation(FRotator(45.0f, 180.0f, 0.0f));

	// Make visible in editor
	TextComp->SetVisibility(true);
	TextComp->SetHiddenInGame(false); // Show in PIE too

	// ✅ Track component (optional, for safety)
	CoordinateTextComponents.Add(TextComp);
	
	return TextComp;
}

void ARoomSpawner::DestroyTextRenderComponent(UTextRenderComponent* TextComp)
{
	if (!TextComp || !TextComp->IsValidLowLevel())
	{
		return;
	}

	// Remove from tracking array
	CoordinateTextComponents. Remove(TextComp);

	// Destroy the component
	TextComp->DestroyComponent();
}

void ARoomSpawner::LogRoomStatistics()
{
	if (!RoomGenerator)
	{
		return;
	}

	DebugHelpers->LogSectionHeader(TEXT("ROOM STATISTICS"));
	
	FIntPoint GridSize = RoomGenerator->GetGridSize();
	int32 TotalCells = RoomGenerator->GetTotalCellCount();
	int32 EmptyCells = RoomGenerator->GetCellCountByType(EGridCellType::ECT_Empty);
	int32 OccupiedCells = RoomGenerator->GetCellCountByType(EGridCellType::ECT_FloorMesh);
	float OccupancyPercent = RoomGenerator->GetOccupancyPercentage();

	DebugHelpers->LogStatistic(TEXT("Grid Size"), FString::Printf(TEXT("%d x %d"), GridSize.X, GridSize.Y));
	DebugHelpers->LogStatistic(TEXT("Total Cells"), TotalCells);
	DebugHelpers->LogStatistic(TEXT("Empty Cells"), EmptyCells);
	DebugHelpers->LogStatistic(TEXT("Occupied Cells"), OccupiedCells);
	DebugHelpers->LogStatistic(TEXT("Occupancy"), OccupancyPercent);
	
	// End header (NEW)
	DebugHelpers->LogSectionHeader(TEXT("ROOM STATISTICS"));
}

void ARoomSpawner::LogFloorStatistics()
{
	if (!RoomGenerator)
	{
		return;
	}

	DebugHelpers->LogSectionHeader(TEXT("FLOOR STATISTICS"));

	int32 LargeTiles, MediumTiles, SmallTiles, FillerTiles;
	RoomGenerator->GetFloorStatistics(LargeTiles, MediumTiles, SmallTiles, FillerTiles);

	int32 TotalTiles = LargeTiles + MediumTiles + SmallTiles + FillerTiles;
	float Coverage = RoomGenerator->GetOccupancyPercentage();
	int32 EmptyCells = RoomGenerator->GetCellCountByType(EGridCellType:: ECT_Empty);

	DebugHelpers->LogStatistic(TEXT("Large Tiles (400x400)"), LargeTiles);
	DebugHelpers->LogStatistic(TEXT("Medium Tiles (200x200)"), MediumTiles);
	DebugHelpers->LogStatistic(TEXT("Small Tiles (100x)"), SmallTiles);
	DebugHelpers->LogStatistic(TEXT("Filler Tiles"), FillerTiles);
	DebugHelpers->LogStatistic(TEXT("Total Tiles Placed"), TotalTiles);
	DebugHelpers->LogStatistic(TEXT("Floor Coverage"), Coverage);
	DebugHelpers->LogStatistic(TEXT("Empty Cells Remaining"), EmptyCells);

	DebugHelpers->LogSectionHeader(TEXT("FLOOR STATISTICS"));
}
#pragma endregion
#endif // WITH_EDITOR

bool ARoomSpawner::InitializeGenerator()
{
	// Create generator if it doesn't exist
	if (!RoomGenerator)
	{
		RoomGenerator = NewObject<URoomGenerator>(this, URoomGenerator::StaticClass());
	}

	// Initialize with RoomData
	return RoomGenerator->Initialize(RoomData);
}

void ARoomSpawner::UpdateVisualization()
{
	if (!RoomGenerator) return;

	// Get grid data
	FVector RoomOrigin = GetActorLocation();
	FIntPoint GridSize = RoomGenerator->GetGridSize();
	float CellSize = RoomGenerator->GetCellSize();
	const TArray<EGridCellType>& GridState = RoomGenerator->GetGridState();
	DebugHelpers->DrawGrid(GridSize, GridState, CellSize, RoomOrigin);

	// Draw forced empty regions (if any)
	if (RoomData && RoomData->ForcedEmptyRegions.Num() > 0)
	{
		DebugHelpers->DrawForcedEmptyRegions(RoomData->ForcedEmptyRegions, GridSize, CellSize, RoomOrigin);
	}

	// Draw forced empty cells (if any)
	if (RoomData && RoomData->ForcedEmptyFloorCells.Num() > 0)
	{
		DebugHelpers->DrawForcedEmptyCells(RoomData->ForcedEmptyFloorCells, GridSize, CellSize, RoomOrigin);
	}
	DebugHelpers->LogVerbose(TEXT("Visualization updated."));
}
