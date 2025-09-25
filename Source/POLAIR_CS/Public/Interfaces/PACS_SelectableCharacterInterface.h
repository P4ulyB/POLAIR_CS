// Copyright Notice: Property of Particular Audience Limited

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PACS_SelectableCharacterInterface.generated.h"

UINTERFACE(MinimalAPI)
class UPACS_SelectableCharacterInterface : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for selectable characters (both heavyweight and lightweight NPCs)
 * Allows existing systems to work with either NPC type
 */
class POLAIR_CS_API IPACS_SelectableCharacterInterface
{
    GENERATED_BODY()

public:
    // Selection System
    virtual class APlayerState* GetCurrentSelector() const = 0;
    virtual void SetCurrentSelector(class APlayerState* Selector) = 0;
    virtual bool IsSelectedBy(class APlayerState* PlayerState) const = 0;

    // Movement
    virtual void MoveToLocation(const FVector& TargetLocation) = 0;
    virtual bool IsMoving() const = 0;

    // Hover/Highlight
    virtual void SetLocalHover(bool bHovered) = 0;
    virtual bool IsLocallyHovered() const = 0;

    // Visual Components
    virtual class UMeshComponent* GetMeshComponent() const = 0;
    virtual class UDecalComponent* GetSelectionDecal() const = 0;
};