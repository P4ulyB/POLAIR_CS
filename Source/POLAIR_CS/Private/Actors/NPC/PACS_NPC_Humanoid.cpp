// Copyright Notice: Property of Particular Audience Limited

#include "Actors/NPC/PACS_NPC_Humanoid.h"
#include "Data/Configs/PACS_NPC_v2_Config.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/DecalComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Animation/AnimSequence.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/PlayerState.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Net/UnrealNetwork.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

APACS_NPC_Humanoid::APACS_NPC_Humanoid()
{
    // Dead simple setup - minimal ticking
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.05f; // 20 FPS for NPCs

    // Network setup
    bReplicates = true;
    SetReplicateMovement(true);
    SetNetUpdateFrequency(10.0f); // Low frequency updates

    // Root component - box collision for better click detection
    CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
    CollisionBox->SetBoxExtent(FVector(35.0f, 35.0f, 90.0f)); // Human-sized box
    RootComponent = CollisionBox;

    // Skeletal mesh - no collision, no physics
    SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMesh"));
    SkeletalMeshComponent->SetupAttachment(RootComponent);
    SkeletalMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SkeletalMeshComponent->SetSimulatePhysics(false);
    SkeletalMeshComponent->bDisableClothSimulation = true;
    SkeletalMeshComponent->CastShadow = false;
    SkeletalMeshComponent->bCastDynamicShadow = false;
    SkeletalMeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f)); // Align with box bottom

    // Lightweight movement component
    FloatingMovement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("FloatingMovement"));
    FloatingMovement->MaxSpeed = 300.0f;
    FloatingMovement->Acceleration = 500.0f;
    FloatingMovement->Deceleration = 500.0f;

    // Selection decal
    SelectionDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("SelectionDecal"));
    SelectionDecal->SetupAttachment(RootComponent);
    SelectionDecal->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
    SelectionDecal->DecalSize = FVector(32.0f, 64.0f, 64.0f);
    SelectionDecal->SetVisibility(false);

    // Set AI controller class for movement
    AIControllerClass = AAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    // Initialize state
    bIsMoving = false;
    bIsLocallyHovered = false;
    ConfiguredWalkSpeed = 300.0f;
    CachedDecalMaterial = nullptr;
}

void APACS_NPC_Humanoid::BeginPlay()
{
    Super::BeginPlay();

    // Load assets from config
    LoadAssetsFromConfig();

    // Start with idle animation
    UpdateAnimation();

    // Start dormant on server
    if (HasAuthority())
    {
        SetNetDormancy(DORM_Initial);
    }
}

void APACS_NPC_Humanoid::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Update animation based on movement state
    UpdateAnimation();
}

void APACS_NPC_Humanoid::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(APACS_NPC_Humanoid, CurrentSelector);
    DOREPLIFETIME(APACS_NPC_Humanoid, TargetLocation);
    DOREPLIFETIME(APACS_NPC_Humanoid, bIsMoving);
}

void APACS_NPC_Humanoid::LoadAssetsFromConfig()
{
    if (!NPCConfig)
    {
        return;
    }

    // Load skeletal mesh
    if (!NPCConfig->SkeletalMesh.IsNull())
    {
        USkeletalMesh* LoadedMesh = NPCConfig->SkeletalMesh.LoadSynchronous();
        if (LoadedMesh && SkeletalMeshComponent)
        {
            SkeletalMeshComponent->SetSkeletalMesh(LoadedMesh);
        }
    }

    // Load animations
    if (!NPCConfig->IdleAnimation.IsNull())
    {
        LoadedIdleAnimation = NPCConfig->IdleAnimation.LoadSynchronous();
    }

    if (!NPCConfig->MoveAnimation.IsNull())
    {
        LoadedMoveAnimation = NPCConfig->MoveAnimation.LoadSynchronous();
    }

    // Set walk speed
    ConfiguredWalkSpeed = NPCConfig->WalkSpeed;
    if (FloatingMovement)
    {
        FloatingMovement->MaxSpeed = ConfiguredWalkSpeed;
    }
}

