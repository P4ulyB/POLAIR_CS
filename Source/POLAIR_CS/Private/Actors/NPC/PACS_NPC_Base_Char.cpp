#include "Actors/NPC/PACS_NPC_Base_Char.h"
#include "Components/BoxComponent.h"
#include "Components/PACS_SelectionPlaneComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Net/UnrealNetwork.h"

APACS_NPC_Base_Char::APACS_NPC_Base_Char()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create box collision (in addition to capsule)
	BoxCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxCollision"));
	BoxCollision->SetupAttachment(RootComponent);
	SetupDefaultCollision();

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
	if (BoxCollision)
	{
		BoxCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

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

void APACS_NPC_Base_Char::SetupDefaultCollision()
{
	if (!BoxCollision)
	{
		return;
	}

	// Set box size to match capsule roughly
	float CapsuleRadius = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleRadius() : 42.0f;
	float CapsuleHalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.0f;

	BoxCollision->SetBoxExtent(FVector(CapsuleRadius, CapsuleRadius, CapsuleHalfHeight));

	// Set collision profile for selection
	BoxCollision->SetCollisionProfileName(TEXT("OverlapOnlyPawn"));
	BoxCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoxCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoxCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// Generate overlap events for selection
	BoxCollision->SetGenerateOverlapEvents(true);
}