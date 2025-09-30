#include "Actors/NPC/PACS_NPC_Base_Char.h"
#include "Components/PACS_SelectionPlaneComponent.h"
#include "Data/PACS_SelectionProfile.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Components/SkeletalMeshComponent.h"

APACS_NPC_Base_Char::APACS_NPC_Base_Char()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create selection plane component (manages state and client-side visuals)
	// The component itself handles creating visual elements ONLY on non-VR clients
	SelectionPlaneComponent = CreateDefaultSubobject<UPACS_SelectionPlaneComponent>(TEXT("SelectionPlaneComponent"));
	SelectionPlaneComponent->SetIsReplicated(true);

	// Configure character movement for NPCs
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->bOrientRotationToMovement = true;
		GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
		GetCharacterMovement()->MaxWalkSpeed = DefaultMaxWalkSpeed;
		GetCharacterMovement()->MinAnalogWalkSpeed = 20.0f;
		GetCharacterMovement()->BrakingDecelerationWalking = 2000.0f;
	}

	// Configure the mesh component inherited from ACharacter
	// CRITICAL: Log constructor mesh transform to detect Blueprint CDO overrides
	if (GetMesh())
	{
		FTransform ConstructorMeshTransform = GetMesh()->GetRelativeTransform();
		UE_LOG(LogTemp, Warning, TEXT("APACS_NPC_Base_Char::Constructor: Initial mesh transform - Loc=%s, Rot=%s, Scale=%s"),
			*ConstructorMeshTransform.GetLocation().ToString(),
			*ConstructorMeshTransform.Rotator().ToString(),
			*ConstructorMeshTransform.GetScale3D().ToString());
	}

	// Replication settings for multiplayer
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(10.0f);
	SetMinNetUpdateFrequency(2.0f);

	// Set AI controller class (can be overridden in Blueprint)
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void APACS_NPC_Base_Char::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Replicate the cached profile data struct (atomic replication)
	DOREPLIFETIME(APACS_NPC_Base_Char, CachedProfileData);
}

void APACS_NPC_Base_Char::OnRep_CachedProfileData()
{
	UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::OnRep_CachedProfileData: Called on client for %s"), *GetName());

	// All profile data arrived atomically - apply in controlled sequence
	ApplyCachedProfileData();
}

