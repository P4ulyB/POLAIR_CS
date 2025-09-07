#pragma once
#include "Engine/DataAsset.h"
#include "Curves/CurveFloat.h"
#include "PACS_CandidateHelicopterData.generated.h"

UCLASS(BlueprintType)
class UPACS_CandidateHelicopterData : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly) float DefaultAltitudeCm = 20000.f;   // 200 m
    UPROPERTY(EditDefaultsOnly) float DefaultRadiusCm   = 15000.f;   // 150 m
    UPROPERTY(EditDefaultsOnly) float DefaultSpeedCms   = 2222.22f;  // 80 km/h
    UPROPERTY(EditDefaultsOnly) float MaxSpeedCms       = 5555.56f;  // 200 km/h

    UPROPERTY(EditDefaultsOnly, meta=(ClampMin="0", ClampMax="10")) float MaxBankDeg = 10.f;

    UPROPERTY(EditDefaultsOnly) TObjectPtr<UCurveFloat> CenterInterp = nullptr;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UCurveFloat> AltInterp    = nullptr;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UCurveFloat> RadiusInterp = nullptr;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UCurveFloat> SpeedInterp  = nullptr;

    UPROPERTY(EditDefaultsOnly) float CenterDurS = 4.f;
    UPROPERTY(EditDefaultsOnly) float AltDurS    = 3.f;
    UPROPERTY(EditDefaultsOnly) float RadiusDurS = 3.f;
    UPROPERTY(EditDefaultsOnly) float SpeedDurS  = 2.f;

    UPROPERTY(EditDefaultsOnly) float MaxCenterDriftCms = 3000.f; // drift clamp for visuals

    UPROPERTY(EditDefaultsOnly) FVector SeatLocalClamp = FVector(50,50,50);

    UPROPERTY(EditDefaultsOnly) FTransform AssessorFollowView;
};