// Copyright 2025 BitProtectStudio. All Rights Reserved.


#include "NpcDataProComponent.h"

#include "NiagaraComponent.h"

#include "WorldDirectorNpcPRO.h"
#include "GameFramework/PawnMovementComponent.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine.h"
#include "GameFramework/Pawn.h"

// Sets default values for this component's properties
UNpcDataProComponent::UNpcDataProComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}


// Called when the game starts
void UNpcDataProComponent::BeginPlay()
{
	Super::BeginPlay();


	if (bIsActivate)
	{
		float inFirstDelay_ = FMath::RandRange(0.01f, 0.1f);
		float loopRate_ = FMath::RandRange(0.25f, 0.5f);
		// Optimization timer.
		GetWorld()->GetTimerManager().SetTimer(registerNpc_Timer, this, &UNpcDataProComponent::InitializeNPC, loopRate_, true, inFirstDelay_);
	}
}

void UNpcDataProComponent::InitializeNPC()
{
	TArray<AActor*> allActorsArr_;
	UGameplayStatics::GetAllActorsOfClass(this, AWorldDirectorNpcPRO::StaticClass(), allActorsArr_);

	npcData.maxDistanceSearchRoadSquare = FMath::Square(npcData.maxDistanceSearchRoad);
	npcData.mainLayerRadiusSquare = FMath::Square(npcData.firstLayerRadius + npcData.layerOffset);
	npcData.firstLayerRadiusSquare = FMath::Square(npcData.firstLayerRadius);
	npcData.secondLayerRadiusSquare = FMath::Square(npcData.secondLayerRadius);
	npcData.thirdLayerRadiusSquare = FMath::Square(npcData.thirdLayerRadius);

	if (allActorsArr_.IsValidIndex(0))
	{
		if (allActorsArr_[0])
		{
			directorNPCRef = Cast<AWorldDirectorNpcPRO>(allActorsArr_[0]);
		}
	}

	if (IsValid(directorNPCRef))
	{
		GetWorld()->GetTimerManager().ClearTimer(registerNpc_Timer);
		
		bIsActivate = true;

		hiddenTickComponentsInterval = FMath::RandRange(0.1f, 0.25f);

		ownerActor = Cast<AActor>(GetOwner());

		TArray<UActorComponent*> allTickComponents_;
		GetOwner()->GetComponents(allTickComponents_);
		for (int compID_ = 0; compID_ < allTickComponents_.Num(); ++compID_)
		{
			defaultTickComponentsInterval.Add(allTickComponents_[compID_]->GetComponentTickInterval());
		}
		defaultTickActorInterval = ownerActor->GetActorTickInterval();
		hiddenTickActorInterval = hiddenTickComponentsInterval;

		// Register NPC
		if (ownerActor != nullptr)
		{
			bool bIsPawn_(false);
			APawn* myPawn_ = Cast<APawn>(ownerActor);
			if (myPawn_)
			{
				if (myPawn_->GetMovementComponent())
				{
					npcData.bIsPawnClass = true;
					if (!myPawn_->GetMovementComponent()->IsFalling())
					{
						if (directorNPCRef->RegisterNPC(Cast<APawn>(GetOwner())))
						{
							bIsPawn_ = true;
							bIsRegistered = true;
						}
					}
				}
			}

			if (!bIsPawn_)
			{
				if (directorNPCRef->RegisterNPC(Cast<AActor>(ownerActor)))
				{
					bIsRegistered = true;
				}
			}
		}

		if (GetNetMode() != NM_DedicatedServer)
		{
			if (UGameplayStatics::GetPlayerController(GetWorld(), 0)->IsLocalController())
			{
				float rateOptimization_ = FMath::RandRange(0.1f, 0.2f);
				// Optimization timer.
				GetWorld()->GetTimerManager().SetTimer(npcOptimization_Timer, this, &UNpcDataProComponent::OptimizationTimer, rateOptimization_, true, rateOptimization_);
			}
		}
	}
	else
	{
		if (bShowErrorMessages)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("Error - The NPC is not registered because the World Director is not found in the scene."));
		}
		return;
	}
}

void UNpcDataProComponent::ExcludeActorFromOptimization()
{
	if (IsValid(directorNPCRef))
	{
		directorNPCRef->RemoveActorFormSystem(GetOwner());
	}
}

