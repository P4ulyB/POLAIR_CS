#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AssessorPawnConfig.generated.h"

UCLASS(BlueprintType)
class POLAIR_CS_API UAssessorPawnConfig : public UDataAsset
{
    GENERATED_BODY()
public:
    // Downward camera tilt applied to the SpringArm (negative pitch).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Assessor|Camera")
    float CameraTiltDegrees = 30.f;

    // Zoom represented as SpringArm length.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Assessor|Zoom")
    float StartingArmLength = 1500.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Assessor|Zoom")
    float MinArmLength = 400.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Assessor|Zoom")
    float MaxArmLength = 4000.f;

    // Planar move speed on AxisBasis X/Y (cm/s).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Assessor|Movement")
    float MoveSpeed = 2400.f;

    // Discrete zoom step per wheel tick.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Assessor|Zoom")
    float ZoomStep = 200.f;

    // SpringArm smoothing.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Assessor|Lag")
    bool bEnableCameraLag = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(EditCondition="bEnableCameraLag"), Category="Assessor|Lag")
    float CameraLagSpeed = 10.f;  // higher = snappier

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(EditCondition="bEnableCameraLag"), Category="Assessor|Lag")
    float CameraLagMaxDistance = 250.f;
};