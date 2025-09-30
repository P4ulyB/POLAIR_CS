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

		// Note: Selection profile is now applied by spawn orchestrator
		// This ensures profiles are preloaded and properly managed
		// Visibility is handled automatically by component's UpdateVisuals()
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
		SelectionPlaneComponent->OnAcquiredFromPool_Implementation();
	}
}

void APACS_NPC_Base::OnReturnedToPool_Implementation()
{
	ResetForPool();

	// Notify selection component
	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->OnReturnedToPool_Implementation();
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
	// Visibility handled automatically by component via UpdateVisuals()
	// Component determines visibility based on state
}

void APACS_NPC_Base::ResetForPool()
{
	// Clear selection
	bIsSelected = false;
	CurrentSelector = nullptr;

	// Selection plane visibility handled by component's OnReturnedToPool()

	// Reset transform
	SetActorTransform(FTransform::Identity);

}

void APACS_NPC_Base::PrepareForUse()
{
	// Selection plane visibility handled by component
	// Component starts hidden and only shows based on selection state
}

void APACS_NPC_Base::SetSelectionProfile(UPACS_SelectionProfileAsset* InProfile)
{
	// Only server should set the profile to ensure consistency
	if (!HasAuthority() || !InProfile)
	{
		return;
	}

	// ========================================
	// NPC VISUALS (moved from component)
	// ========================================

	// TODO: ParticleEffect in profile is UParticleSystem but NiagaraComponent expects UNiagaraSystem
	// This needs to be addressed by changing the profile to use UNiagaraSystem or converting the component
	// For now, particle effects are disabled to prevent type mismatch errors

	// For static mesh NPCs (vehicles, lightweight)
	// Subclasses override ApplyNPCMeshFromProfile()
	ApplyNPCMeshFromProfile(InProfile);

	// ========================================
	// SELECTION VISUALS (component handles)
	// ========================================

	// Skip on dedicated server
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// Apply selection profile to component
	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->ApplyProfileAsset(InProfile);
		UE_LOG(LogTemp, Verbose, TEXT("PACS_NPC_Base: Applied selection profile to %s"), *GetName());
	}
}

void APACS_NPC_Base::ApplyNPCMeshFromProfile(UPACS_SelectionProfileAsset* Profile)
{
	// Base class does nothing (Character uses FNPCProfileData replication)
	// PACS_NPC_Base_Veh overrides to apply vehicle mesh
	// PACS_NPC_Base_LW overrides to apply lightweight static mesh
}

void APACS_NPC_Base::ApplySelectionProfile()
{
	// This method is now deprecated - selection profiles are applied directly
	// via SetSelectionProfile which is called by the spawn orchestrator
	// with preloaded profiles (following Principle #1: Pre-load Selection Profiles)
}