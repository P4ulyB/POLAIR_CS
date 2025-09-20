#include "Components/PACS_SelectionInputBridge.h"
#include "Actors/PACS_SelectionCueProxy.h"
#include "Actors/PACS_NPCBase.h"
#include "Players/PACS_PlayerController.h"
#include "Players/PACS_PlayerState.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

UPACS_SelectionInputBridge::UPACS_SelectionInputBridge()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UPACS_SelectionInputBridge::BeginPlay()
{
    Super::BeginPlay();
    OwnerPC = Cast<APACS_PlayerController>(GetOwner());
}

void UPACS_SelectionInputBridge::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (CurrentProxy.IsValid()) CurrentProxy->SetLocalHovered(false);
    Super::EndPlay(EndPlayReason);
}

void UPACS_SelectionInputBridge::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!bEnableHoverTrace) return;
    UpdateHover();
}

void UPACS_SelectionInputBridge::UpdateHover()
{
    if (!OwnerPC.IsValid()) OwnerPC = Cast<APACS_PlayerController>(GetOwner());
    APlayerController* PC = OwnerPC.Get();
    if (!PC) return;

    FVector WorldLoc; FVector WorldDir;
    if (!PC->DeprojectMousePositionToWorld(WorldLoc, WorldDir)) return;

    const FVector Start = WorldLoc;
    const FVector End   = Start + WorldDir * TraceDistance;

    FHitResult Hit;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(SelectionHoverTrace), false);
    if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, SelectionTraceChannel, Q))
    {
        if (APACS_SelectionCueProxy* P = FindProxyFromHit(Hit))
        {
            if (CurrentProxy.Get() != P)
            {
                if (CurrentProxy.IsValid()) CurrentProxy->SetLocalHovered(false);
                CurrentProxy = P;
                CurrentProxy->SetLocalHovered(true);
            }
            return;
        }
    }

    if (CurrentProxy.IsValid())
    {
        CurrentProxy->SetLocalHovered(false);
        CurrentProxy = nullptr;
    }
}

APACS_SelectionCueProxy* UPACS_SelectionInputBridge::FindProxyFromHit(const FHitResult& Hit) const
{
    if (AActor* A = Hit.GetActor())
    {
        if (APACS_SelectionCueProxy* P = Cast<APACS_SelectionCueProxy>(A))
            return P;

        if (APACS_NPCBase* NPC = Cast<APACS_NPCBase>(A))
        {
            // If NPC exposes a getter to its proxy, you could use it here.
        }
    }
    return nullptr;
}

uint16 UPACS_SelectionInputBridge::GetLocalShortId() const
{
    if (OwnerPC.IsValid())
    {
        if (const APACS_PlayerState* PS = OwnerPC->GetPlayerState<APACS_PlayerState>())
            return static_cast<uint16>(PS->GetPlayerId() & 0xFFFF);
    }
    return 0;
}

void UPACS_SelectionInputBridge::SelectOrRelease()
{
    if (!CurrentProxy.IsValid()) return;
    const uint16 MyId = GetLocalShortId();
    const bool bIsOwner = (CurrentProxy->SelectedById == MyId);
    const uint16 NewOwner = bIsOwner ? 0 : MyId;
    CurrentProxy->ServerSetSelectedBy(NewOwner);
}