void APACS_NPC_Humanoid::UpdateAnimation()
{
    if (!SkeletalMeshComponent)
    {
        return;
    }

    // Dead simple animation logic
    UAnimSequence* TargetAnimation = bIsMoving ? LoadedMoveAnimation : LoadedIdleAnimation;

    if (!TargetAnimation)
    {
        return;
    }

    // Always play the target animation - PlayAnimation handles switching internally
    SkeletalMeshComponent->PlayAnimation(TargetAnimation, true);

    // Adjust playback speed based on movement speed
    if (bIsMoving && LoadedMoveAnimation)
    {
        float CurrentSpeed = GetVelocity().Size();
        float PlayRate = FMath::Clamp(CurrentSpeed / ConfiguredWalkSpeed, 0.5f, 2.0f);
        SkeletalMeshComponent->SetPlayRate(PlayRate);
    }
}

void APACS_NPC_Humanoid::MoveToLocation(const FVector& Location)
{
    if (HasAuthority())
    {
        ServerMoveToLocation_Implementation(Location);
    }
    else
    {
        ServerMoveToLocation(Location);
    }
}

void APACS_NPC_Humanoid::ServerMoveToLocation_Implementation(FVector Location)
{
    if (!HasAuthority())
    {
        return;
    }

    // Simple movement using AI controller
    if (AAIController* AIController = Cast<AAIController>(GetController()))
    {
        // Wake from dormancy when moving
        FlushNetDormancy();

        // Simple move to location
        AIController->MoveToLocation(Location, 50.0f);

        // Update state
        TargetLocation = Location;
        bIsMoving = true;

        // Bind to movement completion
        if (UPathFollowingComponent* PathComp = AIController->GetPathFollowingComponent())
        {
            PathComp->OnRequestFinished.AddUObject(this, &APACS_NPC_Humanoid::OnMoveCompleted);
        }
    }
}

void APACS_NPC_Humanoid::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
    if (!HasAuthority())
    {
        return;
    }

    // Movement done
    bIsMoving = false;

    // Return to dormancy if not selected
    if (!CurrentSelector)
    {
        SetNetDormancy(DORM_Initial);
    }
}

void APACS_NPC_Humanoid::SetCurrentSelector(APlayerState* Selector)
{
    if (!HasAuthority())
    {
        return;
    }

    CurrentSelector = Selector;

    // Wake from dormancy when selected
    if (CurrentSelector)
    {
        FlushNetDormancy();
    }
    // Return to dormancy when deselected and not moving
    else if (!bIsMoving)
    {
        SetNetDormancy(DORM_Initial);
    }
}

void APACS_NPC_Humanoid::SetLocalHover(bool bHovered)
{
    bIsLocallyHovered = bHovered;
    UpdateVisualState();
}

void APACS_NPC_Humanoid::OnRep_CurrentSelector()
{
    // Update visual state when selector changes
    UpdateVisualState();
}

void APACS_NPC_Humanoid::UpdateVisualState()
{
    if (!SelectionDecal || !NPCConfig || !CachedDecalMaterial)
    {
        return;
    }

    // Get local player state for comparison
    APlayerState* LocalPlayerState = nullptr;
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        LocalPlayerState = PC->GetPlayerState<APlayerState>();
    }

    // Determine visual state based on priority:
    // 1. Local hover (highest priority)
    // 2. Selected by local player
    // 3. Selected by another player (unavailable)
    // 4. Available (default)

    if (bIsLocallyHovered)
    {
        // LOCAL ONLY - Hover state (highest priority)
        ApplyDecalState(NPCConfig->HoveredBrightness, NPCConfig->HoveredColor);
        SelectionDecal->SetVisibility(true);
    }
    else if (CurrentSelector == LocalPlayerState && CurrentSelector != nullptr)
    {
        // This client owns the selection - show Selected state
        ApplyDecalState(NPCConfig->SelectedBrightness, NPCConfig->SelectedColor);
        SelectionDecal->SetVisibility(true);
    }
    else if (CurrentSelector != nullptr)
    {
        // Another client owns this NPC - show Unavailable state
        ApplyDecalState(NPCConfig->UnavailableBrightness, NPCConfig->UnavailableColor);
        SelectionDecal->SetVisibility(true);
    }
    else
    {
        // No one has selected - show Available state
        ApplyDecalState(NPCConfig->AvailableBrightness, NPCConfig->AvailableColor);
        SelectionDecal->SetVisibility(false); // Hide decal when available
    }
}

void APACS_NPC_Humanoid::ApplyDecalState(float Brightness, const FLinearColor& Color)
{
    if (!CachedDecalMaterial)
    {
        return;
    }

    // Update material parameters
    CachedDecalMaterial->SetScalarParameterValue(TEXT("Brightness"), Brightness);
    CachedDecalMaterial->SetVectorParameterValue(TEXT("Color"), Color);
}