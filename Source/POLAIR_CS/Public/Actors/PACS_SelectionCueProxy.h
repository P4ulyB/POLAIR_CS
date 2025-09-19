// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PACS_SelectionCueProxy.generated.h"

class UDecalComponent;
class UMaterialInstanceDynamic;
class APACS_PlayerState;

// Forward decls for data assets
class UPACS_SelectionGlobalConfig;
class UPACS_SelectionLocalConfig;
struct FSelectionVisualSet;
struct FSelectionDecalParams;

/**
 * Visual proxy actor for NPC selection feedback
 * Only visible to assessors (filtered via IsNetRelevantFor)
 */
UCLASS()
class POLAIR_CS_API APACS_SelectionCueProxy : public AActor
{
	GENERATED_BODY()

public:
	APACS_SelectionCueProxy();

	// Begin AActor
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;
	// End AActor

	/** Replicated selection state (0 = free, otherwise short player id) */
	UPROPERTY(ReplicatedUsing=OnRep_SelectedBy)
	uint16 SelectedById = 0;

	/** Server RPC to set selection owner (validate inside body) */
	UFUNCTION(Server, Reliable)
	void ServerSetSelectedBy(uint16 NewOwnerId);

	/** Decal component (projected to floor) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UDecalComponent> DecalComponent;

	/** Set local hover state (not replicated) */
	UFUNCTION(BlueprintCallable, Category="Selection")
	void SetLocalHovered(bool bHovered);

	/** Update visual material/size for current viewer */
	void UpdateVisualForViewer();

	/** Optional: snap the decal to the ground under this actor */
	void SnapToFloor();

	/** Global config reference (soft) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Config")
	TSoftObjectPtr<UPACS_SelectionGlobalConfig> GlobalCfg;

	/** Local per-actor override (soft) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Config")
	TSoftObjectPtr<UPACS_SelectionLocalConfig> LocalCfg;

protected:
	/** Local hover state (not replicated) */
	bool bIsLocallyHovered = false;

	/** Replication callback */
	UFUNCTION()
	void OnRep_SelectedBy();

	/** Access the chosen visual set (local override or global) */
	const FSelectionVisualSet& GetVisualSet() const;

	/** Apply parameters to the decal (MID + DecalSize) */
	void ApplyDecalParams(const FSelectionDecalParams& Params);

private:
	/** MID for dynamic parameter updates */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> MID;
};