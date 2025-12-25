// Fill out your copyright notice in the Description page of Project Settings.

#include "Spawners/Room/RoomSpawner.h"
#include "Generators/Room/RoomGenerator.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Data/Room/DoorData.h" 
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
bool ARoomSpawner::EnsureGeneratorReady()
{
	// Validate RoomData
	if (!RoomData) 
	{ DebugHelpers->LogCritical(TEXT("RoomData is not assigned!")); return false;}

	if (RoomGridSize. X < 4 || RoomGridSize.Y < 4)
	{ DebugHelpers->LogCritical(TEXT("GridSize is too small (min 5x5)!")); return false;}
	
	// Create RoomGenerator if needed
	if (!RoomGenerator)
	{
		DebugHelpers->LogVerbose(TEXT("Creating RoomGenerator..."));
		RoomGenerator = NewObject<URoomGenerator>(this, TEXT("RoomGenerator"));
		if (!RoomGenerator)
		{ DebugHelpers->LogCritical(TEXT("Failed to create RoomGenerator! ")); return false; }
	}

	// Initialize if needed
	if (!RoomGenerator->IsInitialized())
	{
		DebugHelpers->LogVerbose(TEXT("Initializing RoomGenerator..."));
		if (!RoomGenerator->Initialize(RoomData, RoomGridSize))
		{ DebugHelpers->LogCritical(TEXT("Failed to initialize RoomGenerator!")); return false;}

		DebugHelpers->LogVerbose(TEXT("Creating grid cells..."));
		RoomGenerator->CreateGrid();
	}
	return true;
}

#if WITH_EDITOR
#pragma region In Editor Functions

#pragma region Floor Generation
void ARoomSpawner::GenerateRoomGrid()
{
	DebugHelpers->LogSectionHeader(TEXT("GENERATE ROOM GRID"));
	
	if (!EnsureGeneratorReady())
	{
		DebugHelpers->LogCritical(TEXT("Failed to initialize generator!"));
		DebugHelpers->LogSectionHeader(TEXT("GENERATE ROOM GRID"));
		return;
	}
	
	DebugHelpers->bShowGrid = true;
	DebugHelpers->bShowCellStates = true;
	DebugHelpers->bShowCoordinates = true;
	DebugHelpers->bShowForcedEmptyRegions = true;
	DebugHelpers->bShowForcedEmptyCells = true;
	
	// CREATE DEBUG VISUALIZATION (heavy, debug-only)
	DebugHelpers->LogImportant(TEXT("Creating debug visualization... "));
	
	UpdateVisualization();
	
	// Set flag for external checks
	bIsGenerated = true;
	
	LogRoomStatistics();
	
	DebugHelpers->LogImportant(TEXT("Room grid generated successfully!"));
	DebugHelpers->LogSectionHeader(TEXT("GENERATE ROOM GRID"));
}

void ARoomSpawner::ClearRoomGrid()
{
	DebugHelpers->LogSectionHeader(TEXT("CLEAR ROOM GRID"));

	if (! RoomGenerator || !bIsGenerated)
	{
		DebugHelpers->LogImportant(TEXT("No room grid to clear. "));
		DebugHelpers->LogSectionHeader(TEXT("CLEAR ROOM GRID"));
		return;
	}
	
	DebugHelpers->bShowGrid = false;
	DebugHelpers->bShowCellStates = false;
	DebugHelpers->bShowCoordinates = false;
	DebugHelpers->bShowForcedEmptyRegions = false;
	DebugHelpers->bShowForcedEmptyCells = false;
	
	// Clear floor meshes first
	ClearFloorMeshes();
	
	// Clear wall meshes
	ClearWallMeshes();
	
	// Clear corner meshes
	ClearCornerMeshes();
	
	//Clear doorway meshes
	ClearDoorwayMeshes();
	
	// ✅ NEW: Clear doorway LAYOUT (not just meshes)
	if (RoomGenerator)
	{
		RoomGenerator->ClearPlacedDoorways();
	}

	// Clear the grid
	RoomGenerator->ClearGrid();
	bIsGenerated = false;
	
	// Clear coordinate text components (DebugHelpers manages them)
	DebugHelpers->ClearCoordinateTextComponents();
	
	// Clear debug drawings
	DebugHelpers->ClearDebugDrawings();

	DebugHelpers->LogImportant(TEXT("Room grid cleared. "));
	DebugHelpers->LogSectionHeader(TEXT("CLEAR ROOM GRID"));
}

