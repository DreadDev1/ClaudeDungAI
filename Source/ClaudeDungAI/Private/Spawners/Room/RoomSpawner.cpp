// Fill out your copyright notice in the Description page of Project Settings.

#include "Spawners/Room/RoomSpawner.h"

#include "Components/TextRenderComponent.h"
#include "Generators/Room/RoomGenerator.h"

// Sets default values
ARoomSpawner::ARoomSpawner()
{
 	// Set this actor to call Tick() every frame (can turn off for performance)
	PrimaryActorTick.bCanEverTick = false;

	// Create debug helpers component
	DebugHelpers = CreateDefaultSubobject<UDebugHelpers>(TEXT("DebugHelpers"));

	// NEW: Bind delegate so DebugHelpers can request text components
	DebugHelpers->OnCreateTextComponent.BindUObject(this, &ARoomSpawner::CreateTextRenderComponent);
	// Initialize flags
	bIsGenerated = false;
}

// Called when the game starts or when spawned
void ARoomSpawner::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void ARoomSpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// ============================================================================
// EDITOR FUNCTIONS
// ============================================================================

#if WITH_EDITOR
void ARoomSpawner::GenerateRoomGrid()
{
	DebugHelpers->LogSectionHeader(TEXT("GENERATE ROOM GRID"));

	// Validate RoomData
	if (!RoomData)
	{
		DebugHelpers->LogCritical(TEXT("RoomData is not assigned!  Please assign a RoomData asset."));
		return;
	}

	// Initialize generator
	if (!InitializeGenerator())
	{
		DebugHelpers->LogCritical(TEXT("Failed to initialize RoomGenerator!"));
		return;
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
		DebugHelpers->LogImportant(TEXT("No room grid to clear. "));
		return;
	}

	// Clear the grid
	RoomGenerator->ClearGrid();
	bIsGenerated = false;

	// Clear coordinate text components (DebugHelpers manages them)
	DebugHelpers->ClearCoordinateTextComponents();
	
	// Clear debug drawings
	DebugHelpers->ClearDebugDrawings();

	DebugHelpers->LogImportant(TEXT("Room grid cleared. "));
}

void ARoomSpawner::RefreshVisualization()
{
	DebugHelpers->LogImportant(TEXT("Refreshing visualization..."));

	if (!bIsGenerated || !RoomGenerator)
	{
		DebugHelpers->LogImportant(TEXT("No room to visualize.  Generate a room first."));
		return;
	}

	// Clear existing drawings
	DebugHelpers->ClearDebugDrawings();

	// Redraw with current settings
	UpdateVisualization();

	DebugHelpers->LogImportant(TEXT("Visualization refreshed. "));
}

void ARoomSpawner::ToggleCoordinates()
{
	DebugHelpers->bShowCoordinates = !DebugHelpers->bShowCoordinates;
	DebugHelpers->LogImportant(FString::Printf(TEXT("Coordinates display: %s"), 
		DebugHelpers->bShowCoordinates ? TEXT("ON") : TEXT("OFF")));
	
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

	return TextComp;
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

#endif // WITH_EDITOR

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

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
	if (!RoomGenerator || !bIsGenerated)
	{
		return;
	}

	// Get grid data
	FIntPoint GridSize = RoomGenerator->GetGridSize();
	const TArray<EGridCellType>& GridState = RoomGenerator->GetGridState();
	float CellSize = RoomGenerator->GetCellSize();
	FVector RoomOrigin = GetActorLocation();

	// Draw the grid with all enabled visualizations
	DebugHelpers->DrawGrid(GridSize, GridState, CellSize, RoomOrigin);

	DebugHelpers->LogVerbose(TEXT("Visualization updated."));
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