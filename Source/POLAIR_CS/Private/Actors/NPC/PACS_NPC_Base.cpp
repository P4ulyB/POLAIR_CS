#include "Actors/NPC/PACS_NPC_Base.h"
#include "Components/PACS_SelectionPlaneComponent.h"
#include "Data/PACS_SelectionProfile.h"
#include "NiagaraComponent.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

APACS_NPC_Base::APACS_NPC_Base()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create Niagara component
	NiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraComponent"));
	RootComponent = NiagaraComponent;

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

		// Note: Selection profile is now applied by spawn orchestrator
		// This ensures profiles are preloaded and properly managed
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

}

void APACS_NPC_Base::PrepareForUse()
{
	// Ensure selection plane is hidden initially
	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->SetSelectionPlaneVisible(false);
	}
}

void APACS_NPC_Base::SetSelectionProfile(UPACS_SelectionProfileAsset* InProfile)
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

	// Apply the profile directly to the component
	if (InProfile && SelectionPlaneComponent)
	{
		SelectionPlaneComponent->ApplyProfileAsset(InProfile);
		UE_LOG(LogTemp, Verbose, TEXT("PACS_NPC_Base: Applied selection profile to %s"), *GetName());
	}
}

void APACS_NPC_Base::ApplySelectionProfile()
{
	// This method is now deprecated - selection profiles are applied directly
	// via SetSelectionProfile which is called by the spawn orchestrator
	// with preloaded profiles (following Principle #1: Pre-load Selection Profiles)
}