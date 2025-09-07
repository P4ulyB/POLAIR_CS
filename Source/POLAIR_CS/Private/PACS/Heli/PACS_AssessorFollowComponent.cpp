#include "PACS/Heli/PACS_AssessorFollowComponent.h"
#include "PACS/Heli/PACS_CandidateHelicopterCharacter.h"

void UPACS_AssessorFollowComponent::BeginPlay()
{
    Super::BeginPlay();
    SetComponentTickEnabled(false);
}

void UPACS_AssessorFollowComponent::ClientBeginFollow_Implementation(APACS_CandidateHelicopterCharacter* InTarget)
{
    Target = InTarget;
    SetComponentTickEnabled(true);
}

void UPACS_AssessorFollowComponent::ClientEndFollow_Implementation()
{
    Target.Reset();
    SetComponentTickEnabled(false);
}

void UPACS_AssessorFollowComponent::TickComponent(float Dt, ELevelTick, FActorComponentTickFunction*)
{
    if (!Target.IsValid()) return;
    const FVector Desired = Target->GetActorTransform().TransformPositionNoScale(WorldOffset);
    const FVector NewLoc = FMath::VInterpTo(GetOwner()->GetActorLocation(), Desired, Dt, FollowInterpSpeed);
    const FRotator LookAt = (Target->GetActorLocation() - GetOwner()->GetActorLocation()).Rotation();
    const FRotator NewRot = FMath::RInterpTo(GetOwner()->GetActorRotation(), LookAt, Dt, FollowInterpSpeed);
    GetOwner()->SetActorLocationAndRotation(NewLoc, NewRot, false);
}