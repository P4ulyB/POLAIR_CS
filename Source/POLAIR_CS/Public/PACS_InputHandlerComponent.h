#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnhancedInputSubsystems.h"
#include "PACS_InputTypes.h"
#include "PACS_InputMappingConfig.h"
#include "PACS_InputHandlerComponent.generated.h"

struct FInputActionInstance;

UCLASS(NotBlueprintable, ClassGroup=(Input), meta=(BlueprintSpawnableComponent=false))
class POLAIR_CS_API UPACS_InputHandlerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPACS_InputHandlerComponent();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input")
    TObjectPtr<UPACS_InputMappingConfig> InputConfig;

    bool IsHealthy() const { return bIsInitialized && InputConfig && InputConfig->IsValid(); }

    // --- Public API ---
    
    UFUNCTION(BlueprintCallable, Category="Input")
    void RegisterReceiver(UObject* Receiver, int32 Priority);

    UFUNCTION(BlueprintCallable, Category="Input")
    void UnregisterReceiver(UObject* Receiver);

    UFUNCTION(BlueprintCallable, Category="Input")
    void SetBaseContext(EPACS_InputContextMode ContextMode);

    UFUNCTION(BlueprintCallable, Category="Input")
    void ToggleMenuContext();

    UFUNCTION(BlueprintCallable, Category="Input")
    void PushOverlay(UInputMappingContext* Context, EPACS_OverlayType Type = EPACS_OverlayType::Blocking, 
        int32 Priority = 1000);

    UFUNCTION(BlueprintCallable, Category="Input")
    void PopOverlay();

    UFUNCTION(BlueprintCallable, Category="Input")
    void PopAllOverlays();

    UFUNCTION(BlueprintPure, Category="Input")
    bool HasBlockingOverlay() const;

    UFUNCTION(BlueprintPure, Category="Input")
    int32 GetOverlayCount() const { return OverlayStack.Num(); }

    void HandleAction(const FInputActionInstance& Instance);

    void OnSubsystemAvailable();
    void OnSubsystemUnavailable();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UPROPERTY() 
    TArray<FPACS_InputReceiverEntry> Receivers;
    
    UPROPERTY() 
    TMap<TObjectPtr<UInputAction>, FName> ActionToNameMap;
    
    UPROPERTY() 
    EPACS_InputContextMode CurrentBaseMode = EPACS_InputContextMode::Gameplay;
    
    UPROPERTY() 
    TArray<FPACS_OverlayEntry> OverlayStack;

    UPROPERTY()
    TSet<TObjectPtr<UInputMappingContext>> ManagedContexts;

#if !UE_SERVER
    bool bIsInitialized = false;
    bool bActionMapBuilt = false;
    bool bSubsystemValid = false;
    
    uint32 RegistrationCounter = 0;
    int32 InvalidReceiverCount = 0;

    TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> CachedSubsystem;

    void Initialize();
    void Shutdown();
    void BuildActionNameMap();
    void EnsureActionMapBuilt();
    
    EPACS_InputHandleResult RouteActionInternal(FName ActionName, const FInputActionValue& Value);
    void CleanInvalidReceivers();
    void SortReceivers();
    
    void UpdateManagedContexts();
    void RemoveAllManagedContexts();
    
    UInputMappingContext* GetBaseContext(EPACS_InputContextMode Mode) const;
    int32 GetBaseContextPriority(EPACS_InputContextMode Mode) const;
    
    UEnhancedInputLocalPlayerSubsystem* GetValidSubsystem();
    bool ValidateConfig() const;
    
    FORCEINLINE bool EnsureGameThread() const
    {
        if (!IsInGameThread())
        {
            PACS_INPUT_ERROR("Input handler called from non-game thread!");
            return false;
        }
        return true;
    }
#endif // !UE_SERVER
};