#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "PACS_HoverProbeComponent.generated.h"

class APACS_NPCCharacter;
class APACS_PlayerController;
class UInputMappingContext;

/**
 * Lean local-only hover probe component for NPCs
 * Runs at 30Hz, attaches to PlayerController, with input context gating
 * Epic pattern: Component-based hover detection with robust cleanup
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class POLAIR_CS_API UPACS_HoverProbeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPACS_HoverProbeComponent();

	// Input mapping contexts that allow hover probe to be active
	// If empty, hover probe is always active (legacy behavior)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hover",
		meta=(DisplayName="Active Input Contexts",
				ToolTip="Hover probe only works when one of these input contexts is active. Leave empty to always be active."))
	TArray<TObjectPtr<UInputMappingContext>> ActiveInputContexts;

	// Read-only status for debugging
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hover",
		meta=(DisplayName="Currently Active"))
	bool bIsCurrentlyActive = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hover", meta=(ClampMin="1", ClampMax="240"))
	float RateHz = 30.0f; // ~30 Hz

	// Only hover when line of sight is clear (prevents hover through walls)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hover")
	bool bConfirmVisibility = true;

	// Which object types count as "hover targets" (set HoverBox to this object type in editor)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hover")
	TArray<TEnumAsByte<EObjectTypeQuery>> HoverObjectTypes;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	TWeakObjectPtr<APACS_PlayerController> OwnerPC;
	TWeakObjectPtr<APACS_NPCCharacter> CurrentNPC;

	// Input context monitoring for automatic cleanup
	FDelegateHandle InputContextHandle;

	// Current context tracking
	TWeakObjectPtr<UInputMappingContext> CurrentActiveContext;
	bool bWasActiveLastFrame = true;

	void ProbeOnce();
	void ClearHover();

	// Cleanup handlers
	UFUNCTION()
	void OnNPCDestroyed(AActor* DestroyedActor);
	void OnInputContextChanged();

	// Context monitoring
	bool IsInputContextActive() const;
	UInputMappingContext* GetCurrentActiveContext() const;

	// Resolve NPC from a hit result (box may be child of NPC)
	APACS_NPCCharacter* ResolveNPCFrom(const FHitResult& Hit) const;

	// Safe cleanup helpers
	void UnbindNPCDelegates();
	void UnbindInputDelegates();
};