#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PACS_InputHandlerComponent.h"
#include "PACS_PlayerController.generated.h"

UCLASS()
class POLAIR_CS_API APACS_PlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    APACS_PlayerController();

protected:
    virtual void SetupInputComponent() override;
    virtual void BeginPlay() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Input", 
        meta=(AllowPrivateAccess="true"))
    TObjectPtr<UPACS_InputHandlerComponent> InputHandler;

    void BindInputActions();
    void ValidateInputSystem();
};