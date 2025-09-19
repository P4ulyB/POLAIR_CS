#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpectatorPawn.h"
#include "PACS_InputTypes.h"                // IPACS_InputReceiver, EPACS_InputHandleResult
#include "PACS_AssessorPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UPACS_InputHandlerComponent;
class APACS_PlayerController;
class UAssessorPawnConfig;

/**
 * Minimal Assessor spectator pawn (client-only navigation):
 * - WASD planar movement using AxisBasis (local X/Y only; Z locked)
 * - Mouse wheel zoom via SpringArm TargetArmLength (clamped)
 * - Camera tilt set by config; optional camera lag
 * - Registers with PACS input handler on possession
 * Threading: game-thread only.
 */
UCLASS()
class POLAIR_CS_API APACS_AssessorPawn : public ASpectatorPawn, public IPACS_InputReceiver
{
    GENERATED_BODY()

public:
    APACS_AssessorPawn();

    // IPACS_InputReceiver
    virtual EPACS_InputHandleResult HandleInputAction(FName ActionName, const FInputActionValue& Value) override;
    virtual int32 GetInputPriority() const override { return PACS_InputPriority::Gameplay; }

    // Config
    UPROPERTY(EditDefaultsOnly, Category="Assessor|Config")
    TObjectPtr<UAssessorPawnConfig> Config;

    // Narrow navigation API (controller may call later).
    void AddPlanarInput(const FVector2D& Axis01); // Accumulate this frame (X=Right, Y=Forward)
    void AddZoomSteps(float Steps);                // Discrete wheel ticks
    void AddRotationInput(float Direction);        // Discrete rotation steps (+1 right, -1 left)

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void UnPossessed() override;

    // Override input setup to disable SpectatorPawn defaults and use only PACS input
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void EnableInput(APlayerController* PlayerController) override;
    virtual void DisableInput(APlayerController* PlayerController) override;

    virtual void OnRep_Controller() override;

private:
    // Components
    UPROPERTY(VisibleAnywhere, Category="Assessor|Components")
    TObjectPtr<USceneComponent> AxisBasis;

    UPROPERTY(VisibleAnywhere, Category="Assessor|Components")
    TObjectPtr<USpringArmComponent> SpringArm;

    UPROPERTY(VisibleAnywhere, Category="Assessor|Components")
    TObjectPtr<UCameraComponent> Camera;

    // Input accumulation
    float InputForward = 0.f;
    float InputRight   = 0.f;

    // Target zoom (ArmLength)
    float TargetArmLength = 0.f;

    // Rotation state - using cumulative tracking to avoid direction reversal
    float CumulativeYaw = 0.0f;         // Total rotation applied (can exceed 360°)
    float TargetCumulativeYaw = 0.0f;    // Target cumulative rotation
    bool bIsRotating = false;            // For external systems that need rotation state

    // Optional: set this to a DA in content to guarantee a config even if the BP forgot to assign one.
    UPROPERTY(EditDefaultsOnly, Category="Assessor|Config")
    TSoftObjectPtr<class UAssessorPawnConfig> FallbackConfig;

    bool bConfigApplied = false;

    // Helpers
    void RegisterWithInputHandler(APACS_PlayerController* PC);
    void UnregisterFromInputHandler(APACS_PlayerController* PC);
    void ApplyConfigDefaults();
    void StepZoom(float AxisValue);
    void UpdateRotation(float DeltaTime);
    float NormalizeYaw(float Yaw);

    // Ensures Config is non-null; tries FallbackConfig if needed. Returns true if we have a config.
    bool EnsureConfigReady();
};