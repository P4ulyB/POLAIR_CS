#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PACSServerKeepaliveSubsystem.generated.h"

UCLASS()
class POLAIR_CS_API UPACSServerKeepaliveSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category="PACS Server")
    void RegisterPlayer(const FString& PlayerId);

    UFUNCTION(BlueprintCallable, Category="PACS Server")  
    void UnregisterPlayer(const FString& PlayerId);

    UFUNCTION(BlueprintPure, Category="PACS Server")
    int32 GetConnectedPlayerCount() const { return ConnectedPlayers.Num(); }

private:
    void TickGSDKUpdate();
    void CheckIdleShutdown();
    void ShutdownServer();

    TSet<FString> ConnectedPlayers;
    FTimerHandle GSDKUpdateTimer;
    FTimerHandle IdleCheckTimer;
    
    float LastPlayerDisconnectTime = 0.0f;
    static constexpr float IDLE_SHUTDOWN_DELAY = 300.0f; // 5 minutes
};