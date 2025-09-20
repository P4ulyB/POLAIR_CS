// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PACS_NPCBase.generated.h"

class APACS_SelectionCueProxy;
class UPACS_NPCArchetypeData;
class UPACS_SelectionGlobalConfig;
class UPACS_SelectionLocalConfig;

/**
 * Thin NPC character shell with archetype-driven appearance.
 * No selection state here; selection visuals live on an assessor-only proxy.
 */
UCLASS()
class POLAIR_CS_API APACS_NPCBase : public ACharacter
{
	GENERATED_BODY()

public:
	APACS_NPCBase();

	// Begin AActor
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	// End AActor

	/** What this NPC is (mesh/materials/anim/speeds), soft ref */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Config")
	TSoftObjectPtr<UPACS_NPCArchetypeData> Archetype;

	/** Optional per-actor selection override to feed the proxy */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Selection")
	TSoftObjectPtr<UPACS_SelectionLocalConfig> SelectionLocalOverride;

	/** Optional global selection config to feed the proxy (can also be provided by a subsystem) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Selection")
	TSoftObjectPtr<UPACS_SelectionGlobalConfig> SelectionGlobalConfig;

protected:
	/** Assessor-only visual proxy (server spawns; clients receive by relevance) */
	TWeakObjectPtr<APACS_SelectionCueProxy> SelectionProxy;

private:
	void ApplyArchetype(); // sets mesh/material/anim/walk speed
};