void ARoomSpawner::GenerateFloorMeshes()
{
	DebugHelpers->LogSectionHeader(TEXT("GENERATE FLOOR MESHES"));
	
	if (!EnsureGeneratorReady())
	{
		DebugHelpers->LogCritical(TEXT("Failed to initialize generator!"));
		DebugHelpers->LogSectionHeader(TEXT("GENERATE FLOOR MESHES"));
		return;
	}
	
	// CLEANUP: Clear existing floor meshes
	ClearFloorMeshes();
	
	// Generate Floor Layout
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
	PlacedMeshes.Num(),	FloorMeshComponents.Num())); DebugHelpers->LogSectionHeader(TEXT("GENERATE FLOOR MESHES"));
}

void ARoomSpawner::ClearFloorMeshes()
{
	// Clear all floor ISM components
	UDungeonSpawnerHelpers:: ClearISMComponentMap(FloorMeshComponents);

	// Clear generator data AND reset grid state
	if (RoomGenerator)
	{
		// Clear placed mesh array
		RoomGenerator->ClearPlacedFloorMeshes();
		
		// Reset grid cells back to empty for fresh generation
		RoomGenerator->ResetGridCellStates();
	}
	DebugHelpers->LogImportant(TEXT("Floor meshes cleared"));
}
#pragma endregion

