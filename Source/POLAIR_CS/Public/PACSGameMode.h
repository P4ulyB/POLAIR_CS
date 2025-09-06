#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PACSGameMode.generated.h"

UCLASS()
class POLAIR_CS_API APACSGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    virtual void PreLogin(const FString& Options, const FString& Address, 
        const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
        
    virtual void PostLogin(APlayerController* NewPlayer) override;
        
    virtual void Logout(AController* Exiting) override;
};