void UNpcDataProComponent::OptimizationTimer()
{
	if (!bIsActivate)
	{
		return;
	}

	TArray<UActorComponent*> allComponents = GetOwner()->GetComponentsByTag(UPrimitiveComponent::StaticClass(), componentsTag);

	bool bIsCameraSeeNPC_ = IsCameraSeeNPC();

	for (int i = 0; i < allComponents.Num(); ++i)
	{
		UPrimitiveComponent* primitiveComponent_ = Cast<UPrimitiveComponent>(allComponents[i]);
		if (primitiveComponent_)
		{
			primitiveComponent_->SetVisibility(bIsCameraSeeNPC_);
		}

		USkeletalMeshComponent* skeletalMesh_ = Cast<USkeletalMeshComponent>(allComponents[i]);
		if (skeletalMesh_)
		{
			if (bIsCameraSeeNPC_)
			{
				if (basedAnimTickOptionArr.Contains(skeletalMesh_))
				{
					skeletalMesh_->VisibilityBasedAnimTickOption = basedAnimTickOptionArr.FindRef(skeletalMesh_);
					basedAnimTickOptionArr.Remove(skeletalMesh_);
				}
				else
				{
					skeletalMesh_->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
				}
			}
			else
			{
				if (!basedAnimTickOptionArr.Contains(skeletalMesh_))
				{
					basedAnimTickOptionArr.Add(skeletalMesh_, skeletalMesh_->VisibilityBasedAnimTickOption);
				}

				skeletalMesh_->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
			}
		}
	}


	// Set Tick Interval for all components in actor.
	TArray<UActorComponent*> allTickComponents_;
	GetOwner()->GetComponents(allTickComponents_);

	if (allTickComponents_.Num() > 0 && bIsOptimizeAllActorComponentsTickInterval)
	{
		if (bIsCameraSeeNPC_)
		{
			for (int compID_ = 0; compID_ < allTickComponents_.Num(); ++compID_)
			{
				if (bIsDisableTickIfBehindCameraFOV)
				{
					if (!Cast<UNiagaraComponent>(allTickComponents_[compID_]))
					{
						allTickComponents_[compID_]->SetComponentTickEnabled(true);
					}

					ownerActor->SetActorTickEnabled(true);
				}
				else if (defaultTickComponentsInterval.IsValidIndex(compID_))
				{
					allTickComponents_[compID_]->SetComponentTickInterval(defaultTickComponentsInterval[compID_]);
					ownerActor->SetActorTickInterval(defaultTickActorInterval);
				}
			}
		}
		else
		{
			for (int compID_ = 0; compID_ < allTickComponents_.Num(); ++compID_)
			{
				if (bIsDisableTickIfBehindCameraFOV)
				{
					if (!Cast<UNiagaraComponent>(allTickComponents_[compID_]))
					{
						allTickComponents_[compID_]->SetComponentTickEnabled(false);
					}

					ownerActor->SetActorTickEnabled(false);
				}
				else
				{
					allTickComponents_[compID_]->SetComponentTickInterval(hiddenTickComponentsInterval);
					ownerActor->SetActorTickInterval(hiddenTickActorInterval);
				}
			}
		}
	}

	if (bIsCameraSeeNPC_)
	{
		BroadcastInCameraFOV();
	}
	else
	{
		BroadcastBehindCameraFOV();
	}
}

bool UNpcDataProComponent::IsCameraSeeNPC() const
{
	bool bIsSee_ = true;

	APlayerCameraManager* playerCam_ = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (playerCam_)
	{
		AActor* playerActor_ = Cast<AActor>(playerCam_->GetOwningPlayerController());
		if (playerActor_)
		{
			if (playerCam_->GetDotProductTo(GetOwner()) < 1.f - playerCam_->GetFOVAngle() / 100.f &&
				(playerCam_->GetCameraLocation() - GetOwner()->GetActorLocation()).Size() > distanceCamara)
			{
				bIsSee_ = false;
			}
			else
			{
				bIsSee_ = true;
			}
		}
	}

	return bIsSee_;
}


// Called every frame
void UNpcDataProComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UNpcDataProComponent::BroadcastOnPrepareForOptimization() const
{
	OnPrepareForOptimization.Broadcast();
}

void UNpcDataProComponent::BroadcastOnRecoveryFromOptimization() const
{
	OnRecoveryFromOptimization.Broadcast();
}

void UNpcDataProComponent::BroadcastBehindCameraFOV() const
{
	EventBehindCameraFOV.Broadcast();
}

void UNpcDataProComponent::BroadcastInCameraFOV() const
{
	EventInCameraFOV.Broadcast();
}

void UNpcDataProComponent::SetNpcUniqName(FString setName)
{
	npcUniqName = setName;
}

FString UNpcDataProComponent::GetNpcUniqName()
{
	return npcUniqName;
}
