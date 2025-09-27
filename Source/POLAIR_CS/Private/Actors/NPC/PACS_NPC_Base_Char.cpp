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

	// Replicate the skeletal mesh to all clients
	// Epic doesn't replicate SetSkeletalMesh() by default, so we need custom replication
	DOREPLIFETIME(APACS_NPC_Base_Char, ReplicatedSkeletalMesh);
}

void APACS_NPC_Base_Char::OnRep_SkeletalMeshAsset()
{
	UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::OnRep_SkeletalMeshAsset: Called on client for %s"), *GetName());

	// Apply the replicated skeletal mesh to the mesh component
	if (ReplicatedSkeletalMesh && GetMesh())
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::OnRep_SkeletalMeshAsset: Applying SK mesh %s to %s on client"),
			*ReplicatedSkeletalMesh->GetName(), *GetName());

		GetMesh()->SetSkeletalMesh(ReplicatedSkeletalMesh);

		// Also apply the transform if we have a cached selection profile
		// Note: We may need to replicate the transform separately if needed
		UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::OnRep_SkeletalMeshAsset: Successfully applied SK mesh on client"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::OnRep_SkeletalMeshAsset: Failed - ReplicatedMesh=%s, GetMesh=%s"),
			ReplicatedSkeletalMesh ? TEXT("Valid") : TEXT("Null"),
			GetMesh() ? TEXT("Valid") : TEXT("Null"));
	}
}

void APACS_NPC_Base_Char::BeginPlay()
{
	Super::BeginPlay();

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

	// Reset transform
	SetActorTransform(FTransform::Identity);

	// Clear velocity
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->Velocity = FVector::ZeroVector;
		GetCharacterMovement()->StopMovementImmediately();
	}
}

void APACS_NPC_Base_Char::PrepareForUse()
{
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

	// Selection profile is now applied by spawn orchestrator when acquired from pool
	// This follows Principle #4: Pool Pre-configured NPCs
}

void APACS_NPC_Base_Char::SetSelectionProfile(UPACS_SelectionProfileAsset* InProfile)
{
	// Only server should set the profile to ensure consistency
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char: SetSelectionProfile called without authority for %s"), *GetName());
		return;
	}

	if (!InProfile)
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char: SetSelectionProfile called with null profile for %s"), *GetName());
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char: SetSelectionProfile starting for %s with profile %s"),
		*GetName(), *InProfile->GetName());

	// CRITICAL FIX: Do NOT skip SK mesh on dedicated server!
	// SK mesh MUST be set on server for replication to clients
	// Only skip expensive visual effects on DS, not replicated mesh data

	// Apply skeletal mesh - MUST happen on server for replication
	ApplySkeletalMeshFromProfile(InProfile);

	// Apply other visual components (can skip on DS since they're client-side)
	if (GetWorld() && GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		// Apply to selection plane component (client-side visual)
		if (SelectionPlaneComponent)
		{
			SelectionPlaneComponent->ApplyProfileAsset(InProfile);
			UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char: Applied selection plane for %s"), *GetName());
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char: SetSelectionProfile completed for %s"), *GetName());
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

void APACS_NPC_Base_Char::ApplySkeletalMeshFromProfile(UPACS_SelectionProfileAsset* ProfileAsset)
{
	// CRITICAL: Do NOT skip on dedicated server!
	// The SkeletalMesh property is REPLICATED and MUST be set on the server
	// for clients to receive it. This is NOT just a visual effect - it's replicated data!

	if (!ProfileAsset)
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::ApplySkeletalMeshFromProfile: Null ProfileAsset for %s"), *GetName());
		return;
	}

	if (ProfileAsset->SkeletalMeshAsset.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::ApplySkeletalMeshFromProfile: Null SkeletalMeshAsset in profile for %s"), *GetName());
		return;
	}

	// Apply skeletal mesh to character mesh component
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		UE_LOG(LogTemp, Error, TEXT("PACS_NPC_Base_Char::ApplySkeletalMeshFromProfile: No mesh component found for %s"), *GetName());
		return;
	}

	// Log current state for debugging
	UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::ApplySkeletalMeshFromProfile: Starting for %s on %s"),
		*GetName(),
		HasAuthority() ? TEXT("Server") : TEXT("Client"));

	// Principle 1 & 4: First check if already loaded (from pre-loading or pooling)
	USkeletalMesh* SkMesh = ProfileAsset->SkeletalMeshAsset.Get();

	if (!SkMesh)
	{
		// Principle 4: Pool Pre-configured NPCs - Allow synchronous load only during pooling/initialization
		// This ensures pooled NPCs are fully configured before being made available
		bool bIsPoolingPhase = !HasActorBegunPlay();

		if (bIsPoolingPhase)
		{
			// Safe to load synchronously during pool initialization
			SkMesh = ProfileAsset->SkeletalMeshAsset.LoadSynchronous();
			UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::ApplySkeletalMeshFromProfile: Synchronously loaded SK mesh during pooling for %s"), *GetName());
		}
		else
		{
			// Principle 3: Runtime loads should use async - this shouldn't happen if pre-loading works correctly
			UE_LOG(LogTemp, Error, TEXT("PACS_NPC_Base_Char::ApplySkeletalMeshFromProfile: SK mesh not pre-loaded for runtime use on %s - CRITICAL ERROR"), *GetName());
			return;
		}
	}

	// Principle 5: Proper SK Asset Handling - Apply mesh and transform
	if (SkMesh)
	{
		// Store current mesh for comparison
		USkeletalMesh* OldMesh = MeshComp->GetSkeletalMeshAsset();

		// CRITICAL: Set the replicated property on server
		// This will trigger OnRep_SkeletalMeshAsset on clients
		if (HasAuthority())
		{
			ReplicatedSkeletalMesh = SkMesh;
			UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::ApplySkeletalMeshFromProfile: Set ReplicatedSkeletalMesh on server for %s"),
				*GetName());
		}

		// Apply new mesh locally (server needs this, clients will get it via OnRep)
		MeshComp->SetSkeletalMesh(SkMesh);
		MeshComp->SetRelativeTransform(ProfileAsset->SkeletalMeshTransform);

		// Log success with detailed info
		UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::ApplySkeletalMeshFromProfile: SUCCESS - Applied SK mesh %s to %s (was %s) [Authority=%s]"),
			*SkMesh->GetName(),
			*GetName(),
			OldMesh ? *OldMesh->GetName() : TEXT("null"),
			HasAuthority() ? TEXT("Server") : TEXT("Client"));

		// Verify the mesh was actually applied
		if (MeshComp->GetSkeletalMeshAsset() != SkMesh)
		{
			UE_LOG(LogTemp, Error, TEXT("PACS_NPC_Base_Char::ApplySkeletalMeshFromProfile: VERIFICATION FAILED - Mesh not applied correctly to %s"), *GetName());
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("PACS_NPC_Base_Char::ApplySkeletalMeshFromProfile: Failed to load SK mesh for %s"), *GetName());
	}
}