void APACS_NPC_Base_Char::ApplyCachedProfileData()
{
	UE_LOG(LogTemp, Warning, TEXT("======================================================================"));
	UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::ApplyCachedProfileData: START for %s [Authority=%s]"),
		*GetName(), HasAuthority() ? TEXT("Server") : TEXT("Client"));

	// Validate cached data
	if (!CachedProfileData.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("PACS_NPC_Base_Char::ApplyCachedProfileData: INVALID cached profile data for %s"), *GetName());
		UE_LOG(LogTemp, Error, TEXT("  - SkeletalMeshAsset.IsNull() = %s"), CachedProfileData.SkeletalMeshAsset.IsNull() ? TEXT("TRUE") : TEXT("FALSE"));
		UE_LOG(LogTemp, Error, TEXT("  - StaticMeshAsset.IsNull() = %s"), CachedProfileData.StaticMeshAsset.IsNull() ? TEXT("TRUE") : TEXT("FALSE"));
		return;
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		UE_LOG(LogTemp, Error, TEXT("PACS_NPC_Base_Char::ApplyCachedProfileData: NO MESH COMPONENT for %s"), *GetName());
		return;
	}

	// LOG CACHED DATA BEFORE APPLICATION
	UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::ApplyCachedProfileData: Cached data snapshot:"));
	UE_LOG(LogTemp, Warning, TEXT("  - SkeletalMeshAsset: %s"), *CachedProfileData.SkeletalMeshAsset.ToString());
	UE_LOG(LogTemp, Warning, TEXT("  - SkeletalMesh Location: %s"), *CachedProfileData.SkeletalMeshLocation.ToString());
	UE_LOG(LogTemp, Warning, TEXT("  - SkeletalMesh Rotation: %s"), *CachedProfileData.SkeletalMeshRotation.ToString());
	UE_LOG(LogTemp, Warning, TEXT("  - SkeletalMesh Scale: %s"), *CachedProfileData.SkeletalMeshScale.ToString());

	// STEP 1: Load and apply skeletal mesh FIRST (required before transform/materials)
	if (!CachedProfileData.SkeletalMeshAsset.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::ApplyCachedProfileData: [STEP 1] Loading skeletal mesh..."));

		USkeletalMesh* LoadedMesh = CachedProfileData.SkeletalMeshAsset.LoadSynchronous();
		if (LoadedMesh)
		{
			UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::ApplyCachedProfileData: [STEP 1] Mesh loaded successfully: %s"), *LoadedMesh->GetName());

			// Store old mesh for comparison
			USkeletalMesh* OldMesh = MeshComp->GetSkeletalMeshAsset();

			MeshComp->SetSkeletalMesh(LoadedMesh);

			// Verify mesh was set
			USkeletalMesh* NewMesh = MeshComp->GetSkeletalMeshAsset();
			if (NewMesh == LoadedMesh)
			{
				UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::ApplyCachedProfileData: [STEP 1] VERIFIED - Mesh applied successfully (was %s, now %s)"),
					OldMesh ? *OldMesh->GetName() : TEXT("null"),
					*NewMesh->GetName());
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("PACS_NPC_Base_Char::ApplyCachedProfileData: [STEP 1] VERIFICATION FAILED - Mesh not set correctly!"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("PACS_NPC_Base_Char::ApplyCachedProfileData: [STEP 1] FAILED to load skeletal mesh from %s"),
				*CachedProfileData.SkeletalMeshAsset.ToString());
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::ApplyCachedProfileData: [STEP 1] No skeletal mesh in cached data"));
	}

	// STEP 2: Apply transform using ACharacter's BaseOffset system
	// CRITICAL: ACharacter overrides direct mesh transforms with BaseTranslationOffset/BaseRotationOffset
	// We MUST use these properties instead of SetRelativeTransform to avoid being overridden
	UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::ApplyCachedProfileData: [STEP 2] Applying transform via BaseOffsets..."));

	// Log current base offsets BEFORE
	UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::ApplyCachedProfileData: [STEP 2] Old BaseTranslationOffset=%s, BaseRotationOffset=%s"),
		*BaseTranslationOffset.ToString(),
		*BaseRotationOffset.ToString());

	// Set the CHARACTER's base offsets (not the mesh component directly!)
	// These are persistent and won't be overridden by UpdateMeshForCapsuleSize
	BaseTranslationOffset = CachedProfileData.SkeletalMeshLocation;
	BaseRotationOffset = CachedProfileData.SkeletalMeshRotation.Quaternion();

	// Log new base offsets
	UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::ApplyCachedProfileData: [STEP 2] New BaseTranslationOffset=%s, BaseRotationOffset=%s"),
		*BaseTranslationOffset.ToString(),
		*BaseRotationOffset.Rotator().ToString());

	// Now apply the base offsets to the mesh component
	// This mimics what ACharacter::PostInitializeComponents does
	if (MeshComp)
	{
		// Apply location with capsule height adjustment (Epic's pattern)
		FVector AdjustedLocation = BaseTranslationOffset;
		if (GetCapsuleComponent())
		{
			// ACharacter adjusts Z based on capsule half-height
			const float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			AdjustedLocation.Z = BaseTranslationOffset.Z - CapsuleHalfHeight;
		}

		// Apply both location and rotation
		MeshComp->SetRelativeLocationAndRotation(
			AdjustedLocation,
			BaseRotationOffset
		);

		// Apply scale separately (BaseOffsets don't include scale)
		MeshComp->SetRelativeScale3D(CachedProfileData.SkeletalMeshScale);

		// Verify transform was applied
		FTransform NewTransform = MeshComp->GetRelativeTransform();
		UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::ApplyCachedProfileData: [STEP 2] Final Mesh Transform - Loc=%s, Rot=%s, Scale=%s"),
			*NewTransform.GetLocation().ToString(),
			*NewTransform.Rotator().ToString(),
			*NewTransform.GetScale3D().ToString());

		UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::ApplyCachedProfileData: [STEP 2] BaseOffsets applied successfully"));
	}

	// STEP 3: Apply animations (requires mesh to be set)
	// Note: Animation application is handled separately by animation system
	if (!CachedProfileData.IdleAnimationSequence.IsNull())
	{
		UE_LOG(LogTemp, Log, TEXT("PACS_NPC_Base_Char::ApplyCachedProfileData: [STEP 3] Idle animation cached: %s"),
			*CachedProfileData.IdleAnimationSequence.ToString());
	}

	// STEP 4: Apply selection plane settings (handled by SelectionPlaneComponent)
	if (SelectionPlaneComponent)
	{
		// Selection component will read from CachedProfileData when needed
		UE_LOG(LogTemp, Log, TEXT("PACS_NPC_Base_Char::ApplyCachedProfileData: [STEP 4] Selection component present"));
	}

	UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::ApplyCachedProfileData: COMPLETE for %s"), *GetName());
	UE_LOG(LogTemp, Warning, TEXT("======================================================================"));
}

