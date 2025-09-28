// Copyright 2025 BitProtectStudio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InterestingPlacePro.generated.h"

UCLASS()
class WORLDDIRECTORPRO_API AInterestingPlacePro : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AInterestingPlacePro();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Director Parameters", meta = (ClampMin = 0))
	float walkingRadius = 1500.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Director Parameters", meta = (ClampMin = 0, ClampMax = 100))
	float chanceAttractAttention = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Director Parameters", meta = (ClampMin = 0))
	float findRadius = 5000.f;
	UPROPERTY()
	float findRadiusSquare = 0.f;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

private:	
	

};
