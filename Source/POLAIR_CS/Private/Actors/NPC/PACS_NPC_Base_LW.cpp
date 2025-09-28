#include "Actors/NPC/PACS_NPC_Base_LW.h"
#include "Components/BoxComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PACS_SelectionPlaneComponent.h"
#include "Data/PACS_SelectionProfile.h"
#include "Animation/AnimSequence.h"
#include "Net/UnrealNetwork.h"
#include "Engine/NetDriver.h"
#include "Engine/NetConnection.h"
#include "Engine/ActorChannel.h"
// SignificanceManager integration removed - using distance-based optimization instead
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerState.h"

APACS_NPC_Base_LW::APACS_NPC_Base_LW(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set default tick settings
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// Network settings
	bReplicates = true;
	SetReplicateMovement(false); // We handle custom replication
	SetNetUpdateFrequency(10.0f); // Lower frequency for lightweight NPCs
	SetMinNetUpdateFrequency(2.0f);

	// Create box collision component (more efficient than capsule for flat terrain)
	CollisionComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionComponent->SetBoxExtent(FVector(40.0f, 40.0f, 88.0f)); // Human-sized
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionComponent->SetCollisionProfileName(TEXT("Pawn"));
	RootComponent = CollisionComponent;

	// Create floating pawn movement (lightweight alternative to CharacterMovementComponent)
	FloatingMovement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("FloatingMovement"));
	FloatingMovement->UpdatedComponent = CollisionComponent;
	FloatingMovement->MaxSpeed = MovementSpeed;
	FloatingMovement->Acceleration = 1000.0f;
	FloatingMovement->Deceleration = 2000.0f;

	// Create skeletal mesh component
	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMesh"));
	SkeletalMeshComponent->SetupAttachment(RootComponent);
	SkeletalMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Position mesh at ground level (bottom of collision box)
	SkeletalMeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, -85.0f)); // Slight offset up from bottom
	SkeletalMeshComponent->bUseAttachParentBound = false; // Allow independent bounds calculation

	// Create selection plane component (ActorComponent, not SceneComponent)
	SelectionPlaneComponent = CreateDefaultSubobject<UPACS_SelectionPlaneComponent>(TEXT("SelectionPlane"));

	// Initialize movement state
	MovementStateRep = static_cast<uint8>(EPACSLightweightNPCMovementState::Idle);
}

void APACS_NPC_Base_LW::BeginPlay()
{
	Super::BeginPlay();

	// Register with significance manager
	RegisterWithSignificanceManager();

	// Set initial animation state
	PlayIdleAnimation();
}

void APACS_NPC_Base_LW::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unregister from significance manager
	UnregisterFromSignificanceManager();

	// Cancel any pending asset loads
	if (ProfileLoadHandle.IsValid())
	{
		ProfileLoadHandle->CancelHandle();
		ProfileLoadHandle.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void APACS_NPC_Base_LW::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Update movement only on server
	if (HasAuthority())
	{
		UpdateMovement(DeltaSeconds);
	}
}

void APACS_NPC_Base_LW::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Minimal replication for lightweight NPCs
	DOREPLIFETIME(APACS_NPC_Base_LW, MovementStateRep);
	DOREPLIFETIME(APACS_NPC_Base_LW, TargetLocation);
}

void APACS_NPC_Base_LW::OnAcquiredFromPool_Implementation()
{
	PrepareForUse();
}

void APACS_NPC_Base_LW::OnReturnedToPool_Implementation()
{
	ResetForPool();
}

void APACS_NPC_Base_LW::MoveToLocation(const FVector& InTargetLocation)
{
	if (!HasAuthority())
	{
		return;
	}

	TargetLocation = InTargetLocation;
	SetMovementState(EPACSLightweightNPCMovementState::Moving);

	// Calculate direction and rotate toward target
	FVector Direction = (TargetLocation - GetActorLocation()).GetSafeNormal();
	if (!Direction.IsNearlyZero())
	{
		SetActorRotation(Direction.Rotation());

		// Set velocity for movement
		FloatingMovement->Velocity = Direction * MovementSpeed;

		// Switch to run animation
		PlayRunAnimation();
	}
}

void APACS_NPC_Base_LW::StopMovement()
{
	if (!HasAuthority())
	{
		return;
	}

	SetMovementState(EPACSLightweightNPCMovementState::Idle);
	FloatingMovement->Velocity = FVector::ZeroVector;

	// Switch to idle animation
	PlayIdleAnimation();
}

