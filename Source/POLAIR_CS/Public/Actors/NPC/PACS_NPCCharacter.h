#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Data/PACS_NPCVisualConfig.h"
#include "PACS_NPCCharacter.generated.h"

class UPACS_NPCConfig;
class UBoxComponent;
class UDecalComponent;
class APlayerState;
struct FStreamableHandle;

UCLASS(BlueprintType, Blueprintable)
class POLAIR_CS_API APACS_NPCCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APACS_NPCCharacter();

	// Selection System - Server-authoritative ownership
	UPROPERTY(ReplicatedUsing = OnRep_CurrentSelector, BlueprintReadOnly, Category = "Selection")
	TObjectPtr<APlayerState> CurrentSelector = nullptr;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC", meta = (DisplayName = "NPC Configuration"))
	TObjectPtr<UPACS_NPCConfig> NPCConfigAsset;

	UPROPERTY(ReplicatedUsing = OnRep_VisualConfig, meta = (RepNotifyCondition = "REPNOTIFY_OnChanged"))
	FPACS_NPCVisualConfig VisualConfig;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NPC|Collision")
	TObjectPtr<UBoxComponent> CollisionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NPC|Collision")
	TObjectPtr<UDecalComponent> CollisionDecal;

	// Cache dynamic material instance for direct hover access
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> CachedDecalMaterial;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreInitializeComponents() override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Local-only hover methods (no replication)
	UFUNCTION(BlueprintCallable, Category="Selection")
	void SetLocalHover(bool bHovered);

	UFUNCTION(BlueprintCallable, Category="Selection")
	bool IsLocallyHovered() const { return bIsLocallyHovered; }

	// Selection System Methods
	UFUNCTION(BlueprintCallable, Category="Selection")
	bool IsSelected() const { return CurrentSelector != nullptr; }

	UFUNCTION(BlueprintCallable, Category="Selection")
	bool IsSelectedBy(APlayerState* InPlayerState) const { return CurrentSelector == InPlayerState; }

	// Getter for collision decal component
	UFUNCTION(BlueprintCallable, Category="NPC")
	UDecalComponent* GetCollisionDecal() const { return CollisionDecal; }

	// Getter for collision box component
	UFUNCTION(BlueprintCallable, Category="NPC")
	UBoxComponent* GetCollisionBox() const { return CollisionBox; }

	// Movement System - Server RPC for right-click to move
	UFUNCTION(Server, Reliable)
	void ServerMoveToLocation(FVector TargetLocation);

	// Pooling support
	void SetIsPooledCharacter(bool bPooled) { bIsPooledCharacter = bPooled; }
	bool IsPooledCharacter() const { return bIsPooledCharacter; }

	void SetVisualsApplied(bool bApplied) { bVisualsApplied = bApplied; }
	void SetCachedDecalMaterial(UMaterialInstanceDynamic* Material) { CachedDecalMaterial = Material; }

	// Getter for NPCConfigAsset - needed by CharacterPool
	UPACS_NPCConfig* GetNPCConfigAsset() const { return NPCConfigAsset; }

	// Performance optimization
	virtual void Tick(float DeltaTime) override;
	void UpdateDistanceBasedOptimizations();

protected:
	UFUNCTION()
	void OnRep_VisualConfig();

	UFUNCTION()
	void OnRep_CurrentSelector();

	void ApplyVisuals_Client();
	void BuildVisualConfigFromAsset_Server();
	void ApplyCollisionFromMesh();
	void ApplyGlobalSelectionSettings();
	void UpdateVisualState();

private:
	bool bVisualsApplied = false;
	bool bIsLocallyHovered = false;
	bool bIsPooledCharacter = false;
	TSharedPtr<FStreamableHandle> AssetLoadHandle;

	// Movement tracking for dedicated server performance
	bool bIsMoving = false;
	float LastMovementTime = 0.0f;
	float MovementTimeoutDuration = 2.0f;

	// Visual state priority system
	enum class EVisualPriority : uint8
	{
		Available = 0,
		Hovered = 1,
		Unavailable = 2,
		Selected = 3  // Highest priority
	};

	EVisualPriority GetCurrentVisualPriority() const;
	void CheckMovementCompletion(float DeltaTime);
};