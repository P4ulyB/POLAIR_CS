#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PACS_EdgeScrollComponent.generated.h"

class UPACS_InputHandlerComponent;
class APACS_AssessorPawn;
class UAssessorPawnConfig;

/**
 * Edge scrolling component for PlayerController
 * Handles mouse edge detection and applies movement to AssessorPawn
 * Client-side only - no replication required
 */
UCLASS(NotBlueprintable, ClassGroup=(Input), meta=(BlueprintSpawnableComponent=false))
class POLAIR_CS_API UPACS_EdgeScrollComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPACS_EdgeScrollComponent();

    // Enable/disable edge scrolling (for debugging/testing)
    UFUNCTION(BlueprintCallable, Category="EdgeScroll")
    void SetEnabled(bool bNewEnabled) { bEnabled = bNewEnabled; }

    UFUNCTION(BlueprintPure, Category="EdgeScroll")
    bool IsEnabled() const { return bEnabled; }

    UFUNCTION(BlueprintPure, Category="EdgeScroll")
    bool IsActivelyScrolling() const { return bIsActivelyScrolling; }

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    // Configuration
    UPROPERTY(EditDefaultsOnly, Category="EdgeScroll")
    bool bEnabled = true;

    // State tracking
    bool bIsActivelyScrolling = false;
    bool bComponentReady = false;

    // Performance caching
    mutable bool bPermissionCacheValid = false;
    mutable bool bCachedPermissionResult = false;
    mutable float PermissionCacheTime = 0.0f;
    static constexpr float PermissionCacheLifetime = 0.1f; // 100ms cache

    // Viewport caching (invalidated on window resize)
    mutable FVector2D CachedViewportSize = FVector2D::ZeroVector;
    mutable float CachedDPIScale = 1.0f;
    mutable bool bViewportCacheValid = false;

    // Component references (cached for performance)
    UPROPERTY()
    TWeakObjectPtr<UPACS_InputHandlerComponent> CachedInputHandler;

    UPROPERTY()
    TWeakObjectPtr<APACS_AssessorPawn> CachedAssessorPawn;

    // Core logic methods
    bool ShouldAllowEdgeScrolling() const;
    FVector2D ComputeEdgeScrollInput() const;

    // Helper methods
    bool GetDPIAwareMousePosition(FVector2D& OutMousePos) const;
    bool IsMouseOverInteractiveUI(const FVector2D& MousePos, const FVector2D& ViewportSize, int32 EdgeMarginPx) const;
    bool IsMouseCapturedOrButtonPressed() const;
    bool IsCurrentContextAllowedForEdgeScrolling() const;
    bool UpdateViewportCache() const;
    void UpdateComponentReadiness();
    void InvalidateCaches();

    // Safe getters
    UPACS_InputHandlerComponent* GetInputHandler() const;
    APACS_AssessorPawn* GetAssessorPawn() const;
    const UAssessorPawnConfig* GetAssessorConfig() const;
    APlayerController* GetPlayerController() const;

    // Note: Viewport resize handling moved to tick-based checking in UE5.5
};