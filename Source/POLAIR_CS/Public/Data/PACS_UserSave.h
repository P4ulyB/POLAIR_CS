#pragma once
#include "GameFramework/SaveGame.h"
#include "Data/PACS_VRSeatSave.h"
#include "PACS_UserSave.generated.h"

UCLASS(BlueprintType)
class UPACS_UserSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() TMap<FString, FPACS_VRSeatSave> SeatByPlayer;
};