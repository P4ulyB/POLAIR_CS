# Phase 1.7 — FULL Code Pack (Assessor-Only Decal Selection)
**Date:** 2025-09-19

## Actors/PACS_SelectionCueProxy.h
```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PACS_SelectionCueProxy.generated.h"

class UDecalComponent;
class UMaterialInstanceDynamic;
class APACS_PlayerState;

// Forward decls for data assets
class UPACS_SelectionGlobalConfig;
class UPACS_SelectionLocalConfig;
struct FSelectionVisualSet;
struct FSelectionDecalParams;

/**
 * Visual proxy actor for NPC selection feedback
 * Only visible to assessors (filtered via IsNetRelevantFor)
 */
UCLASS()
class POLAIR_CS_API APACS_SelectionCueProxy : public AActor
{
    GENERATED_BODY()

public:
    APACS_SelectionCueProxy();

    // Begin AActor
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;
    // End AActor

    /** Replicated selection state (0 = free, otherwise short player id) */
    UPROPERTY(ReplicatedUsing=OnRep_SelectedBy, BlueprintReadOnly, Category="Selection")
    uint16 SelectedById = 0;

    /** Server RPC to set selection owner (validate inside body) */
    UFUNCTION(Server, Reliable)
    void ServerSetSelectedBy(uint16 NewOwnerId);

    /** Decal component (projected to floor) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<UDecalComponent> DecalComponent;

    /** Set local hover state (not replicated) */
    UFUNCTION(BlueprintCallable, Category="Selection")
    void SetLocalHovered(bool bHovered);

    /** Update visual material/size for current viewer */
    void UpdateVisualForViewer();

    /** Optional: snap the decal to the ground under this actor */
    void SnapToFloor();

    /** Global config reference (soft) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Config")
    TSoftObjectPtr<UPACS_SelectionGlobalConfig> GlobalCfg;

    /** Local per-actor override (soft) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Config")
    TSoftObjectPtr<UPACS_SelectionLocalConfig> LocalCfg;

protected:
    /** Local hover state (not replicated) */
    bool bIsLocallyHovered = false;

    /** Replication callback */
    UFUNCTION()
    void OnRep_SelectedBy();

    /** Access the chosen visual set (local override or global) */
    const FSelectionVisualSet& GetVisualSet() const;

    /** Apply parameters to the decal (MID + DecalSize) */
    void ApplyDecalParams(const FSelectionDecalParams& Params);

private:
    /** MID for dynamic parameter updates */
    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> MID;
};

```

## Actors/PACS_SelectionCueProxy.cpp
```cpp
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

```

## Actors/PACS_NPCBase.h
```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PACS_NPCBase.generated.h"

class APACS_SelectionCueProxy;
class UPACS_NPCArchetypeData;
class UPACS_SelectionGlobalConfig;
class UPACS_SelectionLocalConfig;

/**
 * Thin NPC character shell with archetype-driven appearance.
 * No selection state here; selection visuals live on an assessor-only proxy.
 */
UCLASS()
class POLAIR_CS_API APACS_NPCBase : public ACharacter
{
    GENERATED_BODY()

public:
    APACS_NPCBase();

    // Begin AActor
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    // End AActor

    /** What this NPC is (mesh/materials/anim/speeds), soft ref */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Config")
    TSoftObjectPtr<UPACS_NPCArchetypeData> Archetype;

    /** Optional per-actor selection override to feed the proxy */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Selection")
    TSoftObjectPtr<UPACS_SelectionLocalConfig> SelectionLocalOverride;

    /** Optional global selection config to feed the proxy (can also be provided by a subsystem) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Selection")
    TSoftObjectPtr<UPACS_SelectionGlobalConfig> SelectionGlobalConfig;

protected:
    /** Assessor-only visual proxy (server spawns; clients receive by relevance) */
    TWeakObjectPtr<APACS_SelectionCueProxy> SelectionProxy;

private:
    void ApplyArchetype(); // sets mesh/material/anim/walk speed
};

```

