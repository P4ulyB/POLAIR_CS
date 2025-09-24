// Copyright POLAIR_CS Project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PACS_NPCSpawnPoint.generated.h"

class UBillboardComponent;
class APACS_NPCCharacter;

/**
 * Character type identifier for spawn points
 * Duplicated here to avoid circular dependency with CharacterPool
 */
UENUM(BlueprintType)
enum class EPACSSpawnCharacterType : uint8
{
    Civilian = 0,
    Police,
    Firefighter,
    Paramedic,
    MAX UMETA(Hidden)
};

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
    EPACSSpawnCharacterType CharacterType = EPACSSpawnCharacterType::Civilian;

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
    APACS_NPCCharacter* GetSpawnedCharacter() const { return SpawnedCharacter; }

    /**
     * Set the spawned character reference
     */
    void SetSpawnedCharacter(APACS_NPCCharacter* Character) { SpawnedCharacter = Character; }

protected:
    virtual void BeginPlay() override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
    UPROPERTY()
    APACS_NPCCharacter* SpawnedCharacter = nullptr;

#if WITH_EDITORONLY_DATA
    UPROPERTY()
    UBillboardComponent* SpriteComponent = nullptr;
#endif
};