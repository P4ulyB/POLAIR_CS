#include "Actors/PACS_SelectionCueProxy.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Engine.h"

APACS_SelectionCueProxy::APACS_SelectionCueProxy()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false; // Local-only visual feedback

	// Create hover mesh component
	HoverMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HoverMesh"));
	RootComponent = HoverMesh;
	HoverMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HoverMesh->SetVisibility(false);
	HoverMesh->SetCastShadow(false);

	// Create selection mesh component
	SelectionMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SelectionMesh"));
	SelectionMesh->SetupAttachment(RootComponent);
	SelectionMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SelectionMesh->SetVisibility(false);
	SelectionMesh->SetCastShadow(false);
}

void APACS_SelectionCueProxy::BeginPlay()
{
	Super::BeginPlay();

	// Initialize dynamic material instances for runtime parameter control
	InitializeMaterialInstances();
}

void APACS_SelectionCueProxy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clean up any references
	HoverMaterialInstance = nullptr;
	SelectionMaterialInstance = nullptr;

	Super::EndPlay(EndPlayReason);
}

void APACS_SelectionCueProxy::SetLocalHovered(bool bHovered)
{
	if (bIsCurrentlyHovered != bHovered)
	{
		bIsCurrentlyHovered = bHovered;
		UpdateHoverVisuals();
	}
}

void APACS_SelectionCueProxy::SetSelected(bool bSelected)
{
	if (bIsCurrentlySelected != bSelected)
	{
		bIsCurrentlySelected = bSelected;
		UpdateSelectionVisuals();
	}
}

void APACS_SelectionCueProxy::UpdateHoverVisuals()
{
	if (!HoverMesh)
	{
		return;
	}

	if (bIsCurrentlyHovered)
	{
		HoverMesh->SetVisibility(true);

		// Update material parameters if available
		if (HoverMaterialInstance)
		{
			HoverMaterialInstance->SetVectorParameterValue(FName(TEXT("Color")), HoverColor);
			HoverMaterialInstance->SetScalarParameterValue(FName(TEXT("Opacity")), HoverOpacity);
		}
	}
	else
	{
		HoverMesh->SetVisibility(false);
	}
}

void APACS_SelectionCueProxy::UpdateSelectionVisuals()
{
	if (!SelectionMesh)
	{
		return;
	}

	if (bIsCurrentlySelected)
	{
		SelectionMesh->SetVisibility(true);

		// Update material parameters if available
		if (SelectionMaterialInstance)
		{
			SelectionMaterialInstance->SetVectorParameterValue(FName(TEXT("Color")), SelectionColor);
			SelectionMaterialInstance->SetScalarParameterValue(FName(TEXT("Opacity")), SelectionOpacity);
		}
	}
	else
	{
		SelectionMesh->SetVisibility(false);
	}
}

void APACS_SelectionCueProxy::InitializeMaterialInstances()
{
	// Create dynamic material instance for hover mesh
	if (HoverMesh && HoverMaterial)
	{
		HoverMaterialInstance = UMaterialInstanceDynamic::Create(HoverMaterial, this);
		if (HoverMaterialInstance)
		{
			HoverMesh->SetMaterial(0, HoverMaterialInstance);
		}
	}

	// Create dynamic material instance for selection mesh
	if (SelectionMesh && SelectionMaterial)
	{
		SelectionMaterialInstance = UMaterialInstanceDynamic::Create(SelectionMaterial, this);
		if (SelectionMaterialInstance)
		{
			SelectionMesh->SetMaterial(0, SelectionMaterialInstance);
		}
	}
}