## Actors/PACS_NPCBase.cpp
```cpp
#include "Actors/PACS_NPCBase.h"
#include "Actors/PACS_SelectionCueProxy.h"
#include "Data/PACS_NPCArchetypeData.h"
#include "Data/PACS_SelectionGlobalConfig.h"
#include "Data/PACS_SelectionLocalConfig.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

APACS_NPCBase::APACS_NPCBase()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
}

void APACS_NPCBase::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        if (!SelectionProxy.IsValid())
        {
            if (APACS_SelectionCueProxy* P = GetWorld()->SpawnActor<APACS_SelectionCueProxy>())
            {
                P->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
                P->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);

                P->GlobalCfg = SelectionGlobalConfig;
                P->LocalCfg  = SelectionLocalOverride;

                SelectionProxy = P;
            }
        }
    }

    ApplyArchetype();
}

void APACS_NPCBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (SelectionProxy.IsValid())
    {
        SelectionProxy->Destroy();
        SelectionProxy = nullptr;
    }
    Super::EndPlay(EndPlayReason);
}

void APACS_NPCBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    // No selection state on NPC
}

void APACS_NPCBase::ApplyArchetype()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
    if (Archetype && !Archetype.IsValid()) Archetype.LoadSynchronous();
#endif
    UPACS_NPCArchetypeData* Data = Archetype.Get();
    if (!Data) return;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
    if (Data->SkeletalMesh && !Data->SkeletalMesh.IsValid()) Data->SkeletalMesh.LoadSynchronous();
    if (Data->AnimBPClass  && !Data->AnimBPClass.IsValid())  Data->AnimBPClass.LoadSynchronous();
    for (auto& KVP : Data->MaterialOverrides)
    {
        if (KVP.Value && !KVP.Value.IsValid()) KVP.Value.LoadSynchronous();
    }
#endif

    if (USkeletalMesh* Mesh = Data->SkeletalMesh.Get())
    {
        GetMesh()->SetSkeletalMesh(Mesh);
    }
    if (UClass* AnimClass = Data->AnimBPClass.Get())
    {
        GetMesh()->SetAnimInstanceClass(AnimClass);
    }
    for (const auto& KVP : Data->MaterialOverrides)
    {
        if (UMaterialInterface* MI = KVP.Value.Get())
        {
            GetMesh()->SetMaterial(KVP.Key, MI);
        }
    }
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->MaxWalkSpeed = Data->WalkSpeed;
    }
}

```

## Players/PACS_PlayerState.h
```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "PACS_PlayerState.generated.h"

UENUM(BlueprintType)
enum class EHMDState : uint8
{
    Unknown = 0,
    NoHMD   = 1,
    HasHMD  = 2
};

UCLASS()
class POLAIR_CS_API APACS_PlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    APACS_PlayerState();

    UPROPERTY(ReplicatedUsing=OnRep_HMDState, BlueprintReadOnly, Category="PACS")
    EHMDState HMDState = EHMDState::Unknown;

    UPROPERTY(ReplicatedUsing=OnRep_IsAssessor, BlueprintReadOnly, Category="PACS")
    bool bIsAssessor = false;

    UFUNCTION() void OnRep_HMDState();
    UFUNCTION() void OnRep_IsAssessor();

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
};

```

## Players/PACS_PlayerState.cpp
```cpp
#include "Players/PACS_PlayerState.h"
#include "Net/UnrealNetwork.h"

APACS_PlayerState::APACS_PlayerState()
{
    HMDState = EHMDState::Unknown;
}

void APACS_PlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(APACS_PlayerState, HMDState);
    DOREPLIFETIME(APACS_PlayerState, bIsAssessor);
}

void APACS_PlayerState::OnRep_HMDState()
{
    UE_LOG(LogTemp, Log, TEXT("PACS PlayerState: HMD state changed to %d"), static_cast<int32>(HMDState));
}

void APACS_PlayerState::OnRep_IsAssessor()
{
    UE_LOG(LogTemp, Log, TEXT("PACS PlayerState: Assessor state changed to %s"), bIsAssessor ? TEXT("true") : TEXT("false"));
}

```