void APACS_NPC_Base_LW::SetSelected(bool bNewSelected, APlayerState* Selector)
{
	bIsSelected = bNewSelected;
	CurrentSelector = Selector;

	UpdateSelectionVisuals();
}

void APACS_NPC_Base_LW::SetSelectionProfile(UPACS_SelectionProfileAsset* InProfile)
{
	if (!InProfile)
	{
		return;
	}

	// Cancel any existing load
	if (ProfileLoadHandle.IsValid())
	{
		ProfileLoadHandle->CancelHandle();
		ProfileLoadHandle.Reset();
	}

	// Store profile
	CurrentProfileAsset = InProfile;

	// Check if assets are already loaded (synchronous path)
	bool bAllAssetsLoaded = true;

	// Check if we can actually get the assets (they're loaded in memory)
	if (!InProfile->SkeletalMeshAsset.IsNull() && !InProfile->SkeletalMeshAsset.Get())
	{
		bAllAssetsLoaded = false;
	}
	if (!InProfile->IdleAnimationSequence.IsNull() && !InProfile->IdleAnimationSequence.Get())
	{
		bAllAssetsLoaded = false;
	}
	if (!InProfile->RunAnimationSequence.IsNull() && !InProfile->RunAnimationSequence.Get())
	{
		bAllAssetsLoaded = false;
	}

	if (bAllAssetsLoaded)
	{
		// Assets are already loaded, apply immediately
		UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_LW: Assets already loaded for %s, applying immediately"), *GetName());
		ApplyAnimationsFromProfile(InProfile);
		ApplySkeletalMeshFromProfile(InProfile);
		ApplySelectionProfile();
	}
	else
	{
		// Need to load assets asynchronously
		FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
		TArray<FSoftObjectPath> AssetsToLoad;

		if (!InProfile->SkeletalMeshAsset.IsNull())
		{
			AssetsToLoad.Add(InProfile->SkeletalMeshAsset.ToSoftObjectPath());
		}
		if (!InProfile->IdleAnimationSequence.IsNull())
		{
			AssetsToLoad.Add(InProfile->IdleAnimationSequence.ToSoftObjectPath());
		}
		if (!InProfile->RunAnimationSequence.IsNull())
		{
			AssetsToLoad.Add(InProfile->RunAnimationSequence.ToSoftObjectPath());
		}

		if (AssetsToLoad.Num() > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_LW: Loading assets async for %s"), *GetName());
			ProfileLoadHandle = StreamableManager.RequestAsyncLoad(
				AssetsToLoad,
				FStreamableDelegate::CreateUObject(this, &APACS_NPC_Base_LW::OnSelectionProfileLoaded),
				FStreamableManager::AsyncLoadHighPriority
			);
		}
	}

	// Apply selection plane profile regardless of asset loading
	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->ApplyProfileAsset(InProfile);
	}
}

void APACS_NPC_Base_LW::ApplySelectionProfile()
{
	// Apply selection visuals through the selection plane component
	if (SelectionPlaneComponent && CurrentProfileAsset)
	{
		SelectionPlaneComponent->ApplyProfileAsset(CurrentProfileAsset);
	}
}

void APACS_NPC_Base_LW::UpdateSignificance(float NewSignificance)
{
	CurrentSignificance = NewSignificance;

	// Adjust tick rate and component updates based on significance
	if (NewSignificance < 0.3f) // Far away
	{
		// Reduce tick rate significantly
		SetActorTickInterval(0.5f);

		// Disable animation updates
		if (SkeletalMeshComponent)
		{
			SkeletalMeshComponent->SetComponentTickEnabled(false);
		}

		// Lower movement update rate
		if (FloatingMovement)
		{
			FloatingMovement->SetComponentTickInterval(0.2f);
		}
	}
	else if (NewSignificance < 0.7f) // Medium distance
	{
		// Moderate tick rate
		SetActorTickInterval(0.2f);

		// Enable animation with lower update rate
		if (SkeletalMeshComponent)
		{
			SkeletalMeshComponent->SetComponentTickEnabled(true);
			SkeletalMeshComponent->SetComponentTickInterval(0.1f);
		}

		// Normal movement updates
		if (FloatingMovement)
		{
			FloatingMovement->SetComponentTickInterval(0.1f);
		}
	}
	else // Close
	{
		// Full update rate
		SetActorTickInterval(0.0f);

		// Full animation fidelity
		if (SkeletalMeshComponent)
		{
			SkeletalMeshComponent->SetComponentTickEnabled(true);
			SkeletalMeshComponent->SetComponentTickInterval(0.0f);
		}

		// Full movement updates
		if (FloatingMovement)
		{
			FloatingMovement->SetComponentTickInterval(0.0f);
		}
	}
}

