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

    // Server RPC for PlayFab player name transmission
    UFUNCTION(Server, Reliable)
    void ServerSetPlayFabPlayerName(const FString& PlayerName);
    void ServerSetPlayFabPlayerName_Implementation(const FString& PlayerName);

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

    // Trace channels for selection and movement
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input|Trace Channels",
        meta=(DisplayName="Selection Trace Channel"))
    TEnumAsByte<ECollisionChannel> SelectionTraceChannel = ECC_Visibility;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input|Trace Channels",
        meta=(DisplayName="Movement Trace Channel"))
    TEnumAsByte<ECollisionChannel> MovementTraceChannel = ECC_Visibility;

protected:
    virtual void SetupInputComponent() override;
    virtual void BeginPlay() override;
    virtual void PostInitializeComponents() override;
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

    // Getter for EdgeScrollComponent
    UFUNCTION(BlueprintCallable, Category="Input")
    UPACS_EdgeScrollComponent* GetEdgeScrollComponent() const { return EdgeScrollComponent; }

    // Getter for HoverProbe component
    UFUNCTION(BlueprintCallable, Category="Selection")
    UPACS_HoverProbeComponent* GetHoverProbe() const { return HoverProbe; }

    // VR Decal Visibility Control
    UFUNCTION(BlueprintCallable, Category="VR")
    void UpdateNPCDecalVisibility(bool bIsVRClient);

    // Selection System RPCs
    UFUNCTION(Server, Reliable)
    void ServerRequestSelect(AActor* TargetActor);

    UFUNCTION(Server, Reliable)
    void ServerRequestDeselect();

    // NPC Movement RPC
    UFUNCTION(Server, Reliable)
    void ServerRequestNPCMove(APACS_NPCCharacter* TargetNPC, FVector TargetLocation);

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
};