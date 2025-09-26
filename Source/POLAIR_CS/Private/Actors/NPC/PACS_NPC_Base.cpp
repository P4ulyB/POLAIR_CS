#include "Actors/NPC/PACS_NPC_Base.h"
#include "Components/BoxComponent.h"
#include "Components/DecalComponent.h"
#include "GameFramework/PlayerState.h"
#include "Materials/Material.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

APACS_NPC_Base::APACS_NPC_Base()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create root box collision
	BoxCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxCollision"));
	RootComponent = BoxCollision;
	SetupDefaultCollision();

	// Create selection decal
	SelectionDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("SelectionDecal"));
	SelectionDecal->SetupAttachment(RootComponent);
	SetupDefaultDecal();

	// Replication settings for multiplayer
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(10.0f);
	SetMinNetUpdateFrequency(2.0f);
}

void APACS_NPC_Base::BeginPlay()
{
	Super::BeginPlay();

	// Hide selection decal by default
	if (SelectionDecal)
	{
		SelectionDecal->SetVisibility(false);
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
}

void APACS_NPC_Base::OnReturnedToPool_Implementation()
{
	ResetForPool();
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

	// Log for debugging
	UE_LOG(LogTemp, Log, TEXT("PACS_NPC_Base: %s %s by %s"),
		*GetName(),
		bIsSelected ? TEXT("selected") : TEXT("deselected"),
		Selector ? *Selector->GetPlayerName() : TEXT("None"));
}

void APACS_NPC_Base::UpdateSelectionVisuals()
{
	if (SelectionDecal)
	{
		SelectionDecal->SetVisibility(bIsSelected);
	}
}

void APACS_NPC_Base::ResetForPool()
{
	// Clear selection
	bIsSelected = false;
	CurrentSelector = nullptr;

	// Hide decal
	if (SelectionDecal)
	{
		SelectionDecal->SetVisibility(false);
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

	// Ensure decal is hidden initially
	if (SelectionDecal)
	{
		SelectionDecal->SetVisibility(false);
	}
}

void APACS_NPC_Base::SetupDefaultDecal()
{
	if (!SelectionDecal)
	{
		return;
	}

	// Set default decal size (can be overridden in Blueprint)
	SelectionDecal->DecalSize = FVector(128.0f, 128.0f, 256.0f);

	// Rotate to project downward
	SelectionDecal->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));

	// Position at bottom of collision box
	SelectionDecal->SetRelativeLocation(FVector(0.0f, 0.0f, -100.0f));

	// Try to load default selection material
	static ConstructorHelpers::FObjectFinder<UMaterial> DecalMaterialAsset(
		TEXT("/Engine/EngineMaterials/DefaultDecalMaterial"));
	if (DecalMaterialAsset.Succeeded())
	{
		SelectionDecal->SetDecalMaterial(DecalMaterialAsset.Object);
	}

	// Start hidden
	SelectionDecal->SetVisibility(false);
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