void APACS_NPC_Base_Char::BeginPlay()
{
	UE_LOG(LogTemp, Warning, TEXT("APACS_NPC_Base_Char::BeginPlay: START for %s [Authority=%s]"),
		*GetName(), HasAuthority() ? TEXT("Server") : TEXT("Client"));

	// Log mesh transform BEFORE Super::BeginPlay
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		FTransform BeforeSuper = MeshComp->GetRelativeTransform();
		UE_LOG(LogTemp, Warning, TEXT("APACS_NPC_Base_Char::BeginPlay: Mesh transform BEFORE Super::BeginPlay - Loc=%s, Rot=%s, Scale=%s"),
			*BeforeSuper.GetLocation().ToString(),
			*BeforeSuper.Rotator().ToString(),
			*BeforeSuper.GetScale3D().ToString());
	}

	Super::BeginPlay();

	// Log mesh transform AFTER Super::BeginPlay
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		FTransform AfterSuper = MeshComp->GetRelativeTransform();
		UE_LOG(LogTemp, Warning, TEXT("APACS_NPC_Base_Char::BeginPlay: Mesh transform AFTER Super::BeginPlay - Loc=%s, Rot=%s, Scale=%s"),
			*AfterSuper.GetLocation().ToString(),
			*AfterSuper.Rotator().ToString(),
			*AfterSuper.GetScale3D().ToString());
	}

	// Store default walk speed
	if (GetCharacterMovement())
	{
		DefaultMaxWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
	}

	// Initialize selection plane component
	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->InitializeSelectionPlane();
		SelectionPlaneComponent->SetSelectionPlaneVisible(false);

		// Apply selection profile if set
		ApplySelectionProfile();
	}

	// Log final mesh transform after BeginPlay complete
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		FTransform Final = MeshComp->GetRelativeTransform();
		UE_LOG(LogTemp, Warning, TEXT("APACS_NPC_Base_Char::BeginPlay: FINAL mesh transform - Loc=%s, Rot=%s, Scale=%s"),
			*Final.GetLocation().ToString(),
			*Final.Rotator().ToString(),
			*Final.GetScale3D().ToString());
	}

	UE_LOG(LogTemp, Warning, TEXT("APACS_NPC_Base_Char::BeginPlay: COMPLETE for %s"), *GetName());
}

void APACS_NPC_Base_Char::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clear selection if selected
	if (bIsSelected)
	{
		SetSelected(false, nullptr);
	}

	// Stop any movement
	StopMovement();

	Super::EndPlay(EndPlayReason);
}

void APACS_NPC_Base_Char::OnAcquiredFromPool_Implementation()
{
	PrepareForUse();

	// Notify selection component
	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->OnAcquiredFromPool();
	}
}

void APACS_NPC_Base_Char::OnReturnedToPool_Implementation()
{
	ResetForPool();

	// Notify selection component
	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->OnReturnedToPool();
	}
}

void APACS_NPC_Base_Char::SetSelected(bool bNewSelected, APlayerState* Selector)
{
	if (bIsSelected == bNewSelected && CurrentSelector == Selector)
	{
		return;
	}

	bIsSelected = bNewSelected;
	CurrentSelector = bNewSelected ? Selector : nullptr;

	UpdateSelectionVisuals();

	// Update selection plane component
	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->SetSelectionState(bIsSelected ? ESelectionVisualState::Selected : ESelectionVisualState::Available);
	}

	UE_LOG(LogTemp, Log, TEXT("PACS_NPC_Base_Char: %s %s by %s"),
		*GetName(),
		bIsSelected ? TEXT("selected") : TEXT("deselected"),
		Selector ? *Selector->GetPlayerName() : TEXT("None"));
}

void APACS_NPC_Base_Char::MoveToLocation(const FVector& TargetLocation)
{
	if (!HasAuthority())
	{
		return;
	}

	// Get AI controller and move to location
	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->MoveToLocation(TargetLocation, 5.0f, true, true, true, false);

		UE_LOG(LogTemp, Log, TEXT("PACS_NPC_Base_Char: %s moving to %s"),
			*GetName(), *TargetLocation.ToString());
	}
}

void APACS_NPC_Base_Char::StopMovement()
{
	if (!HasAuthority())
	{
		return;
	}

	// Stop AI movement
	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->StopMovement();
	}

	// Stop character movement
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->StopMovementImmediately();
	}
}

void APACS_NPC_Base_Char::UpdateSelectionVisuals()
{
	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->SetSelectionPlaneVisible(bIsSelected);
	}
}

