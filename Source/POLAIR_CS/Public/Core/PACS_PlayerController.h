#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Components/PACS_InputHandlerComponent.h"
#include "Data/PACS_InputTypes.h"
#include "Core/PACS_PlayerState.h"
#include "Engine/TimerHandle.h"
#include "Components/PACS_EdgeScrollComponent.h"
#include "Components/PACS_HoverProbeComponent.h"
#include "PACS_PlayerController.generated.h"

UCLASS()
class POLAIR_CS_API APACS_PlayerController : public APlayerController, public IPACS_InputReceiver
{
    GENERATED_BODY()

public:
    APACS_PlayerController();

#pragma region Lifecycle
protected:
    virtual void SetupInputComponent() override;
    virtual void BeginPlay() override;
    virtual void PostInitializeComponents() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void Tick(float DeltaTime) override;
    virtual void InitPlayerState() override;
#pragma endregion

#pragma region VR/HMD Management
public:
    // Per-player timer handle for HMD detection timeout
    FTimerHandle HMDWaitHandle;

    // Allow test access to these members
    EHMDState PendingHMDState = EHMDState::Unknown;
    bool bHasPendingHMDState = false;

    // Client RPC for zero-swap handshake
    UFUNCTION(Client, Reliable)
    void ClientRequestHMDState();
    void ClientRequestHMDState_Implementation();

    // Server RPC response for zero-swap handshake
    UFUNCTION(Server, Reliable)
    void ServerReportHMDState(EHMDState DetectedState);
    void ServerReportHMDState_Implementation(EHMDState DetectedState);

    // VR integration delegate handles
    FDelegateHandle OnPutOnHandle, OnRemovedHandle, OnRecenterHandle;
    void HandleHMDPutOn();
    void HandleHMDRemoved();
    void HandleHMDRecenter();

#pragma endregion

#pragma region Player Identity
public:
    // Server RPC for PlayFab player name transmission
    UFUNCTION(Server, Reliable)
    void ServerSetPlayFabPlayerName(const FString& PlayerName);
    void ServerSetPlayFabPlayerName_Implementation(const FString& PlayerName);
#pragma endregion

#pragma region Input System
public:
    // Timer handle for InputConfig retry binding
    FTimerHandle InputConfigRetryTimer;

    // Debug input context display
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug")
    bool bShowInputContextDebug = false;

    // Trace channels for selection and movement
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input|Trace Channels",
        meta=(DisplayName="Selection Trace Channel"))
    TEnumAsByte<ECollisionChannel> SelectionTraceChannel = ECC_Visibility;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input|Trace Channels",
        meta=(DisplayName="Movement Trace Channel"))
    TEnumAsByte<ECollisionChannel> MovementTraceChannel = ECC_Visibility;

    // Called by InputHandler when initialization completes
    void BindInputActions();

    // IPACS_InputReceiver interface
    virtual EPACS_InputHandleResult HandleInputAction(FName ActionName, const FInputActionValue& Value) override;
    virtual int32 GetInputPriority() const override { return PACS_InputPriority::Gameplay; }

    // Component getters
    UPACS_InputHandlerComponent* GetInputHandler() const { return InputHandler; }

    UFUNCTION(BlueprintCallable, Category="Input")
    UPACS_EdgeScrollComponent* GetEdgeScrollComponent() const { return EdgeScrollComponent; }

    UFUNCTION(BlueprintCallable, Category="Selection")
    UPACS_HoverProbeComponent* GetHoverProbe() const { return HoverProbe; }

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Input",
        meta=(AllowPrivateAccess="true"))
    TObjectPtr<UPACS_InputHandlerComponent> InputHandler;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Input",
        meta=(AllowPrivateAccess="true"))
    TObjectPtr<UPACS_EdgeScrollComponent> EdgeScrollComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Selection",
        meta=(AllowPrivateAccess="true"))
    TObjectPtr<UPACS_HoverProbeComponent> HoverProbe;

    void ValidateInputSystem();
    void DisplayInputContextDebug();
#pragma endregion

#pragma region Selection System
public:
    // Selection System RPCs
    UFUNCTION(Server, Reliable)
    void ServerRequestSelect(AActor* TargetActor);

    UFUNCTION(Server, Reliable)
    void ServerRequestDeselect();
#pragma endregion
};