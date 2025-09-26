#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interfaces/PACS_Poolable.h"
#include "PACS_NPC_Base_Char.generated.h"

class UBoxComponent;
class UDecalComponent;

/**
 * Base Character class for humanoid NPCs in POLAIR_CS
 * Extends ACharacter with pooling support and selection system
 * Includes standard components: DecalComponent and BoxCollision
 */
UCLASS(Abstract)
class POLAIR_CS_API APACS_NPC_Base_Char : public ACharacter, public IPACS_Poolable
{
	GENERATED_BODY()

public:
	APACS_NPC_Base_Char();

protected:
	// AActor interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// IPACS_Poolable interface
	virtual void OnAcquiredFromPool_Implementation() override;
	virtual void OnReturnedToPool_Implementation() override;

public:
	// Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> BoxCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDecalComponent> SelectionDecal;

protected:
	// Selection state
	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	bool bIsSelected = false;

	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	TWeakObjectPtr<class APlayerState> CurrentSelector;

	// Movement state for pooling
	UPROPERTY()
	float DefaultMaxWalkSpeed = 600.0f;

public:
	// Selection interface
	UFUNCTION(BlueprintCallable, Category = "Selection")
	virtual void SetSelected(bool bNewSelected, class APlayerState* Selector = nullptr);

	UFUNCTION(BlueprintPure, Category = "Selection")
	bool IsSelected() const { return bIsSelected; }

	UFUNCTION(BlueprintPure, Category = "Selection")
	class APlayerState* GetCurrentSelector() const { return CurrentSelector.Get(); }

	// Movement control
	UFUNCTION(BlueprintCallable, Category = "NPC Movement")
	virtual void MoveToLocation(const FVector& TargetLocation);

	UFUNCTION(BlueprintCallable, Category = "NPC Movement")
	virtual void StopMovement();

protected:
	// Visual feedback
	virtual void UpdateSelectionVisuals();

	// Pool state management
	virtual void ResetForPool();
	virtual void PrepareForUse();

	// Character-specific reset
	virtual void ResetCharacterMovement();
	virtual void ResetCharacterAnimation();

private:
	// Default component setup
	void SetupDefaultDecal();
	void SetupDefaultCollision();
};