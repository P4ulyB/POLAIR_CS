#include "Actors/NPC/PACS_NPC_Base.h"
#include "Components/BoxComponent.h"
#include "Components/PACS_SelectionPlaneComponent.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

APACS_NPC_Base::APACS_NPC_Base()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create root box collision
	BoxCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxCollision"));
	RootComponent = BoxCollision;
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

void APACS_NPC_Base::BeginPlay()
{
	Super::BeginPlay();

	// Initialize selection plane component
	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->InitializeSelectionPlane();
		SelectionPlaneComponent->SetSelectionPlaneVisible(false);
	}
}

void APACS_NPC_Base::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clear selection if selected
	if (bIsSelected)
	{
		SetSelected(false, nullptr);
	}

	Super::EndPlay(EndPlayReason);
}

void APACS_NPC_Base::OnAcquiredFromPool_Implementation()
{
	PrepareForUse();

	// Notify selection component
	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->OnAcquiredFromPool();
	}
}

void APACS_NPC_Base::OnReturnedToPool_Implementation()
{
	ResetForPool();

	// Notify selection component
	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->OnReturnedToPool();
	}
}

void APACS_NPC_Base::SetSelected(bool bNewSelected, APlayerState* Selector)
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

	// Log for debugging
	UE_LOG(LogTemp, Log, TEXT("PACS_NPC_Base: %s %s by %s"),
		*GetName(),
		bIsSelected ? TEXT("selected") : TEXT("deselected"),
		Selector ? *Selector->GetPlayerName() : TEXT("None"));
}

void APACS_NPC_Base::UpdateSelectionVisuals()
{
	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->SetSelectionPlaneVisible(bIsSelected);
	}
}

void APACS_NPC_Base::ResetForPool()
{
	// Clear selection
	bIsSelected = false;
	CurrentSelector = nullptr;

	// Hide selection plane
	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->SetSelectionPlaneVisible(false);
	}

	// Reset transform
	SetActorTransform(FTransform::Identity);

	// Clear velocity if physics enabled
	if (BoxCollision && BoxCollision->IsSimulatingPhysics())
	{
		BoxCollision->SetPhysicsLinearVelocity(FVector::ZeroVector);
		BoxCollision->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
}

void APACS_NPC_Base::PrepareForUse()
{
	// Re-enable collision
	if (BoxCollision)
	{
		BoxCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	// Ensure selection plane is hidden initially
	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->SetSelectionPlaneVisible(false);
	}
}


void APACS_NPC_Base::SetupDefaultCollision()
{
	if (!BoxCollision)
	{
		return;
	}

	// Set default box size (can be overridden in Blueprint)
	BoxCollision->SetBoxExtent(FVector(50.0f, 50.0f, 88.0f));

	// Set collision profile
	BoxCollision->SetCollisionProfileName(TEXT("Pawn"));
	BoxCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BoxCollision->SetCollisionResponseToAllChannels(ECR_Block);
	BoxCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	BoxCollision->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Ignore);

	// Generate overlap events for selection
	BoxCollision->SetGenerateOverlapEvents(true);
}