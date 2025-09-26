#include "Components/PACS_HoverProbeComponent.h"
#include "Core/PACS_PlayerController.h"
#include "Core/PACS_CollisionChannels.h"
#include "Components/PACS_InputHandlerComponent.h"
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

	// Perform line trace from cursor
	FHitResult HitResult;
	bool bHit = OwnerPC->GetHitResultUnderCursor(ECC_Visibility, false, HitResult);

	if (bHit)
	{
		AActor* HitActor = HitResult.GetActor();
		AActor* CurrentActor = CurrentHoverActor.Get();

		// Update hover state if actor changed
		if (HitActor != CurrentActor)
		{
			ClearHover();

			if (HitActor)
			{
				CurrentHoverActor = HitActor;
				// Could add hover visuals here
			}
		}
	}
	else
	{
		// No hit - clear hover
		ClearHover();
	}

	// Debug visualization can be added here if needed
}

void UPACS_HoverProbeComponent::ClearHover()
{
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