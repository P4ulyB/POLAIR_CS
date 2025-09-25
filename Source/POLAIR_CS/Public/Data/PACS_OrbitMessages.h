#pragma once
#include "CoreMinimal.h"
#include "PACS_OrbitMessages.generated.h"

UENUM()
enum class EPACS_AnchorPolicy : uint8
{
    PreserveAngleOnce = 0,
    ResetAngleNow     = 1
};

USTRUCT()
struct FPACS_OrbitEdit
{
    GENERATED_BODY()

    UPROPERTY() bool bHasCenter=false;  UPROPERTY() FVector_NetQuantize100 NewCenterCm = FVector::ZeroVector;
    UPROPERTY() bool bHasAlt=false;     UPROPERTY() float NewAltCm=0.f;
    UPROPERTY() bool bHasRadius=false;  UPROPERTY() float NewRadiusCm=0.f;
    UPROPERTY() bool bHasSpeed=false;   UPROPERTY() float NewSpeedCms=0.f;

    UPROPERTY() float DurCenterS=0.f, DurAltS=0.f, DurRadiusS=0.f, DurSpeedS=0.f;

    UPROPERTY() uint16 TransactionId=0;
    UPROPERTY() EPACS_AnchorPolicy AnchorPolicy = EPACS_AnchorPolicy::PreserveAngleOnce;
};