// Copyright 2025 BitProtectStudio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/UserDefinedStruct.h"

#include "Animation/AnimTypes.h"
#include "Components/SkeletalMeshComponent.h"

#include "NpcDataProComponent.generated.h"

USTRUCT(BlueprintType)
struct FNpcDataPro
{
	GENERATED_USTRUCT_BODY()

	// If pawn or character class.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director PRO Parameters")
	bool bIsWander = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director PRO Parameters")
	bool bIsRestoreOriginSpawnLocation = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director NPC Parameters")
	float correctSpawnAxisZ = 80.f;
	// If pawn or character class.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director PRO Parameters")
	float wanderRadius = 3000.f;
	// If pawn or character class.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director PRO Parameters")
	float npcSpeed = 600.f;
	// If pawn or character class.
	UPROPERTY(BlueprintReadOnly, Category = "Director NPC Struct", meta=(ClampMin="0.1"))
	float delayTimeFindLocation = 3.f;
	// If pawn or character class.
	UPROPERTY(BlueprintReadWrite, Category = "Director PRO Parameters")
	FVector targetLocation = FVector::ZeroVector;

	bool bIsPawnClass = false;

	// Override global layers in Director.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director PRO Parameters")
	bool bIsOverrideLayers = false;
	// Override global layers in Director.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director PRO Parameters")
	float firstLayerRadius = 3000.f;
	UPROPERTY()
	float firstLayerRadiusSquare = 0.f;
	UPROPERTY()
	float mainLayerRadiusSquare = 0.f;
	// Override global layers in Director.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director PRO Parameters")
	float secondLayerRadius = 15000.f;
	UPROPERTY()
	float secondLayerRadiusSquare = 0.f;
	// Override global layers in Director.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director PRO Parameters")
	float thirdLayerRadius = 30000.f;
	UPROPERTY()
	float thirdLayerRadiusSquare = 0.f;
	// Override global layers in Director.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director PRO Parameters")
	float layerOffset = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director PRO Parameters")
	bool bRandomYawRotationOnSpawn = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director PRO Parameters")
	bool bRandomRollRotationOnSpawn = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director PRO Parameters")
	bool bRandomPitchRotationOnSpawn = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director NPC Parameters")
	class UStaticMesh* staticMesh = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director NPC Parameters")
	FVector pivotOffsetLocation = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director NPC Parameters")
	FRotator pivotOffsetRotation = FRotator::ZeroRotator;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director NPC Parameters")
	float maxDistanceSearchRoad = 2000.f;
	UPROPERTY()
	float maxDistanceSearchRoadSquare = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director NPC Parameters")
	bool bUseRoads = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director NPC Parameters")
	bool bUseInterestingPlaces = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director NPC Parameters")
	TArray<FName> accessibleRoadsArr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director NPC Parameters")
	TArray<FName> accessiblePlacesArr;

	FNpcDataPro()
	{
	};
};

UCLASS(Blueprintable)
class WORLDDIRECTORPRO_API UNpcDataProComponent : public UActorComponent
{
	GENERATED_BODY()

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPrepareForOptimization);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRecoveryFromOptimization);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBehindCameraFOV);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInCameraFOV);

public:
	// Sets default values for this component's properties
	UNpcDataProComponent();

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Broadcasts notification
	void BroadcastOnPrepareForOptimization() const;
	// Broadcasts notification
	void BroadcastOnRecoveryFromOptimization() const;

	// Broadcasts notification
	void BroadcastBehindCameraFOV() const;
	// Broadcasts notification
	void BroadcastInCameraFOV() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Director PRO Parameters")
	void PrepareForOptimizationBP();
	UFUNCTION(BlueprintImplementableEvent, Category = "Director PRO Parameters")
	void RecoveryFromOptimization();

	UFUNCTION(BlueprintCallable, Category = "Director PRO Parameters")
	void InitializeNPC();
	UFUNCTION(BlueprintCallable, Category = "Director PRO Parameters")
	void ExcludeActorFromOptimization();

	void SetNpcUniqName(FString setName);
	FString GetNpcUniqName();

	// Delegate
	UPROPERTY(BlueprintAssignable)
	FPrepareForOptimization OnPrepareForOptimization;
	UPROPERTY(BlueprintAssignable)
	FRecoveryFromOptimization OnRecoveryFromOptimization;

	// Delegate
	UPROPERTY(BlueprintAssignable)
	FPrepareForOptimization EventBehindCameraFOV;
	UPROPERTY(BlueprintAssignable)
	FRecoveryFromOptimization EventInCameraFOV;

	// Set Enable Director Actor Component.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director PRO Parameters")
	bool bIsActivate = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director PRO Parameters")
	bool bIsOptimizeAllActorComponentsTickInterval = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director PRO Parameters")
	bool bIsDisableTickIfBehindCameraFOV = false;

	// Enable this if used Population Control System.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director NPC Parameters")
	bool bPopulationControlSupport = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director PRO Parameters")
	FNpcDataPro npcData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director NPC Parameters")
	bool bShowErrorMessages = true;

	UPROPERTY(BlueprintReadWrite, Category = "Director PRO Parameters")
	FVector npcSpawnLocation;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	void OptimizationTimer();

	bool IsCameraSeeNPC() const;


private:
	TArray<float> defaultTickComponentsInterval;
	float hiddenTickComponentsInterval;

	float defaultTickActorInterval;
	float hiddenTickActorInterval;

	float distanceCamara = 1000.f;

	FName componentsTag = "DNPC";

	UPROPERTY()
	class AWorldDirectorNpcPRO* directorNPCRef;

	FTimerHandle registerNpc_Timer;

	bool bIsRegistered = false;

	FTimerHandle npcOptimization_Timer;

	UPROPERTY()
	class UPawnMovementComponent* movementComponent = nullptr;

	UPROPERTY()
	AActor* ownerActor = nullptr;

	FString npcUniqName = "";

	UPROPERTY()
	TMap<USkeletalMeshComponent*, EVisibilityBasedAnimTickOption> basedAnimTickOptionArr;
};
