#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Components/PACS_InputHandlerComponent.h"
#include "Data/PACS_InputTypes.h"
#include "Core/PACS_PlayerState.h"
#include "Engine/TimerHandle.h"
#include "Components/PACS_EdgeScrollComponent.h"
#include "Components/PACS_HoverProbeComponent.h"
#include "GameplayTagContainer.h"
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
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PACS|Debug")
    bool bShowInputContextDebug = false;

    // Trace channels for selection and movement
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PACS|Input|Trace Channels",
        meta=(DisplayName="Selection Trace Channel"))
    TEnumAsByte<ECollisionChannel> SelectionTraceChannel = ECC_Visibility;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PACS|Input|Trace Channels",
        meta=(DisplayName="Movement Trace Channel"))
    TEnumAsByte<ECollisionChannel> MovementTraceChannel = ECC_Visibility;

    // Called by InputHandler when initialization completes
    void BindInputActions();

    // IPACS_InputReceiver interface
    virtual EPACS_InputHandleResult HandleInputAction(FName ActionName, const FInputActionValue& Value) override;
    virtual int32 GetInputPriority() const override { return PACS_InputPriority::Gameplay; }

    // Component getters
    UPACS_InputHandlerComponent* GetInputHandler() const { return InputHandler; }

    UFUNCTION(BlueprintCallable, Category="PACS|Input")
    UPACS_EdgeScrollComponent* GetEdgeScrollComponent() const { return EdgeScrollComponent; }

    UFUNCTION(BlueprintCallable, Category="PACS|Selection")
    UPACS_HoverProbeComponent* GetHoverProbe() const { return HoverProbe; }

    // HoverProbe configuration (applied after runtime creation)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PACS|Selection|HoverProbe",
        meta=(DisplayName="Hover Active Input Contexts",
              ToolTip="Input contexts that enable hover detection. Leave empty for always active."))
    TArray<TObjectPtr<UInputMappingContext>> HoverProbeActiveContexts;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PACS|Selection|HoverProbe",
        meta=(DisplayName="Hover Object Types",
              ToolTip="Object types to detect for hover. Leave empty to use Selection channel default."))
    TArray<TEnumAsByte<EObjectTypeQuery>> HoverProbeObjectTypes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PACS|Selection|HoverProbe",
        meta=(DisplayName="Hover Probe Rate (Hz)", ClampMin="1", ClampMax="240"))
    float HoverProbeRateHz = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PACS|Selection|HoverProbe",
        meta=(DisplayName="Confirm Line of Sight",
              ToolTip="Only hover when line of sight is clear (prevents hover through walls)"))
    bool bHoverProbeConfirmVisibility = true;

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PACS|Input",
        meta=(AllowPrivateAccess="true"))
    TObjectPtr<UPACS_InputHandlerComponent> InputHandler;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PACS|Input",
        meta=(AllowPrivateAccess="true"))
    TObjectPtr<UPACS_EdgeScrollComponent> EdgeScrollComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PACS|Selection",
        meta=(AllowPrivateAccess="true"))
    TObjectPtr<UPACS_HoverProbeComponent> HoverProbe;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PACS|NPC",
        meta=(AllowPrivateAccess="true"))
    TObjectPtr<class UPACS_NPCBehaviorComponent> NPCBehaviorComponent;

    void ValidateInputSystem();
    void DisplayInputContextDebug();
#pragma endregion

#pragma region Selection System
public:
    // Selection System RPCs
    UFUNCTION(Server, Reliable)
    void ServerRequestSelect(AActor* TargetActor);

    UFUNCTION(Server, Reliable)
    void ServerRequestSelectMultiple(const TArray<AActor*>& TargetActors);

    UFUNCTION(Server, Reliable)
    void ServerRequestDeselect();

    // Multi-NPC movement command
    UFUNCTION(Server, Reliable)
    void ServerRequestMoveMultiple(const TArray<AActor*>& NPCs, FVector_NetQuantize TargetLocation);

    // Client notification for selection changes
    UFUNCTION(Client, Reliable)
    void ClientUpdateSelectedNPCs(const TArray<AActor*>& SelectedNPCs);
#pragma endregion

#pragma region Marquee Selection
public:
    // Marquee state accessors
    UFUNCTION(BlueprintPure, Category = "PACS|Marquee")
    bool IsMarqueeActive() const { return bIsMarqueeActive; }

    UFUNCTION(BlueprintPure, Category = "PACS|Marquee")
    FVector2D GetMarqueeStartPos() const { return MarqueeStartPos; }

    UFUNCTION(BlueprintPure, Category = "PACS|Marquee")
    FVector2D GetMarqueeCurrentPos() const { return MarqueeCurrentPos; }

    // Marquee configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PACS|Marquee",
        meta = (DisplayName = "Marquee Drag Threshold", ClampMin = "1.0", ClampMax = "20.0"))
    float MarqueeDragThreshold = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PACS|Marquee",
        meta = (DisplayName = "Marquee Update Rate (Hz)", ClampMin = "10", ClampMax = "60"))
    float MarqueeUpdateRate = 30.0f;

