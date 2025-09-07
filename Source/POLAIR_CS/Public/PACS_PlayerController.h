#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PACS_InputHandlerComponent.h"
#include "PACS_PlayerState.h"
#include "Engine/TimerHandle.h"
#include "PACS_PlayerController.generated.h"

UCLASS()
class POLAIR_CS_API APACS_PlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    APACS_PlayerController();

    // Per-player timer handle for HMD detection timeout
    FTimerHandle HMDWaitHandle;

    // Client RPC for zero-swap handshake
    UFUNCTION(Client, Reliable)
    void ClientRequestHMDState();
    void ClientRequestHMDState_Implementation();

public:
    // Allow test access to these members
    EHMDState PendingHMDState = EHMDState::Unknown;
    bool bHasPendingHMDState = false;

    // Server RPC response for zero-swap handshake
    UFUNCTION(Server, Reliable)
    void ServerReportHMDState(EHMDState DetectedState);
    void ServerReportHMDState_Implementation(EHMDState DetectedState);

    // Override for server-side PlayerState initialization
    virtual void InitPlayerState() override;

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