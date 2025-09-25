#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Data/PACS_InputTypes.h"
#include "PACS_InputMappingConfig.generated.h"

USTRUCT(BlueprintType)
struct FPACS_InputActionMapping
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input", 
        meta=(DisplayName="Input Action"))
    TObjectPtr<UInputAction> InputAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input",
        meta=(DisplayName="Action Name"))
    FName ActionIdentifier;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    bool bBindStarted = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    bool bBindTriggered = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    bool bBindCompleted = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    bool bBindOngoing = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
    bool bBindCanceled = false;

    FPACS_InputActionMapping()
        : ActionIdentifier(NAME_None)
    {}
};

UCLASS(BlueprintType, Category="Input")
class POLAIR_CS_API UPACS_InputMappingConfig : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Action Mapping", 
        meta=(TitleProperty="ActionIdentifier"))
    TArray<FPACS_InputActionMapping> ActionMappings;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Contexts", 
        meta=(DisplayName="Gameplay Context"))
    TObjectPtr<UInputMappingContext> GameplayContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Contexts",
        meta=(DisplayName="Menu Context"))
    TObjectPtr<UInputMappingContext> MenuContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Contexts",
        meta=(DisplayName="UI Context"))
    TObjectPtr<UInputMappingContext> UIContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI Protection",
        meta=(DisplayName="Actions Blocked by UI"))
    TArray<FName> UIBlockedActions = { 
        TEXT("Move"),
        TEXT("Look"),
        TEXT("Jump"),
        TEXT("Fire"),
        TEXT("Interact")
    };

    bool IsValid() const
    {
        return GameplayContext != nullptr && 
               MenuContext != nullptr && 
               ActionMappings.Num() > 0 &&
               ActionMappings.Num() <= PACS_InputLimits::MaxActionsPerConfig;
    }

    FName GetActionIdentifier(const UInputAction* InputAction) const;
};