void APACS_NPC_Base_Char::ResetForPool()
{
	UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::ResetForPool: Called for %s"), *GetName());

	// Clear selection
	bIsSelected = false;
	CurrentSelector = nullptr;

	// Hide selection plane
	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->SetSelectionPlaneVisible(false);
	}

	// Stop all movement
	StopMovement();

	// Reset character-specific state
	ResetCharacterMovement();
	ResetCharacterAnimation();

	// Reset ACTOR transform (NOT mesh component transform)
	// The actor transform is set by PrepareActorForUse based on spawn location
	SetActorTransform(FTransform::Identity);

	// IMPORTANT: DO NOT reset the mesh component's relative transform here
	// The mesh transform is part of the cached profile data and should persist
	// This is critical for proper replication of skeletal mesh transforms

	// Clear velocity
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->Velocity = FVector::ZeroVector;
		GetCharacterMovement()->StopMovementImmediately();
	}

	UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::ResetForPool: Complete for %s"), *GetName());
}

void APACS_NPC_Base_Char::PrepareForUse()
{
	UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::PrepareForUse: Called for %s"), *GetName());

	// Re-enable collision
	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	// Restore movement settings
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = DefaultMaxWalkSpeed;
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}

	// Ensure selection plane is hidden initially
	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->SetSelectionPlaneVisible(false);
	}

	// Log current mesh transform to verify it hasn't been cleared
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		FTransform CurrentTransform = MeshComp->GetRelativeTransform();
		UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::PrepareForUse: Current mesh transform - Loc=%s, Rot=%s, Scale=%s"),
			*CurrentTransform.GetLocation().ToString(),
			*CurrentTransform.Rotator().ToString(),
			*CurrentTransform.GetScale3D().ToString());
	}

	// Selection profile is now applied by spawn orchestrator when acquired from pool
	// This follows Principle #4: Pool Pre-configured NPCs

	UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::PrepareForUse: Complete for %s"), *GetName());
}

void APACS_NPC_Base_Char::SetSelectionProfile(UPACS_SelectionProfileAsset* InProfile)
{
	// Only server should set the profile to ensure consistency
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::SetSelectionProfile: Called without authority for %s"), *GetName());
		return;
	}

	if (!InProfile)
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::SetSelectionProfile: Null profile for %s"), *GetName());
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::SetSelectionProfile: Caching profile %s for %s"),
		*InProfile->GetName(), *GetName());

	// CACHE all profile data into replicated struct
	// This populates the struct and triggers replication to clients automatically
	CachedProfileData.PopulateFromProfile(InProfile);

	// Apply cached data locally on server
	ApplyCachedProfileData();

	// Apply to selection plane component (for server/listen server visuals)
	if (GetWorld() && GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		if (SelectionPlaneComponent)
		{
			// Selection component can now read from CachedProfileData
			SelectionPlaneComponent->ApplyProfileAsset(InProfile);
			UE_LOG(LogTemp, Log, TEXT("PACS_NPC_Base_Char::SetSelectionProfile: Applied to selection plane"));
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::SetSelectionProfile: Complete for %s"), *GetName());
}

void APACS_NPC_Base_Char::ApplySelectionProfile()
{
	// This method is now deprecated - selection profiles are applied directly
	// via SetSelectionProfile which is called by the spawn orchestrator
	// with preloaded profiles (following Principle #1: Pre-load Selection Profiles)
}

void APACS_NPC_Base_Char::ResetCharacterMovement()
{
	if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
	{
		// Reset movement mode
		MovementComp->SetMovementMode(MOVE_Walking);

		// Clear movement state
		MovementComp->StopMovementImmediately();
		MovementComp->Velocity = FVector::ZeroVector;
		// Acceleration is protected, cleared by StopMovementImmediately

		// Reset movement properties
		MovementComp->MaxWalkSpeed = DefaultMaxWalkSpeed;
		MovementComp->bOrientRotationToMovement = true;
	}
}

void APACS_NPC_Base_Char::ResetCharacterAnimation()
{
	// Stop any playing montages
	if (GetMesh() && GetMesh()->GetAnimInstance())
	{
		GetMesh()->GetAnimInstance()->StopAllMontages(0.0f);

		// Reset animation state
		GetMesh()->GetAnimInstance()->ResetDynamics(ETeleportType::ResetPhysics);
	}
}

void APACS_NPC_Base_Char::OnSelectionProfileLoaded()
{
	// This method is now deprecated - selection profiles are preloaded
	// by the spawn orchestrator and applied directly via SetSelectionProfile
	// (following Principle #1: Pre-load Selection Profiles)
}

// NOTE: ApplySkeletalMeshFromProfile has been removed and replaced with ApplyCachedProfileData()
// The new approach uses struct-based replication (CachedProfileData) for atomic replication

