#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "PACS_PlayerState.generated.h"

UENUM(BlueprintType)
enum class EHMDState : uint8
{
    Unknown = 0,
    NoHMD   = 1,
    HasHMD  = 2
};

UCLASS()
class POLAIR_CS_API APACS_PlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    APACS_PlayerState();

    UPROPERTY(ReplicatedUsing=OnRep_HMDState, BlueprintReadOnly, Category="PACS")
    EHMDState HMDState = EHMDState::Unknown;

    UPROPERTY(ReplicatedUsing=OnRep_IsAssessor, BlueprintReadOnly, Category="PACS")
    bool bIsAssessor = false;

    UFUNCTION() void OnRep_HMDState();
    UFUNCTION() void OnRep_IsAssessor();

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
};
