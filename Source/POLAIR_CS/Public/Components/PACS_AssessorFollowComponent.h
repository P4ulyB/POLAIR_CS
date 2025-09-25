#pragma once
#include "Components/ActorComponent.h"
#include "PACS_AssessorFollowComponent.generated.h"

UCLASS(ClassGroup=(Camera), meta=(BlueprintSpawnableComponent))
class UPACS_AssessorFollowComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere) float FollowInterpSpeed = 4.f;
    UPROPERTY(EditAnywhere) FVector WorldOffset = FVector(-2000, 1200, 800);

    UPROPERTY() TWeakObjectPtr<class APACS_CandidateHelicopterCharacter> Target;

    virtual void BeginPlay() override;
    virtual void TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(Client, Reliable) void ClientBeginFollow(APACS_CandidateHelicopterCharacter* InTarget);
    UFUNCTION(Client, Reliable) void ClientEndFollow();
};