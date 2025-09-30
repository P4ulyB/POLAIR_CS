#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "PACS_SpawnPoint.generated.h"

class UPACS_SpawnOrchestrator;
class UBillboardComponent;
class UArrowComponent;

/**
 * Spawn pattern types for spawn points
 */
UENUM(BlueprintType)
enum class ESpawnPattern : uint8
{
	Immediate    UMETA(DisplayName = "Immediate"),
	Delayed      UMETA(DisplayName = "Delayed"),
	Wave         UMETA(DisplayName = "Wave"),
	Manual       UMETA(DisplayName = "Manual Only")
	// Note: Continuous respawn removed - incompatible with pooling architecture
	// (pooled actors aren't destroyed, so respawn trigger doesn't exist)
};

/**
 * Placeable spawn point actor for level designers
 * Requests spawns from the orchestrator on BeginPlay (server only)
 */
UCLASS(Blueprintable, ClassGroup=(Spawning), meta=(BlueprintSpawnableComponent))
class POLAIR_CS_API APACS_SpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	APACS_SpawnPoint();

protected:
	// AActor interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:
	// Manual spawn control
	UFUNCTION(BlueprintCallable, Category = "Spawn Point", meta = (CallInEditor = "true"))
	AActor* SpawnActor();

	UFUNCTION(BlueprintCallable, Category = "Spawn Point")
	void DespawnActor();

	UFUNCTION(BlueprintCallable, Category = "Spawn Point")
	void StartSpawning();

	UFUNCTION(BlueprintCallable, Category = "Spawn Point")
	void StopSpawning();

	// Getters
	UFUNCTION(BlueprintPure, Category = "Spawn Point")
	AActor* GetSpawnedActor() const { return SpawnedActor; }

	UFUNCTION(BlueprintPure, Category = "Spawn Point")
	bool IsSpawnActive() const { return bIsSpawnActive; }

	UFUNCTION(BlueprintPure, Category = "Spawn Point")
	FGameplayTag GetSpawnTag() const { return SpawnTag; }

protected:
	// Spawn configuration
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn Configuration", meta = (Categories = "PACS.Spawn"))
	FGameplayTag SpawnTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn Configuration")
	ESpawnPattern SpawnPattern = ESpawnPattern::Immediate;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn Configuration", meta = (EditCondition = "SpawnPattern == ESpawnPattern::Delayed", EditConditionHides, ClampMin = 0.1, ClampMax = 60.0))
	float SpawnDelay = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn Configuration", meta = (EditCondition = "SpawnPattern == ESpawnPattern::Wave", EditConditionHides, ClampMin = 1, ClampMax = 20))
	int32 WaveCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn Configuration", meta = (EditCondition = "SpawnPattern == ESpawnPattern::Wave", EditConditionHides, ClampMin = 0.1, ClampMax = 10.0))
	float WaveInterval = 2.0f;

	// Auto-start spawning on BeginPlay
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn Configuration")
	bool bAutoStart = true;

	// Use spawn point's transform for spawned actors
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn Configuration")
	bool bUseSpawnPointTransform = true;

	// Spawn transform offset (relative to spawn point)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn Configuration", meta = (EditCondition = "bUseSpawnPointTransform"))
	FTransform SpawnTransformOffset;

	// Override spawn parameters
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn Configuration")
	bool bOverrideOwner = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn Configuration", meta = (EditCondition = "bOverrideOwner"))
	AActor* SpawnOwner = nullptr;

	// Visual components
#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleDefaultsOnly, Category = "Components")
	TObjectPtr<UBillboardComponent> BillboardComponent;

	UPROPERTY(VisibleDefaultsOnly, Category = "Components")
	TObjectPtr<UArrowComponent> ArrowComponent;
#endif

private:
	// Internal spawn logic
	void ExecuteSpawn();
	void ScheduleNextSpawn();
	void HandleWaveSpawn();
	void CheckOrchestratorReady();

	// Get spawn parameters
	FTransform GetSpawnTransform() const;

	// State tracking
	UPROPERTY(Transient)
	TObjectPtr<AActor> SpawnedActor;

	UPROPERTY(Transient)
	TObjectPtr<UPACS_SpawnOrchestrator> SpawnOrchestrator;

	bool bIsSpawnActive = false;
	int32 CurrentWaveCount = 0;

	// Timer handles
	FTimerHandle SpawnTimerHandle;
	FTimerHandle WaveTimerHandle;
	FTimerHandle ReadyCheckTimerHandle;
};