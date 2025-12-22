// Fill out your copyright notice in the Description page of Project Settings.

#include "Utilities/Helpers/DungeonGenerationHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"

// ========================================================================
// GRID & CELL OPERATIONS
// ========================================================================

TArray<int32> UDungeonGenerationHelpers::GetEdgeCellIndices(EWallEdge Edge, FIntPoint GridSize)
{
	TArray<int32> Cells;

	switch (Edge)
	{
		case EWallEdge::North:  // +X edge
			for (int32 Y = 0; Y < GridSize.Y; ++Y)
				Cells.Add(Y);
			break;

		case EWallEdge:: South:  // -X edge
			for (int32 Y = 0; Y < GridSize. Y; ++Y)
				Cells.Add(Y);
			break;

		case EWallEdge::East:    // +Y edge
			for (int32 X = 0; X < GridSize.X; ++X)
				Cells.Add(X);
			break;

		case EWallEdge:: West:   // -Y edge
			for (int32 X = 0; X < GridSize. X; ++X)
				Cells.Add(X);
			break;

		default:
			break;
	}

	return Cells;
}

bool UDungeonGenerationHelpers::IsValidGridCoordinate(FIntPoint Coord, FIntPoint GridSize)
{
	return Coord.X >= 0 && Coord.X < GridSize. X && Coord.Y >= 0 && Coord.Y < GridSize.Y;
}

FIntPoint UDungeonGenerationHelpers::IndexToCoordinate(int32 Index, int32 GridWidth)
{
	if (GridWidth <= 0) return FIntPoint:: ZeroValue;
	
	int32 X = Index / GridWidth;
	int32 Y = Index % GridWidth;
	return FIntPoint(X, Y);
}

int32 UDungeonGenerationHelpers::CoordinateToIndex(FIntPoint Coord, int32 GridWidth)
{
	return Coord.Y * GridWidth + Coord.X;
}

// ========================================================================
// ROTATION & FOOTPRINT OPERATIONS
// ========================================================================

FIntPoint UDungeonGenerationHelpers::GetRotatedFootprint(FIntPoint OriginalFootprint, int32 RotationDegrees)
{
	// Normalize rotation to 0-360 range
	RotationDegrees = RotationDegrees % 360;
	if (RotationDegrees < 0) RotationDegrees += 360;

	// 90° and 270° rotations swap X and Y
	if (RotationDegrees == 90 || RotationDegrees == 270)
	{
		return FIntPoint(OriginalFootprint.Y, OriginalFootprint.X);
	}

	// 0° and 180° keep original dimensions
	return OriginalFootprint;
}

bool UDungeonGenerationHelpers::DoesRotationSwapDimensions(int32 RotationDegrees)
{
	RotationDegrees = RotationDegrees % 360;
	if (RotationDegrees < 0) RotationDegrees += 360;

	return (RotationDegrees == 90 || RotationDegrees == 270);
}

// ========================================================================
// WALL EDGE OPERATIONS
// ========================================================================

FRotator UDungeonGenerationHelpers::GetWallRotationForEdge(EWallEdge Edge)
{
	// All walls face INWARD toward room interior
	switch (Edge)
	{
		case EWallEdge::North:  // X = Max, face South (-X, into room)
			return FRotator(0.0f, 180.0f, 0.0f);

		case EWallEdge:: South:  // X = 0, face North (+X, into room)
			return FRotator(0.0f, 0.0f, 0.0f);

		case EWallEdge::East:   // Y = Max, face West (-Y, into room)
			return FRotator(0.0f, 270.0f, 0.0f);

		case EWallEdge::West:   // Y = 0, face East (+Y, into room)
			return FRotator(0.0f, 90.0f, 0.0f);

		default:
			return FRotator::ZeroRotator;
	}
}

