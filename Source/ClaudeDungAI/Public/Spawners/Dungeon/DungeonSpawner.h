// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DungeonSpawner.generated.h"

UCLASS()
class CLAUDEDUNGAI_API ADungeonSpawner : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ADungeonSpawner();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