private:
    // Marquee selection state
    FVector2D MarqueeStartPos;
    FVector2D MarqueeCurrentPos;
    bool bIsMarqueeActive = false;
    bool bLeftMousePressed = false;
    bool bIsLeftClickHeld = false;  // Track Enhanced Input click state
    TArray<TWeakObjectPtr<AActor>> MarqueeHoveredActors;

    // Marquee update timer
    FTimerHandle MarqueeUpdateTimer;

    // Marquee helper methods
    void StartMarquee();
    void UpdateMarquee();
    void FinalizeMarquee();
    void ClearMarquee();
    void QueryActorsInMarquee();
#pragma endregion

#pragma region NPC Spawn Placement
public:
    // Spawn placement interface for UI - Updated for GameplayTag-based system
    UFUNCTION(BlueprintCallable, Category = "PACS|Spawn")
    void EnterSpawnPlacementMode(FGameplayTag SpawnTag);

    UFUNCTION(BlueprintCallable, Category = "PACS|Spawn")
    void ExitSpawnPlacementMode();

    // Legacy index-based methods (kept for compatibility)
    UFUNCTION(BlueprintCallable, Category = "PACS|Spawn")
    void BeginSpawnPlacement(int32 ConfigIndex);

    UFUNCTION(BlueprintCallable, Category = "PACS|Spawn")
    void CancelSpawnPlacement();

    // Config discovery for dynamic UI
    UFUNCTION(BlueprintCallable, Category = "PACS|Spawn")
    TArray<int32> GetAvailableSpawnConfigs() const;

    UFUNCTION(BlueprintCallable, Category = "PACS|Spawn")
    FText GetSpawnConfigDisplayName(int32 ConfigIndex) const;

    UFUNCTION(BlueprintCallable, Category = "PACS|Spawn")
    class UTexture2D* GetSpawnConfigIcon(int32 ConfigIndex) const;

    // Spawn state accessors
    UFUNCTION(BlueprintPure, Category = "PACS|Spawn")
    bool IsPlacingSpawn() const { return bIsPlacingSpawn; }

    UFUNCTION(BlueprintPure, Category = "PACS|Spawn")
    bool IsSpawnPlacementMode() const { return bSpawnPlacementMode; }

    UFUNCTION(BlueprintPure, Category = "PACS|Spawn")
    int32 GetActiveSpawnConfigIndex() const { return ActiveSpawnConfigIndex; }

    UFUNCTION(BlueprintPure, Category = "PACS|Spawn")
    FGameplayTag GetPendingSpawnTag() const { return PendingSpawnTag; }

    // Spawn limits
    UPROPERTY(EditDefaultsOnly, Category = "PACS|Spawn",
        meta = (DisplayName = "Max Player Spawns", ClampMin = "1", ClampMax = "100"))
    int32 MaxPlayerSpawns = 20;

    // Server RPC for spawning
    UFUNCTION(Server, Reliable)
    void ServerRequestSpawnNPC(FGameplayTag SpawnTag, FVector_NetQuantize Location);

    // Client RPC for spawn feedback
    UFUNCTION(Client, Reliable)
    void ClientNotifySpawnResult(bool bSuccess, const FString& Message);

private:
    // Spawn placement state (new)
    FGameplayTag PendingSpawnTag;
    bool bSpawnPlacementMode = false;

    // Legacy spawn placement state
    int32 ActiveSpawnConfigIndex = -1;
    bool bIsPlacingSpawn = false;

    // Track spawned NPCs per player (for limits)
    int32 PlayerSpawnedCount = 0;

    // Input handlers for spawn placement
    void HandleSpawnPlacementClick();
    void HandleSpawnPlacementCancel();

    // New input handler for placement mode
    void HandlePlaceNPCAction(const FInputActionValue& Value);
    void HandleCancelPlacementAction(const FInputActionValue& Value);

    // Helper to get spawn location from cursor
    bool GetSpawnLocationFromCursor(FVector& OutLocation) const;

    // Server RPC for spawning
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerSpawnNPCAtLocation(int32 ConfigIndex, FVector_NetQuantize Location);

    // Client feedback
    UFUNCTION(Client, Reliable)
    void ClientSpawnSucceeded(int32 ConfigIndex);

    UFUNCTION(Client, Reliable)
    void ClientSpawnFailed(int32 ConfigIndex, uint8 FailureReason);

    // Spawn UI management
    void CreateSpawnUI();
    void DestroySpawnUI();
#pragma endregion

#pragma region Spawn UI
public:
    // Spawn UI widget class - set this in Blueprint to your WBP_MainOverlay
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PACS|Spawn UI",
        meta = (DisplayName = "Spawn UI Widget Class"))
    TSubclassOf<UUserWidget> SpawnUIWidgetClass;

private:
    // Active spawn UI widget instance
    UPROPERTY()
    TObjectPtr<UUserWidget> SpawnUIWidget;
#pragma endregion
};