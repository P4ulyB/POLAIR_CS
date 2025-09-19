#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PACS_SelectionInputBridge.generated.h"

class APACS_SelectionCueProxy;
class APACS_NPCBase;
class APACS_PlayerController;

/**
 * Minimal bridge that runs a single line trace to drive local hover and
 * forwards selection clicks to the proxy (server-authoritative).
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class POLAIR_CS_API UPACS_SelectionInputBridge : public UActorComponent
{
    GENERATED_BODY()

public:
    UPACS_SelectionInputBridge();

    /** Enable/disable hover tracing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Selection")
    bool bEnableHoverTrace = true;

    /** Trace channel to use for selection */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Selection")
    TEnumAsByte<ECollisionChannel> SelectionTraceChannel = ECC_Visibility;

    /** Max trace distance */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Selection", meta=(ClampMin="100.0"))
    float TraceDistance = 50000.f;

    /** Call from input to attempt selection */
    UFUNCTION(BlueprintCallable, Category="Selection")
    void SelectOrRelease();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    TWeakObjectPtr<APACS_SelectionCueProxy> CurrentProxy;
    TWeakObjectPtr<APACS_PlayerController>  OwnerPC;

    void UpdateHover();
    APACS_SelectionCueProxy* FindProxyFromHit(const FHitResult& Hit) const;
    uint16 GetLocalShortId() const;
};