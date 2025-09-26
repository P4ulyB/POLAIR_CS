// Copyright POLAIR_CS Project. All Rights Reserved.

#include "Actors/PACS_NPCSpawnPoint.h"
#include "Components/BillboardComponent.h"
#include "Engine/Texture2D.h"
#include "Interfaces/PACS_SelectableCharacterInterface.h"
#include "Actors/NPC/PACS_NPCCharacter.h"

APACS_NPCSpawnPoint::APACS_NPCSpawnPoint()
{
    PrimaryActorTick.bCanEverTick = false;

    // Create root component
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

#if WITH_EDITORONLY_DATA
    // Add billboard for editor visualization
    SpriteComponent = CreateDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
    if (SpriteComponent)
    {
        SpriteComponent->SetupAttachment(RootComponent);
        SpriteComponent->SetRelativeLocation(FVector(0, 0, 40));
        SpriteComponent->bHiddenInGame = true;

        // Try to set a default sprite texture
        static ConstructorHelpers::FObjectFinder<UTexture2D> SpriteTexture(TEXT("/Engine/EditorResources/S_NavP"));
        if (SpriteTexture.Succeeded())
        {
            SpriteComponent->SetSprite(SpriteTexture.Object);
        }
    }
#endif
}

void APACS_NPCSpawnPoint::BeginPlay()
{
    Super::BeginPlay();

    // Spawn points don't do anything on their own
    // The PACS_NPCSpawnManager will find and use them
}

#if WITH_EDITOR
void APACS_NPCSpawnPoint::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    if (PropertyChangedEvent.Property)
    {
        const FName PropertyName = PropertyChangedEvent.Property->GetFName();

        // Update editor label when character type changes
        if (PropertyName == GET_MEMBER_NAME_CHECKED(APACS_NPCSpawnPoint, CharacterType))
        {
            FString TypeString = UEnum::GetValueAsString(CharacterType);
            TypeString = TypeString.Replace(TEXT("EPACSCharacterType::"), TEXT(""));
            SetActorLabel(FString::Printf(TEXT("NPCSpawn_%s"), *TypeString));
        }
    }
}
#endif

// Interface methods implementation
TScriptInterface<IPACS_SelectableCharacterInterface> APACS_NPCSpawnPoint::GetSpawnedCharacter() const
{
    return SpawnedCharacter;
}

void APACS_NPCSpawnPoint::SetSpawnedCharacter(TScriptInterface<IPACS_SelectableCharacterInterface> Character)
{
    SpawnedCharacter = Character;
}

APACS_NPCCharacter* APACS_NPCSpawnPoint::GetSpawnedCharacterLegacy() const
{
    // Try to cast the interface to the legacy character type
    if (SpawnedCharacter.GetInterface())
    {
        if (APawn* Pawn = Cast<APawn>(SpawnedCharacter.GetObject()))
        {
            return Cast<APACS_NPCCharacter>(Pawn);
        }
    }
    return nullptr;
}