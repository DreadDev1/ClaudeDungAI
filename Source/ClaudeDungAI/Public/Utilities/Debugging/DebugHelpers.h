// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/Grid/GridData.h"
#include "DebugHelpers.generated.h"

// Log verbosity levels
UENUM(BlueprintType)
enum class EDebugLogLevel : uint8
{
	None = 0       UMETA(DisplayName = "None (No Logging)"),
	Critical = 1   UMETA(DisplayName = "Critical Only"),
	Important = 2  UMETA(DisplayName = "Important Messages"),
	Verbose = 3    UMETA(DisplayName = "Verbose"),
	Everything = 4 UMETA(DisplayName = "Everything")
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CLAUDEDUNGAI_API UDebugHelpers : public UActorComponent
{
	GENERATED_BODY()

public: 
	// Sets default values for this component's properties
	UDebugHelpers();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	// Master toggle for all debug features
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings")
	bool bEnableDebug = true;

	// Log level for console output
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings|Logging")
	EDebugLogLevel LogLevel = EDebugLogLevel::Important;

	// Enable grid visualization
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings|Visualization")
	bool bShowGrid = true;

	// Enable cell state visualization (colored boxes)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings|Visualization")
	bool bShowCellStates = true;

	// Enable coordinate labels at each cell
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings|Visualization")
	bool bShowCoordinates = true;

	// Enable forced placement visualization
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings|Visualization")
	bool bShowForcedPlacements = true;

	// Duration for debug drawings (in seconds, -1 for permanent)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings|Visualization")
	float DebugDrawDuration = -1.0f;

	// Thickness of debug lines
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings|Visualization")
	float LineThickness = 2.0f;

	// Scale of coordinate text
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings|Visualization")
	float CoordinateTextScale = 1.5f;

	// Height offset for coordinate text above grid
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings|Visualization")
	float CoordinateTextHeight = 10.0f;

	// ============================================================================
	// COLOR SCHEME
	// ============================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings|Colors")
	FColor GridColor = FColor:: White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings|Colors")
	FColor EmptyCellColor = FColor::Green;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings|Colors")
	FColor OccupiedCellColor = FColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings|Colors")
	FColor UnoccupiedCellColor = FColor::Blue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings|Colors")
	FColor WallCellColor = FColor::Orange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings|Colors")
	FColor DoorCellColor = FColor:: Cyan;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings|Colors")
	FColor ForcedPlacementColor = FColor:: Magenta;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings|Colors")
	FColor CoordinateTextColor = FColor:: White;

	// ============================================================================
	// LOGGING API
	// ============================================================================

	// Log a message at specified level
	void Log(EDebugLogLevel Level, const FString& Message);

	// Convenience functions for different log levels
	void LogCritical(const FString& Message);
	void LogImportant(const FString& Message);
	void LogVerbose(const FString& Message);
	void LogEverything(const FString& Message);

	// Formatted logging with category
	void LogWithCategory(EDebugLogLevel Level, const FString& Category, const FString& Message);

	// Log a section header (for visual separation)
	void LogSectionHeader(const FString& Title);

	// Log statistics
	void LogStatistic(const FString& Label, int32 Value);
	void LogStatistic(const FString& Label, float Value);
	void LogStatistic(const FString& Label, const FString& Value);

	// ============================================================================
	// GRID VISUALIZATION API
	// ============================================================================

	/**
	 * Draw the entire grid with cell states and coordinates
	 * @param GridSize - The size of the grid in cells (X, Y)
	 * @param CellStates - Array of cell states (Empty, Occupied, Unoccupied, Wall, Doorway)
	 * @param CellSize - Size of each cell in cm (default 100.0)
	 * @param OriginLocation - World location of grid origin (0,0)
	 */
	void DrawGrid(FIntPoint GridSize, const TArray<EGridCellType>& CellStates, float CellSize = CELL_SIZE, FVector OriginLocation = FVector::ZeroVector);

	/**
	 * Draw grid outline only (no cell states)
	 * @param GridSize - The size of the grid in cells (X, Y)
	 * @param CellSize - Size of each cell in cm (default 100.0)
	 * @param OriginLocation - World location of grid origin (0,0)
	 */
	void DrawGridOutline(FIntPoint GridSize, float CellSize = CELL_SIZE, FVector OriginLocation = FVector::ZeroVector);

	/**
	 * Draw coordinate labels at each cell center
	 * @param GridSize - The size of the grid in cells (X, Y)
	 * @param CellSize - Size of each cell in cm (default 100.0)
	 * @param OriginLocation - World location of grid origin (0,0)
	 */
	void DrawGridCoordinates(FIntPoint GridSize, float CellSize = CELL_SIZE, FVector OriginLocation = FVector::ZeroVector);

	/**
	 * Draw a single cell with specified color
	 * @param GridCoord - Grid coordinates (X, Y)
	 * @param Color - Color to draw the cell
	 * @param CellSize - Size of each cell in cm (default 100.0)
	 * @param OriginLocation - World location of grid origin (0,0)
	 */
	void DrawCell(FIntPoint GridCoord, FColor Color, float CellSize = CELL_SIZE, FVector OriginLocation = FVector::ZeroVector);

	/**
	 * Draw cell states for the entire grid
	 * @param GridSize - The size of the grid in cells (X, Y)
	 * @param CellStates - Array of cell states
	 * @param CellSize - Size of each cell in cm (default 100.0)
	 * @param OriginLocation - World location of grid origin (0,0)
	 */
	void DrawCellStates(FIntPoint GridSize, const TArray<EGridCellType>& CellStates, float CellSize = CELL_SIZE, FVector OriginLocation = FVector::ZeroVector);

	/**
	 * Clear all debug drawings (if using non-persistent draws)
	 */
	void ClearDebugDrawings();

private:
	// Helper to check if logging should occur
	bool ShouldLog(EDebugLogLevel Level) const;

	// Helper to get category prefix
	FString GetCategoryPrefix() const;

	// Helper to get color for cell type
	FColor GetColorForCellType(EGridCellType CellType) const;

	// Helper to convert grid coordinates to world position (center of cell)
	FVector GridToWorldPosition(FIntPoint GridCoord, float CellSize, FVector OriginLocation) const;
};