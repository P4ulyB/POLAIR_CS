#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PACS_InputHandlerComponent.h"
#include "PACS_InputTypes.h"
#include "PACS_PlayerState.h"
#include "Engine/TimerHandle.h"
#include "PACS_PlayerController.generated.h"

UCLASS()
class POLAIR_CS_API APACS_PlayerController : public APlayerController, public IPACS_InputReceiver
{
    GENERATED_BODY()

public:
    APACS_PlayerController();

    // Per-player timer handle for HMD detection timeout
    FTimerHandle HMDWaitHandle;
    
    // Timer handle for InputConfig retry binding
    FTimerHandle InputConfigRetryTimer;

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

    // VR integration delegate handles
    FDelegateHandle OnPutOnHandle, OnRemovedHandle, OnRecenterHandle;
    void HandleHMDPutOn();
    void HandleHMDRemoved();
    void HandleHMDRecenter();

    // Override for server-side PlayerState initialization
    virtual void InitPlayerState() override;

    // Debug input context display
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug")
    bool bShowInputContextDebug = false;

protected:
    virtual void SetupInputComponent() override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void Tick(float DeltaTime) override;

public:
    // Called by InputHandler when initialization completes
    void BindInputActions();
    
    // IPACS_InputReceiver interface - TEST IMPLEMENTATION
    virtual EPACS_InputHandleResult HandleInputAction(FName ActionName, const FInputActionValue& Value) override;
    virtual int32 GetInputPriority() const override { return PACS_InputPriority::Gameplay; }

    // Getter for InputHandler component
    UPACS_InputHandlerComponent* GetInputHandler() const { return InputHandler; }

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Input",
        meta=(AllowPrivateAccess="true"))
    TObjectPtr<UPACS_InputHandlerComponent> InputHandler;
    void ValidateInputSystem();
    void DisplayInputContextDebug();
};