#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PACSLaunchArgSubsystem.generated.h"

USTRUCT(BlueprintType)
struct POLAIR_CS_API FPACSLaunchConnectArgs
{
    GENERATED_BODY()
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) 
    FString ServerIP;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) 
    int32 ServerPort = 0;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) 
    FString PlayFabPlayerName;

    bool IsServerEndpointValid() const 
    { 
        return !ServerIP.IsEmpty() && ServerPort > 0 && ServerPort <= 65535; 
    }
};

UCLASS()
class POLAIR_CS_API UPACSLaunchArgSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Launch") 
    FPACSLaunchConnectArgs Parsed;

    UFUNCTION(BlueprintCallable, Category="Launch") 
    void BeginConnectFlow();
    
    UFUNCTION(BlueprintPure, Category="Launch") 
    FString GetLauncherUsername() const { return Parsed.PlayFabPlayerName; }
    
    UFUNCTION(BlueprintCallable, Category="Launch") 
    bool TravelToServer();
    
    // Public for testing purposes
    void ParseCommandLine();

protected:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

private:
    void AttemptAutoConnect();
    
    bool bHasAttemptedConnect = false;
};