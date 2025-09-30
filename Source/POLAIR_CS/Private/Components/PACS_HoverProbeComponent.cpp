#include "Components/PACS_HoverProbeComponent.h"
#include "Core/PACS_PlayerController.h"
#include "Core/PACS_CollisionChannels.h"
#include "Components/PACS_InputHandlerComponent.h"
#include "Components/PACS_SelectionPlaneComponent.h"
#include "Actors/NPC/PACS_NPC_Base.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

UPACS_HoverProbeComponent::UPACS_HoverProbeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	// Default probe settings
	RateHz = 30.0f; // 30Hz probe rate
	bConfirmVisibility = true;
	bIsCurrentlyActive = true;
}

void UPACS_HoverProbeComponent::BeginPlay()
{
	Super::BeginPlay();

	// Resolve player controller and setup tick interval
	OwnerPC = Cast<APACS_PlayerController>(GetOwner());
	float Interval = RateHz > 0 ? 1.0f / RateHz : 0.0f;
	SetComponentTickInterval(Interval);

	// Default object type if not set in editor: Selection channel
	if (HoverObjectTypes.Num() == 0)
	{
		// Use the Selection collision channel for hover detection
		HoverObjectTypes.Add(UEngineTypes::ConvertToObjectType(static_cast<ECollisionChannel>(EPACS_CollisionChannel::Selection)));
	}
}

void UPACS_HoverProbeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearHover();
	Super::EndPlay(EndPlayReason);
}

void UPACS_HoverProbeComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	ClearHover();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UPACS_HoverProbeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ProbeOnce();
}

void UPACS_HoverProbeComponent::ProbeOnce()
{
	if (!OwnerPC.IsValid())
	{
		return;
	}

	// Perform line trace from cursor using object type query for selection planes
	FHitResult HitResult;
	bool bHit = false;

	// Use object type query if configured
	if (HoverObjectTypes.Num() > 0)
	{
		bHit = OwnerPC->GetHitResultUnderCursorForObjects(HoverObjectTypes, false, HitResult);
	}
	else
	{
		// Fallback to visibility channel
		bHit = OwnerPC->GetHitResultUnderCursor(ECC_Visibility, false, HitResult);
	}

	if (bHit)
	{
		AActor* HitActor = HitResult.GetActor();

		// Try to get SelectionPlaneComponent from the hit actor
		UPACS_SelectionPlaneComponent* HitPlaneComponent = nullptr;
		if (HitActor)
		{
			// Check if this is an NPC with a selection plane
			if (APACS_NPC_Base* NPC = Cast<APACS_NPC_Base>(HitActor))
			{
				HitPlaneComponent = NPC->SelectionPlaneComponent;
			}
		}

		UPACS_SelectionPlaneComponent* CurrentPlaneComponent = CurrentHoverPlaneComponent.Get();

		// Update hover state if component changed
		if (HitPlaneComponent != CurrentPlaneComponent)
		{
			ClearHover();

			if (HitPlaneComponent)
			{
				CurrentHoverActor = HitActor;
				CurrentHoverPlaneComponent = HitPlaneComponent;

				// Activate hover visuals on the selection plane
				HitPlaneComponent->SetHoverState(true);
			}
		}
	}
	else
	{
		// No hit - clear hover
		ClearHover();
	}
}

void UPACS_HoverProbeComponent::ClearHover()
{
	// Clear hover state on the selection plane component
	if (CurrentHoverPlaneComponent.IsValid())
	{
		CurrentHoverPlaneComponent->SetHoverState(false);
		CurrentHoverPlaneComponent = nullptr;
	}

	if (CurrentHoverActor.IsValid())
	{
		CurrentHoverActor = nullptr;
	}
}

AActor* UPACS_HoverProbeComponent::ResolveSelectableActor(const FHitResult& HitResult) const
{
	return HitResult.GetActor();
}

bool UPACS_HoverProbeComponent::IsInputContextActive() const
{
	// Simplified - always active
	return true;
}

UInputMappingContext* UPACS_HoverProbeComponent::GetCurrentActiveContext() const
{
	return nullptr;
}

void UPACS_HoverProbeComponent::OnNPCDestroyed(AActor* DestroyedActor)
{
	if (CurrentHoverActor == DestroyedActor)
	{
		ClearHover();
	}
}

void UPACS_HoverProbeComponent::OnInputContextChanged()
{
	// Context monitoring removed for now
}

void UPACS_HoverProbeComponent::UnbindNPCDelegates()
{
	// Cleanup removed for simplicity
}

void UPACS_HoverProbeComponent::UnbindInputDelegates()
{
	// Cleanup removed for simplicity
}