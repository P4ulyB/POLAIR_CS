#include "Components/PACS_HoverProbeComponent.h"
#include "Core/PACS_PlayerController.h"
#include "Core/PACS_CollisionChannels.h"
#include "Components/PACS_InputHandlerComponent.h"
#include "Components/PACS_SelectionPlaneComponent.h"
#include "Interfaces/PACS_Poolable.h"
#include "Interfaces/PACS_SelectableCharacterInterface.h"
#include "Actors/NPC/PACS_NPC_Base.h"
#include "Data/PACS_SelectionProfile.h"
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

void UPACS_HoverProbeComponent::ApplyConfiguration(
	const TArray<TObjectPtr<UInputMappingContext>>& InActiveContexts,
	const TArray<TEnumAsByte<EObjectTypeQuery>>& InObjectTypes,
	float InRateHz,
	bool bInConfirmVisibility)
{
	// Apply settings from PlayerController
	ActiveInputContexts = InActiveContexts;
	HoverObjectTypes = InObjectTypes;
	RateHz = InRateHz;
	bConfirmVisibility = bInConfirmVisibility;
	bConfigurationApplied = true; // Mark that configuration has been applied

	// Update tick interval to match new rate
	if (RateHz > 0.0f)
	{
		SetComponentTickInterval(1.0f / RateHz);
	}

	// Ensure tick is enabled
	SetComponentTickEnabled(true);

	UE_LOG(LogTemp, Log, TEXT("HoverProbe configuration applied: %d contexts, %d object types, %.1fHz, LoS=%d, TickEnabled=%d"),
		ActiveInputContexts.Num(), HoverObjectTypes.Num(), RateHz, bInConfirmVisibility, IsComponentTickEnabled());
}

void UPACS_HoverProbeComponent::BeginPlay()
{
	Super::BeginPlay();

	// Resolve player controller and setup tick interval
	OwnerPC = Cast<APACS_PlayerController>(GetOwner());

	// Only set defaults if configuration wasn't already applied
	if (!bConfigurationApplied)
	{
		float Interval = RateHz > 0 ? 1.0f / RateHz : 0.0f;
		SetComponentTickInterval(Interval);

		// Default object type if not set: SelectionObject
		if (HoverObjectTypes.Num() == 0)
		{
			// Use the SelectionObject type for hover detection (ECC_GameTraceChannel2)
			HoverObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_GameTraceChannel2));
		}
	}

	// Force enable ticking as failsafe
	SetComponentTickEnabled(true);
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

		// Fallback to SelectionTrace channel if object query fails
		if (!bHit)
		{
			bHit = OwnerPC->GetHitResultUnderCursor(ECC_GameTraceChannel1, false, HitResult);
		}
	}
	else
	{
		// No object types configured - try channels directly
		bHit = OwnerPC->GetHitResultUnderCursor(ECC_GameTraceChannel1, false, HitResult);
		if (!bHit)
		{
			bHit = OwnerPC->GetHitResultUnderCursor(ECC_GameTraceChannel2, false, HitResult);
		}
		if (!bHit)
		{
			bHit = OwnerPC->GetHitResultUnderCursor(ECC_Visibility, false, HitResult);
		}
	}

	if (bHit)
	{
		AActor* HitActor = HitResult.GetActor();

		// Try to get SelectionPlaneComponent from the hit actor
		UPACS_SelectionPlaneComponent* HitPlaneComponent = nullptr;
		if (HitActor)
		{
			// Check if this is a poolable actor (all NPCs implement this interface)
			if (HitActor->Implements<UPACS_Poolable>())
			{
				// Find the SelectionPlaneComponent on the actor
				HitPlaneComponent = HitActor->FindComponentByClass<UPACS_SelectionPlaneComponent>();

				if (HitPlaneComponent)
				{
					// Check if NPC is in Available state (SelectionState == 3)
					// Only Available NPCs can be hovered
					bool bCanHover = false;

					// Get the current selection state from the component
					uint8 CurrentSelectionState = HitPlaneComponent->GetSelectionState();

					// Only allow hover if NPC is Available (state 3)
					if (CurrentSelectionState == 3)
					{
						bCanHover = true;
					}

					if (!bCanHover)
					{
						// Don't set this as hit plane if we can't hover
						HitPlaneComponent = nullptr;
					}
				}
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