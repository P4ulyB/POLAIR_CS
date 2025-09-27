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
		return;
	}

	// Skip on dedicated server (Principle #2: Skip Visual Assets on Dedicated Server)
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// Apply the profile directly to components
	if (InProfile)
	{
		// Apply to selection plane component
		if (SelectionPlaneComponent)
		{
			SelectionPlaneComponent->ApplyProfileAsset(InProfile);
		}

		// Apply skeletal mesh if character has mesh component
		ApplySkeletalMeshFromProfile(InProfile);

		UE_LOG(LogTemp, Verbose, TEXT("PACS_NPC_Base_Char: Applied selection profile to %s"), *GetName());
	}
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
	// Principle 2: Skip Visual Assets on Dedicated Server
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!ProfileAsset || ProfileAsset->SkeletalMeshAsset.IsNull())
	{
		return;
	}

	// Apply skeletal mesh to character mesh component
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
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
				UE_LOG(LogTemp, Verbose, TEXT("PACS_NPC_Base_Char: Synchronously loaded SK mesh during pooling for %s"), *GetName());
			}
			else
			{
				// Principle 3: Runtime loads should use async - this shouldn't happen if pre-loading works correctly
				UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char: SK mesh not pre-loaded for runtime use on %s - should use async loading"), *GetName());
				return;
			}
		}

		// Principle 5: Proper SK Asset Handling - Apply mesh and transform
		if (SkMesh)
		{
			MeshComp->SetSkeletalMesh(SkMesh);
			MeshComp->SetRelativeTransform(ProfileAsset->SkeletalMeshTransform);

			UE_LOG(LogTemp, Verbose, TEXT("PACS_NPC_Base_Char: Applied skeletal mesh to %s"), *GetName());
		}
	}
}

