#include "Actors/PACS_SelectionCueProxy.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Players/PACS_PlayerState.h"
#include "Data/PACS_SelectionTypes.h"
#include "Data/PACS_SelectionGlobalConfig.h"
#include "Data/PACS_SelectionLocalConfig.h"
#include "Net/UnrealNetwork.h"

APACS_SelectionCueProxy::APACS_SelectionCueProxy()
{
    PrimaryActorTick.bCanEverTick = false;

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    DecalComponent = CreateDefaultSubobject<UDecalComponent>(TEXT("Decal"));
    DecalComponent->SetupAttachment(RootComponent);
    DecalComponent->SetHiddenInGame(true);
    DecalComponent->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));

    bReplicates = true;
}

void APACS_SelectionCueProxy::BeginPlay()
{
    Super::BeginPlay();
    SnapToFloor();
    UpdateVisualForViewer();
}

void APACS_SelectionCueProxy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(APACS_SelectionCueProxy, SelectedById);
}

void APACS_SelectionCueProxy::ServerSetSelectedBy_Implementation(uint16 NewOwnerId)
{
    if (!HasAuthority()) return;
    SelectedById = NewOwnerId; // 0 releases
    OnRep_SelectedBy();
}

bool APACS_SelectionCueProxy::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
    const APlayerController* PC = Cast<APlayerController>(RealViewer);
    if (!PC) return false;
    const APACS_PlayerState* PS = PC->GetPlayerState<APACS_PlayerState>();
    return (PS && PS->bIsAssessor);
}

static uint16 GetLocalViewerId(const APACS_SelectionCueProxy* Self)
{
    if (const UWorld* W = Self->GetWorld())
    {
        if (const APlayerController* PC = W->GetFirstPlayerController())
        {
            if (const APACS_PlayerState* PS = PC->GetPlayerState<APACS_PlayerState>())
            {
                return static_cast<uint16>(PS->GetPlayerId() & 0xFFFF);
            }
        }
    }
    return 0;
}

void APACS_SelectionCueProxy::SetLocalHovered(bool bHovered)
{
    if (bIsLocallyHovered != bHovered)
    {
        bIsLocallyHovered = bHovered;
        UpdateVisualForViewer();
    }
}

void APACS_SelectionCueProxy::OnRep_SelectedBy()
{
    UpdateVisualForViewer();
}

void APACS_SelectionCueProxy::UpdateVisualForViewer()
{
    const uint16 Viewer = GetLocalViewerId(this);

    enum class ELocalState { Hide, Hovered, Owner, Unavailable } S = ELocalState::Hide;
    if (SelectedById == 0)
        S = bIsLocallyHovered ? ELocalState::Hovered : ELocalState::Hide;
    else if (SelectedById == Viewer)
        S = ELocalState::Owner;
    else
        S = ELocalState::Unavailable;

    const FSelectionVisualSet& V = GetVisualSet();
    switch (S)
    {
        case ELocalState::Hide:        DecalComponent->SetHiddenInGame(true);  return;
        case ELocalState::Hovered:     ApplyDecalParams(V.Hovered);            break;
        case ELocalState::Owner:       ApplyDecalParams(V.SelectedOwner);      break;
        case ELocalState::Unavailable: ApplyDecalParams(V.Unavailable);        break;
    }
    DecalComponent->SetHiddenInGame(false);
}

const FSelectionVisualSet& APACS_SelectionCueProxy::GetVisualSet() const
{
    if (LocalCfg.IsValid() && LocalCfg.Get()->bOverrideGlobal)
        return LocalCfg.Get()->Visuals;
    if (GlobalCfg.IsValid())
        return GlobalCfg.Get()->Visuals;
    static FSelectionVisualSet Default;
    return Default;
}

void APACS_SelectionCueProxy::ApplyDecalParams(const FSelectionDecalParams& P)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
    if (P.BaseMaterial && !P.BaseMaterial.IsValid()) const_cast<TSoftObjectPtr<UMaterialInterface>&>(P.BaseMaterial).LoadSynchronous();
    if (P.Texture      && !P.Texture.IsValid())      const_cast<TSoftObjectPtr<UTexture2D>&>(P.Texture).LoadSynchronous();
#endif
    if (UMaterialInterface* Base = P.BaseMaterial.Get())
        DecalComponent->SetDecalMaterial(Base);

    if (!MID) MID = DecalComponent->CreateDynamicMaterialInstance();
    if (MID)
    {
        MID->SetScalarParameterValue(TEXT("Brightness"), P.Brightness);
        MID->SetVectorParameterValue(TEXT("Colour"),     P.Colour);
        MID->SetTextureParameterValue(TEXT("Texture"),   P.Texture.Get());
    }
    DecalComponent->DecalSize = FVector(P.ScaleXY, P.ScaleXY, P.ThicknessZ);
}

void APACS_SelectionCueProxy::SnapToFloor()
{
    const FVector Start = GetActorLocation();
    const FVector End   = Start - FVector(0.f, 0.f, 200.f);

    FHitResult Hit;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(SelectionProxySnap), false);
    if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Q))
    {
        SetActorLocation(Hit.ImpactPoint + Hit.ImpactNormal * 2.f);
        const FRotator Align = Hit.ImpactNormal.Rotation();
        DecalComponent->SetWorldRotation(Align + FRotator(-90.f, 0.f, 0.f));
    }
}
