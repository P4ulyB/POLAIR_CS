// Copyright 2025 BitProtectStudio. All Rights Reserved.


#include "InterestingPlacePro.h"

// Sets default values
AInterestingPlacePro::AInterestingPlacePro()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

}

// Called when the game starts or when spawned
void AInterestingPlacePro::BeginPlay()
{
	Super::BeginPlay();

	findRadiusSquare = FMath::Square(findRadius);
}

// Called every frame
void AInterestingPlacePro::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

