#pragma once
#include "GameFramework/Character.h"
#include "PACS_InputTypes.h"
#include "PACS_CandidateHelicopterCharacter.generated.h"

class UPACS_HeliMovementComponent;
class UPACS_CandidateHelicopterData;
class UCameraComponent;

USTRUCT()
struct FPACS_OrbitTargets
{
    GENERATED_BODY()
    UPROPERTY() FVector_NetQuantize100 CenterCm = FVector::ZeroVector;
    UPROPERTY() float AltitudeCm = 0.f;
    UPROPERTY() float RadiusCm   = 1.f;
    UPROPERTY() float SpeedCms   = 0.f;

    UPROPERTY() float CenterDurS = 0.f;
    UPROPERTY() float AltDurS    = 0.f;
    UPROPERTY() float RadiusDurS = 0.f;
    UPROPERTY() float SpeedDurS  = 0.f;
};

USTRUCT()
struct FPACS_OrbitAnchors
{
    GENERATED_BODY()
    UPROPERTY() float CenterStartS = 0.f, AltStartS = 0.f, RadiusStartS = 0.f, SpeedStartS = 0.f;
    UPROPERTY() float OrbitStartS  = 0.f;
    UPROPERTY() float AngleAtStart = 0.f;
};

USTRUCT(BlueprintType)
struct FPACS_OrbitOffsets
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHasAltOffset=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHasRadiusOffset=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHasSpeedOffset=false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite) float AltitudeDeltaCm=0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float RadiusDeltaCm=0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SpeedDeltaCms=0.f;
};

UCLASS()
class POLAIR_CS_API APACS_CandidateHelicopterCharacter : public ACharacter, public IPACS_InputReceiver
{
    GENERATED_BODY()
public:
    APACS_CandidateHelicopterCharacter(const FObjectInitializer& OI);

    UPROPERTY(VisibleAnywhere) UStaticMeshComponent* HelicopterFrame;
    UPROPERTY(VisibleAnywhere) USceneComponent*      CockpitRoot;
    UPROPERTY(VisibleAnywhere) USceneComponent*      SeatOriginRef;
    UPROPERTY(VisibleAnywhere) USceneComponent*      SeatOffsetRoot;
    UPROPERTY(VisibleAnywhere) UCameraComponent*     VRCamera;

    UPROPERTY() UPACS_HeliMovementComponent* HeliCMC;
    UPROPERTY(EditDefaultsOnly) UPACS_CandidateHelicopterData* Data;

    UPROPERTY(ReplicatedUsing=OnRep_OrbitTargets) FPACS_OrbitTargets OrbitTargets;
    UPROPERTY(ReplicatedUsing=OnRep_OrbitAnchors) FPACS_OrbitAnchors OrbitAnchors;
    UPROPERTY(ReplicatedUsing=OnRep_SelectedBy)   APlayerState* SelectedBy = nullptr;

    UPROPERTY(Replicated) uint8  OrbitParamsVersion = 0;
    UPROPERTY()          uint16 LastAppliedTxnId    = 0;

    UPROPERTY(Transient) FVector SeatLocalOffsetCm = FVector::ZeroVector;

    UPROPERTY(EditDefaultsOnly) float BankInterpSpeed = 6.f;
    UPROPERTY(Transient) float CurrentBankDeg = 0.f;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    virtual void BeginPlay() override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void OnRep_Controller() override;
    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION(BlueprintCallable) void CenterSeatedPose(bool bSnapYawToVehicleForward = true);
    void NudgeSeatX(float Sign); void NudgeSeatY(float Sign); void NudgeSeatZ(float Sign);
    void ApplySeatOffset(); void ZeroSeatChain();

    UFUNCTION(Server, Reliable) void Server_ApplyOrbitParams(const struct FPACS_OrbitEdit& Edit);

    UFUNCTION(Server, Reliable) void Server_RequestSelect(APlayerState* Requestor);
    UFUNCTION(Server, Reliable) void Server_ReleaseSelect(APlayerState* Requestor);

    void ApplyOffsetsThenSeed(const FPACS_OrbitOffsets* Off);

    // Register with input on local possess; unregister on unpossess
    virtual void UnPossessed() override;

    // IPACS_InputReceiver
    virtual EPACS_InputHandleResult HandleInputAction(FName ActionName, const FInputActionValue& Value) override;
    virtual int32 GetInputPriority() const override { return PACS_InputPriority::Gameplay; } // keep it at gameplay

private:
    void RegisterAsReceiverIfLocal();
    void UnregisterAsReceiver();
    void Seat_Center();
    void Seat_X(float Axis);
    void Seat_Y(float Axis);
    void Seat_Z(float Axis);

    UPROPERTY(EditDefaultsOnly, Category = "VR Seat")
    float SeatNudgeStepCm = 2.f; // tune in BP/data

protected:
    UFUNCTION() void OnRep_OrbitTargets();
    UFUNCTION() void OnRep_OrbitAnchors();
    UFUNCTION() void OnRep_SelectedBy();

    bool ValidateOrbitCenter(const FVector& Proposed) const;
    void UpdateBankVisual(float DeltaSeconds);
};