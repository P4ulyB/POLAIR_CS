#include "Actors/NPC/PACS_NPC_Base_Veh.h"
#include "Components/BoxComponent.h"
#include "Components/PACS_SelectionPlaneComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

APACS_NPC_Base_Veh::APACS_NPC_Base_Veh()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create box collision for selection
	BoxCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxCollision"));
	BoxCollision->SetupAttachment(RootComponent);
	SetupDefaultCollision();

	// Create selection plane component (manages state and client-side visuals)
	// The component itself handles creating visual elements ONLY on non-VR clients
	SelectionPlaneComponent = CreateDefaultSubobject<UPACS_SelectionPlaneComponent>(TEXT("SelectionPlaneComponent"));
	SelectionPlaneComponent->SetIsReplicated(true);

	// Replication settings for multiplayer
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(10.0f);
	SetMinNetUpdateFrequency(2.0f);
}

void APACS_NPC_Base_Veh::BeginPlay()
{
	Super::BeginPlay();

	// Store default engine state
	if (UChaosWheeledVehicleMovementComponent* VehicleMovement = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
	{
		bEngineStartedByDefault = true; // Vehicles typically start with engine on
	}

	// Initialize selection plane component
	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->InitializeSelectionPlane();
		SelectionPlaneComponent->SetSelectionPlaneVisible(false);
	}
}

void APACS_NPC_Base_Veh::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clear selection if selected
	if (bIsSelected)
	{
		SetSelected(false, nullptr);
	}

	// Stop vehicle
	StopVehicle();

	Super::EndPlay(EndPlayReason);
}

void APACS_NPC_Base_Veh::OnAcquiredFromPool_Implementation()
{
	PrepareForUse();

	// Notify selection component
	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->OnAcquiredFromPool();
	}
}

void APACS_NPC_Base_Veh::OnReturnedToPool_Implementation()
{
	ResetForPool();

	// Notify selection component
	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->OnReturnedToPool();
	}
}

void APACS_NPC_Base_Veh::SetSelected(bool bNewSelected, APlayerState* Selector)
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

	UE_LOG(LogTemp, Log, TEXT("PACS_NPC_Base_Veh: %s %s by %s"),
		*GetName(),
		bIsSelected ? TEXT("selected") : TEXT("deselected"),
		Selector ? *Selector->GetPlayerName() : TEXT("None"));
}

void APACS_NPC_Base_Veh::DriveToLocation(const FVector& TargetLocation)
{
	if (!HasAuthority())
	{
		return;
	}

	// This would typically integrate with an AI driving system
	// For now, log the intent
	UE_LOG(LogTemp, Log, TEXT("PACS_NPC_Base_Veh: %s driving to %s"),
		*GetName(), *TargetLocation.ToString());

	// Basic implementation could use pathfinding and vehicle AI controller
	// This is a placeholder for project-specific driving logic
}

void APACS_NPC_Base_Veh::StopVehicle()
{
	if (!HasAuthority())
	{
		return;
	}

	if (UChaosWheeledVehicleMovementComponent* VehicleMovement = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
	{
		// Apply full brakes
		VehicleMovement->SetBrakeInput(1.0f);

		// Release throttle
		VehicleMovement->SetThrottleInput(0.0f);

		// Apply handbrake
		VehicleMovement->SetHandbrakeInput(true);

		// Zero steering
		VehicleMovement->SetSteeringInput(0.0f);
	}
}

void APACS_NPC_Base_Veh::SetHandbrake(bool bEngaged)
{
	if (!HasAuthority())
	{
		return;
	}

	if (UChaosWheeledVehicleMovementComponent* VehicleMovement = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
	{
		VehicleMovement->SetHandbrakeInput(bEngaged);
	}
}

void APACS_NPC_Base_Veh::UpdateSelectionVisuals()
{
	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->SetSelectionPlaneVisible(bIsSelected);
	}
}

void APACS_NPC_Base_Veh::ResetForPool()
{
	// Clear selection
	bIsSelected = false;
	CurrentSelector = nullptr;

	// Hide selection plane
	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->SetSelectionPlaneVisible(false);
	}

	// Stop vehicle
	StopVehicle();

	// Reset vehicle-specific state
	ResetVehicleState();
	ResetVehiclePhysics();

	// Reset transform
	SetActorTransform(FTransform::Identity);
}

void APACS_NPC_Base_Veh::PrepareForUse()
{
	// Re-enable collision
	if (BoxCollision)
	{
		BoxCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	// Reset vehicle movement
	if (UChaosWheeledVehicleMovementComponent* VehicleMovement = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
	{
		// Start engine if it was started by default
		if (bEngineStartedByDefault)
		{
			// Engine control would go here if exposed
		}

		// Release all inputs
		VehicleMovement->SetThrottleInput(0.0f);
		VehicleMovement->SetBrakeInput(0.0f);
		VehicleMovement->SetSteeringInput(0.0f);
		VehicleMovement->SetHandbrakeInput(false);
	}

	// Ensure selection plane is hidden initially
	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->SetSelectionPlaneVisible(false);
	}
}

void APACS_NPC_Base_Veh::ResetVehicleState()
{
	if (UChaosWheeledVehicleMovementComponent* VehicleMovement = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
	{
		// Reset all inputs
		VehicleMovement->SetThrottleInput(0.0f);
		VehicleMovement->SetBrakeInput(0.0f);
		VehicleMovement->SetSteeringInput(0.0f);
		VehicleMovement->SetHandbrakeInput(true); // Apply handbrake when pooled

		// Reset gear if applicable
		VehicleMovement->SetTargetGear(0, true); // Neutral
	}
}

void APACS_NPC_Base_Veh::ResetVehiclePhysics()
{
	// Get the vehicle mesh (usually the root component for vehicles)
	if (UPrimitiveComponent* VehicleMesh = Cast<UPrimitiveComponent>(GetRootComponent()))
	{
		if (VehicleMesh->IsSimulatingPhysics())
		{
			// Clear velocities
			VehicleMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
			VehicleMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

			// Reset physics state
			VehicleMesh->SetSimulatePhysics(false);
			VehicleMesh->SetSimulatePhysics(true);
		}
	}

	// Additional Chaos vehicle reset if needed
	if (UChaosWheeledVehicleMovementComponent* VehicleMovement = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent()))
	{
		// Reset vehicle to rest
		VehicleMovement->StopMovementImmediately();
	}
}

void APACS_NPC_Base_Veh::SetupDefaultCollision()
{
	if (!BoxCollision)
	{
		return;
	}

	// Set box size for vehicle selection (larger than characters)
	BoxCollision->SetBoxExtent(FVector(200.0f, 100.0f, 100.0f));

	// Set collision profile for selection
	BoxCollision->SetCollisionProfileName(TEXT("OverlapOnlyPawn"));
	BoxCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoxCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoxCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// Generate overlap events for selection
	BoxCollision->SetGenerateOverlapEvents(true);
}