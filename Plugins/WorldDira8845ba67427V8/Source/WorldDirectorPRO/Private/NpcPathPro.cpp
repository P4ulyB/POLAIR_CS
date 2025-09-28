// Copyright 2025 BitProtectStudio. All Rights Reserved.


#include "NpcPathPro.h"

#include "NavigationSystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineComponent.h"

// Sets default values
ANpcPathPro::ANpcPathPro()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	SceneRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRootComponent"));
	RootComponent = SceneRootComponent;

	SplinePathComponent = CreateDefaultSubobject<USplineComponent>(TEXT("SplinePathComponent"));
	SplinePathComponent->SetupAttachment(RootComponent);

	StaticMeshInstanceComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("StaticMeshInstanceComponent"));
	StaticMeshInstanceComponent->SetupAttachment(RootComponent);
	StaticMeshInstanceComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StaticMeshInstanceComponent->SetGenerateOverlapEvents(false);
	StaticMeshInstanceComponent->SetCastShadow(false);
}

void ANpcPathPro::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (bIsDebug)
	{
		StaticMeshInstanceComponent->ClearInstances();
		
		for (int i = 0; i < pathPointsArr.Num(); ++i)
		{
			FTransform newPathInstance_(FTransform::Identity);
			newPathInstance_.SetLocation(pathPointsArr[i]);
			StaticMeshInstanceComponent->AddInstance(newPathInstance_, true);
		}
	}
	else
	{
		StaticMeshInstanceComponent->ClearInstances();
	}

	if (!bSaveParameters)
	{
		pointsCount = 0;
		pathPointsArr.Empty();
	
		UNavigationSystemV1* navSystem_ = UNavigationSystemV1::GetNavigationSystem(this);
		if (navSystem_)
		{
			float splineLenght_ = SplinePathComponent->GetDistanceAlongSplineAtSplinePoint(SplinePathComponent->GetNumberOfSplinePoints() - 1);
			float nextPointDistance_(0.f);
		
			while (nextPointDistance_ < splineLenght_)
			{
				FVector splineWorldPoint_ = SplinePathComponent->GetLocationAtDistanceAlongSpline(nextPointDistance_, ESplineCoordinateSpace::World);
				nextPointDistance_ += distanceBetweenPathPoints;

				FNavLocation randNavPathPoint_;
				navSystem_->GetRandomPointInNavigableRadius(splineWorldPoint_, radiusNearSplinePoint, randNavPathPoint_);
				if (randNavPathPoint_.NodeRef)
				{
					pointsCount++;					
					pathPointsArr.Add(randNavPathPoint_.Location);
				}
			}
		}
	}
}

// Called when the game starts or when spawned
void ANpcPathPro::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		TArray<AActor*> foundActorsArr;
	}
}

// Called every frame
void ANpcPathPro::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}
