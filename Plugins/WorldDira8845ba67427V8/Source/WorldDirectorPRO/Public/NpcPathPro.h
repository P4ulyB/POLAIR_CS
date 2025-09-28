// Copyright 2025 BitProtectStudio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NpcPathPro.generated.h"

UCLASS()
class WORLDDIRECTORPRO_API ANpcPathPro : public AActor
{
	GENERATED_BODY()
	
public:
	
	// Sets default values for this actor's properties
	ANpcPathPro();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	//************************************************************************
	// Component                                                                  
	//************************************************************************

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Component")
	class USplineComponent* SplinePathComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Component")
	class UInstancedStaticMeshComponent* StaticMeshInstanceComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Component")
	class USceneComponent* SceneRootComponent;

	//************************************************************************

	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "World Director Parameters");
	bool bSaveParameters = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "World Director Parameters");
	bool bIsDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Director Parameters", meta = (ClampMin = 0))
	float distanceBetweenPathPoints = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Director Parameters", meta = (ClampMin = 0))
	float radiusNearSplinePoint = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Director Parameters", meta = (ClampMin = 0))
	float radiusRandomPointNearPoint = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Director Parameters", meta = (ClampMin = 3))
	int maxWalkingPoints = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Director Parameters", meta = (ClampMin = 0, ClampMax = 100))
	float chanceAttractAttention = 100.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Director Telemetry")
	int pointsCount = 0;

	UPROPERTY()
	TArray<FVector> pathPointsArr;

protected:
	
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

private:	
	

};