float APACS_NPC_Base_LW::GetNetPriority(const FVector& ViewPos, const FVector& ViewDir,
                                        AActor* Viewer, AActor* ViewTarget,
                                        UActorChannel* InChannel, float Time,
                                        bool bLowBandwidth)
{
	// Lower network priority for lightweight NPCs (50% of normal)
	return Super::GetNetPriority(ViewPos, ViewDir, Viewer, ViewTarget, InChannel, Time, bLowBandwidth) * 0.5f;
}

void APACS_NPC_Base_LW::OnRep_MovementState()
{
	UpdateAnimationState();
}

void APACS_NPC_Base_LW::OnRep_TargetLocation()
{
	// On clients, update rotation to face target
	if (!HasAuthority() && GetMovementState() == EPACSLightweightNPCMovementState::Moving)
	{
		FVector Direction = (TargetLocation - GetActorLocation()).GetSafeNormal();
		if (!Direction.IsNearlyZero())
		{
			SetActorRotation(Direction.Rotation());
		}
	}
}

void APACS_NPC_Base_LW::PlayIdleAnimation()
{
	if (!SkeletalMeshComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_LW::PlayIdleAnimation: No skeletal mesh component on %s"), *GetName());
		return;
	}

	if (!SkeletalMeshComponent->GetSkeletalMeshAsset())
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_LW::PlayIdleAnimation: No skeletal mesh set on %s"), *GetName());
		return;
	}

	if (IdleAnimation)
	{
		UE_LOG(LogTemp, VeryVerbose, TEXT("PACS_NPC_Base_LW::PlayIdleAnimation: Playing idle on %s"), *GetName());
		SkeletalMeshComponent->PlayAnimation(IdleAnimation, true);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_LW::PlayIdleAnimation: No idle animation on %s"), *GetName());
	}
}

void APACS_NPC_Base_LW::PlayRunAnimation()
{
	if (!SkeletalMeshComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_LW::PlayRunAnimation: No skeletal mesh component on %s"), *GetName());
		return;
	}

	if (!SkeletalMeshComponent->GetSkeletalMeshAsset())
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_LW::PlayRunAnimation: No skeletal mesh set on %s"), *GetName());
		return;
	}

	if (RunAnimation)
	{
		UE_LOG(LogTemp, VeryVerbose, TEXT("PACS_NPC_Base_LW::PlayRunAnimation: Playing run on %s"), *GetName());
		SkeletalMeshComponent->PlayAnimation(RunAnimation, true);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_LW::PlayRunAnimation: No run animation on %s"), *GetName());
	}
}

void APACS_NPC_Base_LW::UpdateAnimationState()
{
	switch (GetMovementState())
	{
		case EPACSLightweightNPCMovementState::Idle:
			PlayIdleAnimation();
			break;
		case EPACSLightweightNPCMovementState::Moving:
			PlayRunAnimation();
			break;
	}
}

void APACS_NPC_Base_LW::UpdateMovement(float DeltaSeconds)
{
	if (GetMovementState() == EPACSLightweightNPCMovementState::Moving)
	{
		// Check if we've reached the target
		if (HasReachedTarget())
		{
			StopMovement();
		}
		else
		{
			// Update direction to target
			FVector Direction = (TargetLocation - GetActorLocation()).GetSafeNormal();
			if (!Direction.IsNearlyZero())
			{
				FloatingMovement->Velocity = Direction * MovementSpeed;
			}
		}
	}
}

void APACS_NPC_Base_LW::UpdateSelectionVisuals()
{
	if (SelectionPlaneComponent)
	{
		// Convert bool to proper enum state
		ESelectionVisualState NewState = bIsSelected ? ESelectionVisualState::Selected : ESelectionVisualState::Available;
		SelectionPlaneComponent->SetSelectionState(NewState);
	}
}

void APACS_NPC_Base_LW::ResetForPool()
{
	// Reset movement
	StopMovement();
	TargetLocation = FVector::ZeroVector;

	// Reset selection
	bIsSelected = false;
	CurrentSelector = nullptr;

	// Reset visuals
	UpdateSelectionVisuals();

	// Hide actor
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
}