#pragma region Wall Generation
void ARoomSpawner::GenerateWallMeshes()
{
	DebugHelpers->LogSectionHeader(TEXT("GENERATE WALL MESHES"));

	if (!EnsureGeneratorReady())
	{
		DebugHelpers->LogCritical(TEXT("Failed to initialize generator!"));
		DebugHelpers->LogSectionHeader(TEXT("GENERATE FLOOR MESHES"));
		return;
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
	
	// Get placed walls and spawn
	const TArray<FPlacedWallInfo>& PlacedWalls = RoomGenerator->GetPlacedWalls();
	DebugHelpers->LogImportant(FString::Printf(TEXT("Spawning %d wall segments...  "), PlacedWalls.Num()));
	
	// Get room origin for world space conversion
	FVector RoomOrigin = GetActorLocation();
	
	// Spawn wall segments
	for (const FPlacedWallInfo& PlacedWall : PlacedWalls) { SpawnWallSegment(PlacedWall, RoomOrigin);}
	
	DebugHelpers->LogImportant(TEXT("Wall meshes generated successfully!"));
	DebugHelpers->LogSectionHeader(TEXT("GENERATE WALL MESHES"));
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

void ARoomSpawner::ClearWallMeshes()
{
	// Clear all wall ISM components
	UDungeonSpawnerHelpers::ClearISMComponentMap(WallMeshComponents);

	// Clear generator data
	if (RoomGenerator) { RoomGenerator->ClearPlacedWalls();	}
	DebugHelpers->LogImportant(TEXT("Wall meshes cleared"));
}
#pragma endregion

#pragma region Corner Generation
void ARoomSpawner::GenerateCornerMeshes()
{
	 DebugHelpers->LogSectionHeader(TEXT("GENERATE CORNER MESHES"));

    if (!EnsureGeneratorReady())
    {
        DebugHelpers->LogCritical(TEXT("Failed to initialize generator!"));
        DebugHelpers->LogSectionHeader(TEXT("GENERATE CORNER MESHES"));
        return;
    }

    // Clear existing corners
    ClearCornerMeshes();

    // Generate corner layout (logic only)
    DebugHelpers->LogImportant(TEXT("Generating corner layout..."));
    if (!RoomGenerator->GenerateCorners())
    {
        DebugHelpers->LogCritical(TEXT("Corner generation failed!"));
        DebugHelpers->LogSectionHeader(TEXT("GENERATE CORNER MESHES"));
        return;
    }

    // Get placed corners and spawn
    const TArray<FPlacedCornerInfo>& PlacedCorners = RoomGenerator->GetPlacedCorners();
    
    if (PlacedCorners.Num() == 0)
    {
        DebugHelpers->LogImportant(TEXT("No corners to spawn (no corner mesh assigned)"));
        DebugHelpers->LogSectionHeader(TEXT("GENERATE CORNER MESHES"));
        return;
    }

    DebugHelpers->LogImportant(FString::Printf(TEXT("Spawning %d corner pieces..."), PlacedCorners.Num()));

    // Get room origin for world space conversion
    FVector RoomOrigin = GetActorLocation();

    // Spawn corner meshes
    for (const FPlacedCornerInfo& PlacedCorner : PlacedCorners)
    {
        // Get or create ISM component for corner mesh
        UInstancedStaticMeshComponent* ISM = UDungeonSpawnerHelpers::GetOrCreateISMComponent(
            this,
            PlacedCorner. CornerMesh,
            CornerMeshComponents,
            TEXT("CornerISM_"),
            true
        );

        if (ISM)
        {
            int32 InstanceIndex = UDungeonSpawnerHelpers::SpawnMeshInstance(
                ISM,
                PlacedCorner. Transform,
                RoomOrigin
            );

            if (InstanceIndex >= 0)
            {
                DebugHelpers->LogVerbose(FString::Printf(TEXT("  Spawned %s corner (instance %d)"),
                    *UEnum::GetValueAsString(PlacedCorner.Corner), InstanceIndex));
            }
            else
            {
                DebugHelpers->LogVerbose(FString::Printf(TEXT("  Failed to spawn %s corner"),
                    *UEnum::GetValueAsString(PlacedCorner. Corner)));
            }
        }
    }

    DebugHelpers->LogImportant(TEXT("Corner meshes generated successfully!"));
    DebugHelpers->LogSectionHeader(TEXT("GENERATE CORNER MESHES"));
}

void ARoomSpawner::ClearCornerMeshes()
{
	// Clear all corner ISM components
	UDungeonSpawnerHelpers::ClearISMComponentMap(CornerMeshComponents);

	// Clear generator data
	if (RoomGenerator)
	{
		RoomGenerator->ClearPlacedCorners();
	}

	DebugHelpers->LogImportant(TEXT("Corner meshes cleared"));
}
#pragma endregion

#pragma region Doorway Generation
void ARoomSpawner::GenerateDoorwayMeshes()
{
    DebugHelpers->LogSectionHeader(TEXT("GENERATE DOORWAY MESHES"));

    if (! EnsureGeneratorReady())
    {
        DebugHelpers->LogCritical(TEXT("Failed to initialize generator!"));
        DebugHelpers->LogSectionHeader(TEXT("GENERATE DOORWAY MESHES"));
        return;
    }

    // Clear existing doorway MESHES
    ClearDoorwayMeshes();

    // ✅ CHANGED:   ALWAYS call GenerateDoorways() to recalculate transforms
    // If cache exists, it will reuse layout but recalculate with current offsets
    // If no cache, it will generate new layout
    DebugHelpers->LogImportant(TEXT("Regenerating doorway transforms with current offsets... "));
    
    if (! RoomGenerator->GenerateDoorways())
    {
        DebugHelpers->LogCritical(TEXT("Doorway generation failed!"));
        DebugHelpers->LogSectionHeader(TEXT("GENERATE DOORWAY MESHES"));
        return;
    }

    // Get doorways (either from cache or newly generated)
    const TArray<FPlacedDoorwayInfo>& FinalDoorways = RoomGenerator->GetPlacedDoorways();
    
    if (FinalDoorways. Num() == 0)
    {
        DebugHelpers->LogImportant(TEXT("No doorways to spawn (none configured)"));
        DebugHelpers->LogSectionHeader(TEXT("GENERATE DOORWAY MESHES"));
        return;
    }

    DebugHelpers->LogImportant(FString::Printf(TEXT("Spawning %d doorway frames... "), FinalDoorways.Num()));

    // Get room origin for world space conversion
    FVector RoomOrigin = GetActorLocation();

    int32 FramesSpawned = 0;
    int32 FramesSkipped = 0;

    // Spawn doorway frame meshes
    for (const FPlacedDoorwayInfo& PlacedDoor : FinalDoorways)
    {
        // Validate door data
        if (!PlacedDoor.DoorData)
        {
            DebugHelpers->LogVerbose(TEXT("  Doorway has null DoorData - skipping"));
            FramesSkipped++;
            continue;
        }

        // Load frame mesh
        UStaticMesh* FrameMesh = PlacedDoor.DoorData->FrameSideMesh. LoadSynchronous();
        if (!FrameMesh)
        {
            DebugHelpers->LogVerbose(FString::Printf(TEXT("  Doorway at edge %s has no frame mesh - skipping"),
                *UEnum::GetValueAsString(PlacedDoor.Edge)));
            FramesSkipped++;
            continue;
        }

        // Get or create ISM component for door frame mesh
        UInstancedStaticMeshComponent* ISM = UDungeonSpawnerHelpers::GetOrCreateISMComponent(
            this,
            PlacedDoor.DoorData->FrameSideMesh,
            DoorwayMeshComponents,
            TEXT("DoorFrameISM_"),
            true
        );

        if (ISM)
        {
            // Spawn frame instance
            int32 InstanceIndex = UDungeonSpawnerHelpers::SpawnMeshInstance(
                ISM,
                PlacedDoor.FrameTransform,
                RoomOrigin
            );

            if (InstanceIndex >= 0)
            {
                FramesSpawned++;
                
                FString DoorType = PlacedDoor.bIsStandardDoorway ? TEXT("Standard") : TEXT("Manual");
                DebugHelpers->LogVerbose(FString::Printf(TEXT("  Spawned %s doorway frame on edge %s (instance %d)"),
                    *DoorType, *UEnum::GetValueAsString(PlacedDoor.Edge), InstanceIndex));
            }
            else
            {
                DebugHelpers->LogVerbose(FString::Printf(TEXT("  Failed to spawn doorway frame on edge %s"),
                    *UEnum::GetValueAsString(PlacedDoor. Edge)));
                FramesSkipped++;
                continue;
            }
        }
        else
        {
            DebugHelpers->LogVerbose(TEXT("  Failed to create ISM component for doorway"));
            FramesSkipped++;
            continue;
        }

        // ✅ Spawn side fills if configured
        if (PlacedDoor.DoorData->SideFillType != EDoorwaySideFill::None)
        {
            int32 TotalDoorwayWidth = PlacedDoor. WidthInCells;
            int32 DoorFrameWidth = PlacedDoor.DoorData->FrameFootprintY;
            int32 SideFillTotal = TotalDoorwayWidth - DoorFrameWidth;
            
            if (SideFillTotal > 0)
            {
                int32 LeftSideCells = SideFillTotal / 2;
                int32 RightSideCells = SideFillTotal - LeftSideCells;
                
                DebugHelpers->LogVerbose(FString::Printf(TEXT("  Spawning side fills:  Left=%d cells, Right=%d cells, Type=%s"),
                    LeftSideCells, RightSideCells, *UEnum::GetValueAsString(PlacedDoor.DoorData->SideFillType)));
                
                switch (PlacedDoor.DoorData->SideFillType)
                {
                    case EDoorwaySideFill:: WallModules:
                        if (LeftSideCells > 0)
                            SpawnDoorwaySide_WallModules(PlacedDoor, true, LeftSideCells, RoomOrigin);
                        if (RightSideCells > 0)
                            SpawnDoorwaySide_WallModules(PlacedDoor, false, RightSideCells, RoomOrigin);
                        break;
                        
                    case EDoorwaySideFill::CustomMeshes:
                        if (LeftSideCells > 0)
                            SpawnDoorwaySide_CustomMesh(PlacedDoor, true, LeftSideCells, RoomOrigin);
                        if (RightSideCells > 0)
                            SpawnDoorwaySide_CustomMesh(PlacedDoor, false, RightSideCells, RoomOrigin);
                        break;
                        
                    case EDoorwaySideFill:: CornerPieces:
                        if (LeftSideCells > 0)
                            SpawnDoorwaySide_CornerPiece(PlacedDoor, true, LeftSideCells, RoomOrigin);
                        if (RightSideCells > 0)
                            SpawnDoorwaySide_CornerPiece(PlacedDoor, false, RightSideCells, RoomOrigin);
                        break;
                        
                    default:
                        break;
                }
            }
        }
    }

    DebugHelpers->LogImportant(FString::Printf(TEXT("Doorway spawning complete: %d frames spawned, %d skipped"),
        FramesSpawned, FramesSkipped));
    DebugHelpers->LogSectionHeader(TEXT("GENERATE DOORWAY MESHES"));
}

void ARoomSpawner::ClearDoorwayMeshes()
{
	// Clear all doorway ISM components (mesh instances)
	UDungeonSpawnerHelpers::ClearISMComponentMap(DoorwayMeshComponents);

	// ✅ CHANGED: Do NOT clear generator doorway data
	// The doorway layout should persist until ClearRoomGrid() is called
	// This allows doorway meshes to be respawned without regenerating layout

	DebugHelpers->LogImportant(TEXT("Doorway meshes cleared (layout preserved)"));
}

#pragma region Doorway Side Fill Spawning

void ARoomSpawner::SpawnDoorwaySide_WallModules(const FPlacedDoorwayInfo& Doorway, bool bIsLeftSide, int32 CellCount, FVector RoomOrigin)
{
    if (!Doorway.DoorData) return;

    // Get wall modules to use
    const TArray<FWallModule>& Modules = bIsLeftSide ? Doorway.DoorData->LeftSideModules : Doorway.DoorData->RightSideModules;

    if (Modules.Num() == 0)
    { DebugHelpers->LogVerbose(TEXT("No wall modules configured for side fill")); return; }

    DebugHelpers->LogVerbose(FString::Printf(TEXT("Spawning %s side (wall modules): %d cells"),
        bIsLeftSide ? TEXT("LEFT") : TEXT("RIGHT"), CellCount));

    // Calculate starting cell for this side
    int32 SideStartCell;
	// Left side starts at doorway start
    if (bIsLeftSide) { SideStartCell = Doorway.StartCell; }
    else
    {
        // Right side starts after door frame
        int32 LeftSideCells = (Doorway. WidthInCells - Doorway.DoorData->FrameFootprintY) / 2;
        SideStartCell = Doorway.StartCell + LeftSideCells + Doorway.DoorData->FrameFootprintY;
    }

    // Bin-packing:   Fill with largest modules first
    int32 RemainingCells = CellCount;
    int32 CurrentCell = SideStartCell;

    FRandomStream Stream(FMath:: Rand());

    while (RemainingCells > 0)
    {
        // Find modules that fit
        TArray<const FWallModule*> FittingModules;
        TArray<float> Weights;
        float TotalWeight = 0.0f;

        for (const FWallModule& Module : Modules)
        {
            if (Module.Y_AxisFootprint <= RemainingCells)
            {
                FittingModules. Add(&Module);
                Weights.Add(Module.PlacementWeight);
                TotalWeight += Module.PlacementWeight;
            }
        }

        if (FittingModules. Num() == 0)
        { DebugHelpers->LogVerbose(FString::Printf(TEXT("      No modules fit remaining %d cells"), RemainingCells)); break; }

        // Weighted random selection
        float RandomValue = Stream.FRandRange(0.0f, TotalWeight);
        float CumulativeWeight = 0.0f;
        const FWallModule* SelectedModule = nullptr;

        for (int32 i = 0; i < FittingModules. Num(); ++i)
        {
            CumulativeWeight += Weights[i];
            if (RandomValue <= CumulativeWeight)
            {
                SelectedModule = FittingModules[i];
                break;
            }
        }

        if (! SelectedModule)
            SelectedModule = FittingModules. Last();

        // Load mesh
        UStaticMesh* Mesh = SelectedModule->BaseMesh.LoadSynchronous();
        if (!Mesh)
        {
            DebugHelpers->LogVerbose(TEXT("      Failed to load module mesh"));
            break;
        }

        // Calculate position (using DungeonGenerationHelpers)
        FVector LocalPos = UDungeonGenerationHelpers:: CalculateWallPosition(Doorway.Edge,CurrentCell,
        	SelectedModule->Y_AxisFootprint, RoomGenerator->GetGridSize(), CELL_SIZE, 0.0f, 0.0f, 0.0f, 0.0f);

        FRotator Rotation = UDungeonGenerationHelpers::GetWallRotationForEdge(Doorway.Edge);

        // Spawn mesh instance
        UInstancedStaticMeshComponent* ISM = UDungeonSpawnerHelpers::GetOrCreateISMComponent(this,
        SelectedModule->BaseMesh, DoorwayMeshComponents, TEXT("DoorSideFillISM_"), true);

        if (ISM)
        {
            FTransform LocalTransform(Rotation, LocalPos, FVector:: OneVector);
            UDungeonSpawnerHelpers::SpawnMeshInstance(ISM, LocalTransform, RoomOrigin);

            DebugHelpers->LogVerbose(FString::Printf(TEXT("      Placed %d-cell module at cell %d"),
                SelectedModule->Y_AxisFootprint, CurrentCell));
        }

        RemainingCells -= SelectedModule->Y_AxisFootprint;
        CurrentCell += SelectedModule->Y_AxisFootprint;
    }
}

void ARoomSpawner::SpawnDoorwaySide_CustomMesh(const FPlacedDoorwayInfo& Doorway, bool bIsLeftSide, int32 CellCount, FVector RoomOrigin)
{
    if (!Doorway.DoorData) return;

    // Load mesh
    UStaticMesh* Mesh = bIsLeftSide ? Doorway.DoorData->LeftSideMesh.LoadSynchronous() : Doorway.DoorData->RightSideMesh. LoadSynchronous();

    if (!Mesh)
    {DebugHelpers->LogVerbose(TEXT("    No custom mesh configured for side fill")); return; }

    DebugHelpers->LogVerbose(FString::Printf(TEXT("    Spawning %s side (custom mesh): %d cells"),
        bIsLeftSide ?  TEXT("LEFT") : TEXT("RIGHT"), CellCount));

    // Calculate starting cell
    int32 SideStartCell;
    if (bIsLeftSide) { SideStartCell = Doorway. StartCell; }
    else
    {
        int32 LeftSideCells = (Doorway.WidthInCells - Doorway. DoorData->FrameFootprintY) / 2;
        SideStartCell = Doorway.StartCell + LeftSideCells + Doorway.DoorData->FrameFootprintY;
    }

    // Calculate position (center of side fill span)
    FVector LocalPos = UDungeonGenerationHelpers::CalculateWallPosition(Doorway.Edge, SideStartCell, CellCount,
    RoomGenerator->GetGridSize(), CELL_SIZE,0.0f, 0.0f, 0.0f, 0.0f);

    FRotator Rotation = UDungeonGenerationHelpers::GetWallRotationForEdge(Doorway.Edge);

    // Spawn mesh
    UInstancedStaticMeshComponent* ISM = UDungeonSpawnerHelpers::GetOrCreateISMComponent( this,
    bIsLeftSide ? Doorway.DoorData->LeftSideMesh : Doorway. DoorData->RightSideMesh, DoorwayMeshComponents,TEXT("DoorSideFillISM_"),true);

    if (ISM)
    {
        FTransform LocalTransform(Rotation, LocalPos, FVector::OneVector);
        UDungeonSpawnerHelpers::SpawnMeshInstance(ISM, LocalTransform, RoomOrigin);

        DebugHelpers->LogVerbose(TEXT("      Custom mesh placed successfully"));
    }
}

void ARoomSpawner::SpawnDoorwaySide_CornerPiece(const FPlacedDoorwayInfo& Doorway, bool bIsLeftSide, int32 CellCount, FVector RoomOrigin)
{
    if (!Doorway.DoorData) return;

    UStaticMesh* Mesh = Doorway.DoorData->CornerMesh.LoadSynchronous();
    if (!Mesh)
    { DebugHelpers->LogVerbose(TEXT("    No corner mesh configured for side fill")); return; }

    DebugHelpers->LogVerbose(FString:: Printf(TEXT("    Spawning %s side (corner piece): %d cells"),
        bIsLeftSide ? TEXT("LEFT") : TEXT("RIGHT"), CellCount));

    // Same as custom mesh (single piece)
    SpawnDoorwaySide_CustomMesh(Doorway, bIsLeftSide, CellCount, RoomOrigin);
}
#pragma endregion
#pragma endregion

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
#pragma endregion

#pragma region Debug Functions
void ARoomSpawner::ToggleCoordinates()
{
	DebugHelpers->bShowCoordinates = !DebugHelpers->bShowCoordinates;
	DebugHelpers->LogImportant(FString::Printf(TEXT("Coordinates display: %s"), 
		DebugHelpers->bShowCoordinates ? TEXT("ON") : TEXT("OFF")));
    
	if (!bIsGenerated || !RoomGenerator)
	{
		DebugHelpers->LogImportant(TEXT("No room to visualize. Generate a room first. "));
		return;
	}
    
	// ========================================================================
	// Coordinates use UTextRenderComponent (not debug shapes)
	// They're managed separately and don't require ClearDebugDrawings()
	// Just call the coordinate function directly
	// ========================================================================
    
	// Get grid data
	FVector RoomOrigin = GetActorLocation();
	FIntPoint GridSize = RoomGenerator->GetGridSize();
	float CellSize = RoomGenerator->GetCellSize();
    
	// Toggle coordinates (function handles clearing internally)
	DebugHelpers->DrawGridCoordinatesWithTextComponents(GridSize, CellSize, RoomOrigin);
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
	  
	// Toggle cell state visualization
	DebugHelpers->bShowCellStates = ! DebugHelpers->bShowCellStates;
	DebugHelpers->bShowForcedEmptyRegions = DebugHelpers->bShowCellStates;
	DebugHelpers->bShowForcedEmptyCells = DebugHelpers->bShowCellStates;
    
	// ✅ NEW: Also toggle grid lines with cell states
	// Grid provides context for understanding cell visualization
	DebugHelpers->bShowGrid = DebugHelpers->bShowCellStates;
    
	DebugHelpers->LogImportant(FString::Printf(TEXT("Cell states display: %s"), 
		DebugHelpers->bShowCellStates ?  TEXT("ON") : TEXT("OFF")));
    
	if (!bIsGenerated || !RoomGenerator)
	{
		DebugHelpers->LogImportant(TEXT("No room to visualize. Generate a room first."));
		return;
	}
    
	// Use standard refresh (clear and redraw everything based on toggle states)
	RefreshVisualization();
}

UTextRenderComponent* ARoomSpawner::CreateTextRenderComponent(FVector WorldPosition, FString Text, FColor Color, float Scale)
{
	// Create new text render component
	UTextRenderComponent* TextComp = NewObject<UTextRenderComponent>(this);
	
	if (! TextComp) return nullptr;

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
	TextComp->SetHiddenInGame(true); // Show in PIE too
	
	return TextComp;
}

void ARoomSpawner::DestroyTextRenderComponent(UTextRenderComponent* TextComp)
{
	if (!TextComp || !TextComp->IsValidLowLevel()) return;

	// Destroy the component
	TextComp->DestroyComponent();
}

void ARoomSpawner::LogRoomStatistics()
{
	if (!RoomGenerator) return;

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
	
	DebugHelpers->LogSectionHeader(TEXT("ROOM STATISTICS"));
}

void ARoomSpawner::LogFloorStatistics()
{
	if (!RoomGenerator) return;

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
	{ DebugHelpers->DrawForcedEmptyRegions(RoomData->ForcedEmptyRegions, GridSize, CellSize, RoomOrigin); }

	// Draw forced empty cells (if any)
	if (RoomData && RoomData->ForcedEmptyFloorCells.Num() > 0)
	{ DebugHelpers->DrawForcedEmptyCells(RoomData->ForcedEmptyFloorCells, GridSize, CellSize, RoomOrigin);}
	DebugHelpers->LogVerbose(TEXT("Visualization updated."));
}
#pragma endregion
#endif // WITH_EDITOR