## Data/PACS_SelectionTypes.h
```cpp
#pragma once

#include "CoreMinimal.h"
#include "Engine/PrimaryDataAsset.h"
#include "PACS_SelectionTypes.generated.h"

USTRUCT(BlueprintType)
struct FSelectionDecalParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Selection|Material")
    TSoftObjectPtr<UMaterialInterface> BaseMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Selection|Material")
    TSoftObjectPtr<UTexture2D> Texture;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Selection|Material")
    FLinearColor Colour = FLinearColor(0.2f, 0.7f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Selection|Material", meta=(ClampMin="0.0"))
    float Brightness = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Selection|Decal", meta=(ClampMin="1.0"))
    float ScaleXY = 256.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Selection|Decal", meta=(ClampMin="1.0"))
    float ThicknessZ = 64.f;
};

USTRUCT(BlueprintType)
struct FSelectionVisualSet
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Selection")
    FSelectionDecalParams Hovered;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Selection")
    FSelectionDecalParams SelectedOwner;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Selection")
    FSelectionDecalParams Unavailable;
};

```

## Data/PACS_SelectionGlobalConfig.h
```cpp
#pragma once

#include "CoreMinimal.h"
#include "Engine/PrimaryDataAsset.h"
#include "Data/PACS_SelectionTypes.h"
#include "PACS_SelectionGlobalConfig.generated.h"

/** Global shared selection visual config */
UCLASS(BlueprintType)
class POLAIR_CS_API UPACS_SelectionGlobalConfig : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Selection")
    FSelectionVisualSet Visuals;
};

```

## Data/PACS_SelectionGlobalConfig.cpp
```cpp
#include "Data/PACS_SelectionGlobalConfig.h"

```

## Data/PACS_SelectionLocalConfig.h
```cpp
#pragma once

#include "CoreMinimal.h"
#include "Engine/PrimaryDataAsset.h"
#include "Data/PACS_SelectionTypes.h"
#include "PACS_SelectionLocalConfig.generated.h"

/** Optional per-actor override for selection visuals */
UCLASS(BlueprintType)
class POLAIR_CS_API UPACS_SelectionLocalConfig : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Selection")
    bool bOverrideGlobal = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Selection", meta=(EditCondition="bOverrideGlobal"))
    FSelectionVisualSet Visuals;
};

```

## Data/PACS_SelectionLocalConfig.cpp
```cpp
#include "Data/PACS_SelectionLocalConfig.h"

```

## Data/PACS_NPCArchetypeData.h
```cpp
#pragma once

#include "CoreMinimal.h"
#include "Engine/PrimaryDataAsset.h"
#include "PACS_NPCArchetypeData.generated.h"

/** What the NPC is: appearance & basic movement knobs */
UCLASS(BlueprintType)
class POLAIR_CS_API UPACS_NPCArchetypeData : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
    TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
    TSoftClassPtr<UAnimInstance> AnimBPClass;

    /** Slot index -> material override */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
    TMap<int32, TSoftObjectPtr<UMaterialInterface>> MaterialOverrides;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement", meta=(ClampMin="0.0"))
    float WalkSpeed = 150.f;
};

```

## Data/PACS_NPCArchetypeData.cpp
```cpp
#include "Data/PACS_NPCArchetypeData.h"

```

## Components/PACS_SelectionInputBridge.h
```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PACS_SelectionInputBridge.generated.h"

class APACS_SelectionCueProxy;
class APACS_NPCBase;
class APACS_PlayerController;

/**
 * Minimal bridge that runs a single line trace to drive local hover and
 * forwards selection clicks to the proxy (server-authoritative).
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class POLAIR_CS_API UPACS_SelectionInputBridge : public UActorComponent
{
    GENERATED_BODY()

public:
    UPACS_SelectionInputBridge();

    /** Enable/disable hover tracing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Selection")
    bool bEnableHoverTrace = true;

    /** Trace channel to use for selection */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Selection")
    TEnumAsByte<ECollisionChannel> SelectionTraceChannel = ECC_Visibility;

    /** Max trace distance */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Selection", meta=(ClampMin="100.0"))
    float TraceDistance = 50000.f;

    /** Call from input to attempt selection */
    UFUNCTION(BlueprintCallable, Category="Selection")
    void SelectOrRelease();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    TWeakObjectPtr<APACS_SelectionCueProxy> CurrentProxy;
    TWeakObjectPtr<APACS_PlayerController>  OwnerPC;

    void UpdateHover();
    APACS_SelectionCueProxy* FindProxyFromHit(const FHitResult& Hit) const;
    uint16 GetLocalShortId() const;
};

```

## Components/PACS_SelectionInputBridge.cpp
```cpp
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

```

