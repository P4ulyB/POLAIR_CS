// Copyright POLAIR_CS Project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/PACS_CharacterPool.h"
#include "PACS_NPCSpawnPoint.generated.h"

class UBillboardComponent;
class IPACS_SelectableCharacterInterface;

/**
 * Simple spawn point actor for NPCs
 * Place these in your level to mark where NPCs should spawn
 */
UCLASS(BlueprintType, Blueprintable)
class POLAIR_CS_API APACS_NPCSpawnPoint : public AActor
{
    GENERATED_BODY()

public:
    APACS_NPCSpawnPoint();

    /**
     * Character type to spawn at this location
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
    EPACSCharacterType CharacterType = EPACSCharacterType::Civilian;

    /**
     * Whether this spawn point is enabled
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
    bool bEnabled = true;

    /**
     * Optional specific spawn rotation (if zero, uses spawn point rotation)
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (MakeEditWidget = true))
    FRotator SpawnRotation = FRotator::ZeroRotator;

    /**
     * Get the spawned character at this point (if any)
     */
    UFUNCTION(BlueprintCallable, Category = "Spawn")
    TScriptInterface<IPACS_SelectableCharacterInterface> GetSpawnedCharacter() const;

    /**
     * Set the spawned character reference
     */
    void SetSpawnedCharacter(TScriptInterface<IPACS_SelectableCharacterInterface> Character);

    /**
     * Legacy getter for backward compatibility - will be removed
     */
    APACS_NPCCharacter* GetSpawnedCharacterLegacy() const;

protected:
    virtual void BeginPlay() override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
    UPROPERTY()
    TScriptInterface<IPACS_SelectableCharacterInterface> SpawnedCharacter;

#if WITH_EDITORONLY_DATA
    UPROPERTY()
    UBillboardComponent* SpriteComponent = nullptr;
#endif
};