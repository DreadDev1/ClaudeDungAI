// Fill out your copyright notice in the Description page of Project Settings.


#include "Spawners/Dungeon/DungeonSpawner.h"


// Sets default values
ADungeonSpawner::ADungeonSpawner()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ADungeonSpawner::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ADungeonSpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

