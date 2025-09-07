#pragma once
#include "PACS_VRSeatSave.generated.h"

USTRUCT(BlueprintType)
struct FPACS_VRSeatSave
{
    GENERATED_BODY()
    UPROPERTY() FVector_NetQuantize10 SeatLocalOffsetCm = FVector::ZeroVector;
};