FVector UDungeonGenerationHelpers::CalculateWallPosition(
	EWallEdge Edge,
	int32 StartCell,
	int32 SpanLength,
	FIntPoint GridSize,
	float CellSize,
	float NorthOffset,
	float SouthOffset,
	float EastOffset,
	float WestOffset)
{
	float HalfSpan = (SpanLength * CellSize) * 0.5f;
	FVector Position = FVector:: ZeroVector;

	switch (Edge)
	{
		case EWallEdge::North:  // North wall:  X = GridSize.X
			Position = FVector(
				(GridSize.X * CellSize) + NorthOffset,
				(StartCell * CellSize) + HalfSpan,
				0.0f
			);
			break;

		case EWallEdge::South:  // South wall: X = 0
			Position = FVector(
				0.0f + SouthOffset,
				(StartCell * CellSize) + HalfSpan,
				0.0f
			);
			break;

		case EWallEdge::East:   // East wall: Y = GridSize.Y
			Position = FVector(
				(StartCell * CellSize) + HalfSpan,
				(GridSize.Y * CellSize) + EastOffset,
				0.0f
			);
			break;

		case EWallEdge::West:   // West wall: Y = 0
			Position = FVector(
				(StartCell * CellSize) + HalfSpan,
				0.0f + WestOffset,
				0.0f
			);
			break;

		default:
			break;
	}

	return Position;
}

// ========================================================================
// MESH OPERATIONS
// ========================================================================

UStaticMesh* UDungeonGenerationHelpers::LoadAndValidateMesh(
	const TSoftObjectPtr<UStaticMesh>& MeshAsset,
	const FString& ContextName,
	bool bLogWarning)
{
	if (MeshAsset.IsNull())
	{
		if (bLogWarning)
		{
			UE_LOG(LogTemp, Warning, TEXT("LoadAndValidateMesh: Null mesh asset for context '%s'"), *ContextName);
		}
		return nullptr;
	}

	UStaticMesh* Mesh = MeshAsset.LoadSynchronous();
	if (!Mesh && bLogWarning)
	{
		UE_LOG(LogTemp, Warning, TEXT("LoadAndValidateMesh: Failed to load mesh for context '%s'"), *ContextName);
	}

	return Mesh;
}

// ========================================================================
// SOCKET OPERATIONS
// ========================================================================

bool UDungeonGenerationHelpers:: GetMeshSocketTransform(
	UStaticMesh* Mesh,
	FName SocketName,
	FVector& OutLocation,
	FRotator& OutRotation)
{
	if (!Mesh)
		return false;

	UStaticMeshSocket* Socket = Mesh->FindSocket(SocketName);
	if (Socket)
	{
		OutLocation = Socket->RelativeLocation;
		OutRotation = Socket->RelativeRotation;
		return true;
	}

	return false;
}

bool UDungeonGenerationHelpers:: GetMeshSocketTransformWithFallback(
	UStaticMesh* Mesh,
	FName SocketName,
	FVector& OutLocation,
	FRotator& OutRotation,
	FVector FallbackLocation,
	FRotator FallbackRotation)
{
	if (GetMeshSocketTransform(Mesh, SocketName, OutLocation, OutRotation))
	{
		return true;
	}

	// Use fallback
	OutLocation = FallbackLocation;
	OutRotation = FallbackRotation;
	return false;
}

FTransform UDungeonGenerationHelpers::CalculateSocketWorldTransform(
	UStaticMesh* Mesh,
	FName SocketName,
	const FTransform& ParentTransform,
	FVector FallbackOffset)
{
	FVector SocketLocation;
	FRotator SocketRotation;

	if (! GetMeshSocketTransform(Mesh, SocketName, SocketLocation, SocketRotation))
	{
		// Use fallback
		SocketLocation = FallbackOffset;
		SocketRotation = FRotator::ZeroRotator;
	}

	// Create socket transform and chain with parent
	FTransform SocketTransform(SocketRotation, SocketLocation);
	return SocketTransform * ParentTransform;
}

// ========================================================================
// WEIGHTED RANDOM SELECTION (Specialized Implementations)
// ========================================================================

const FWallModule* UDungeonGenerationHelpers::SelectWeightedWallModule(const TArray<FWallModule>& Modules)
{
	return SelectWeightedRandom<FWallModule>(
		Modules,
		[](const FWallModule& Module) { return Module.PlacementWeight; }
	);
}

const FMeshPlacementInfo* UDungeonGenerationHelpers::SelectWeightedMeshPlacement(const TArray<FMeshPlacementInfo>& MeshPool)
{
	return SelectWeightedRandom<FMeshPlacementInfo>(
		MeshPool,
		[](const FMeshPlacementInfo& Info) { return Info.PlacementWeight; }
	);
}