void APACS_NPC_Base_LW::PrepareForUse()
{
	// Show actor
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorTickEnabled(true);

	// Ensure skeletal mesh is visible
	if (SkeletalMeshComponent)
	{
		SkeletalMeshComponent->SetVisibility(true);
		SkeletalMeshComponent->SetHiddenInGame(false);
		SkeletalMeshComponent->UpdateBounds();

		UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_LW::PrepareForUse: %s - Mesh=%s, Visible=%s, Location=%s"),
			*GetName(),
			SkeletalMeshComponent->GetSkeletalMeshAsset() ? *SkeletalMeshComponent->GetSkeletalMeshAsset()->GetName() : TEXT("None"),
			SkeletalMeshComponent->IsVisible() ? TEXT("true") : TEXT("false"),
			*GetActorLocation().ToString());
	}

	// Reset to idle state
	SetMovementState(EPACSLightweightNPCMovementState::Idle);
	PlayIdleAnimation();
}

void APACS_NPC_Base_LW::OnSelectionProfileLoaded()
{
	if (CurrentProfileAsset)
	{
		ApplyAnimationsFromProfile(CurrentProfileAsset);
		ApplySkeletalMeshFromProfile(CurrentProfileAsset);
		ApplySelectionProfile();
	}

	ProfileLoadHandle.Reset();
}

void APACS_NPC_Base_LW::ApplyAnimationsFromProfile(UPACS_SelectionProfileAsset* ProfileAsset)
{
	if (!ProfileAsset)
	{
		return;
	}

	// Load idle animation
	if (ProfileAsset->IdleAnimationSequence.IsValid())
	{
		IdleAnimation = ProfileAsset->IdleAnimationSequence.Get();
	}

	// Load run animation
	if (ProfileAsset->RunAnimationSequence.IsValid())
	{
		RunAnimation = ProfileAsset->RunAnimationSequence.Get();
	}

	// Update current animation based on state
	UpdateAnimationState();
}

void APACS_NPC_Base_LW::ApplySkeletalMeshFromProfile(UPACS_SelectionProfileAsset* ProfileAsset)
{
	if (!ProfileAsset || !SkeletalMeshComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("PACS_NPC_Base_LW::ApplySkeletalMeshFromProfile: Invalid profile or mesh component for %s"),
			*GetName());
		return;
	}

	// Try to get the skeletal mesh
	USkeletalMesh* NewMesh = ProfileAsset->SkeletalMeshAsset.Get();
	if (!NewMesh)
	{
		// Try loading synchronously if not already loaded
		NewMesh = ProfileAsset->SkeletalMeshAsset.LoadSynchronous();
	}

	if (NewMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_LW: Setting skeletal mesh %s on %s"),
			*NewMesh->GetName(), *GetName());

		// Apply the mesh
		SkeletalMeshComponent->SetSkeletalMesh(NewMesh);

		// Apply the transform from the profile but preserve our custom Z offset
		FTransform ProfileTransform = ProfileAsset->SkeletalMeshTransform;
		ProfileTransform.SetLocation(FVector(0.0f, 0.0f, -85.0f)); // Keep our ground-level positioning
		SkeletalMeshComponent->SetRelativeTransform(ProfileTransform);

		// Ensure visibility
		SkeletalMeshComponent->SetVisibility(true);
		SkeletalMeshComponent->SetHiddenInGame(false);
		SkeletalMeshComponent->SetRenderCustomDepth(false); // Ensure not hidden by custom depth

		// Force update bounds
		SkeletalMeshComponent->UpdateBounds();
		SkeletalMeshComponent->MarkRenderStateDirty();

		UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_LW: Mesh applied. Visibility=%s, HiddenInGame=%s, Location=%s"),
			SkeletalMeshComponent->IsVisible() ? TEXT("true") : TEXT("false"),
			SkeletalMeshComponent->bHiddenInGame ? TEXT("true") : TEXT("false"),
			*SkeletalMeshComponent->GetComponentLocation().ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("PACS_NPC_Base_LW: Failed to load skeletal mesh for %s"), *GetName());
	}
}

void APACS_NPC_Base_LW::SetMovementState(EPACSLightweightNPCMovementState NewState)
{
	MovementStateRep = static_cast<uint8>(NewState);

	// Update animation locally on server
	if (HasAuthority())
	{
		UpdateAnimationState();
	}
}

bool APACS_NPC_Base_LW::HasReachedTarget() const
{
	float DistanceToTarget = FVector::Dist(GetActorLocation(), TargetLocation);
	return DistanceToTarget <= StoppingDistance;
}

void APACS_NPC_Base_LW::RegisterWithSignificanceManager()
{
	// Simplified distance-based significance without SignificanceManager module
	// This will be handled by manual distance checks in Tick
}

void APACS_NPC_Base_LW::UnregisterFromSignificanceManager()
{
	// Cleanup handled in EndPlay
}