#include "Components/PACS_HoverProbeComponent.h"
#include "Core/PACS_PlayerController.h"
#include "Actors/NPC/PACS_NPCCharacter.h"
#include "Interfaces/PACS_SelectableCharacterInterface.h"
#include "Components/PACS_InputHandlerComponent.h"
#include "Core/PACS_CollisionChannels.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "CollisionQueryParams.h"

UPACS_HoverProbeComponent::UPACS_HoverProbeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UPACS_HoverProbeComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerPC = Cast<APACS_PlayerController>(GetOwner());

	// Run at ~30 Hz irrespective of frame rate
	const float Interval = 1.0f / FMath::Max(1.0f, RateHz);
	SetComponentTickInterval(Interval);

	// Default object type if not set in editor: SelectionObject channel
	if (HoverObjectTypes.Num() == 0)
	{
		// Use the SelectionObject collision channel (object type) for hover detection
		// Note: SelectionObject is configured as an object type (bTraceType=False) in DefaultEngine.ini
		HoverObjectTypes.Add(UEngineTypes::ConvertToObjectType(static_cast<ECollisionChannel>(EPACS_CollisionChannel::SelectionObject)));
	}

	// Monitor input context changes for automatic cleanup
	if (OwnerPC.IsValid())
	{
		if (UPACS_InputHandlerComponent* InputHandler = OwnerPC->GetInputHandler())
		{
			// Note: This would need a delegate on InputHandler for context changes
			// For now, we'll rely on the bSelectionContextActive property
		}
	}
}

void UPACS_HoverProbeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Comprehensive cleanup on component end
	ClearHover();
	UnbindInputDelegates();

	Super::EndPlay(EndPlayReason);
}

void UPACS_HoverProbeComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	// Final cleanup safety net
	ClearHover();
	UnbindInputDelegates();

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UPACS_HoverProbeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Early exit patterns for performance
	const bool bInputContextActive = IsInputContextActive();
	if (!bInputContextActive)
	{
		if (bWasActiveLastFrame)
		{
			ClearHover(); // Only clear when transitioning from active to inactive
		}
		bIsCurrentlyActive = false;
		bWasActiveLastFrame = false;
		return;
	}

	// Update active state for debugging
	bIsCurrentlyActive = true;
	bWasActiveLastFrame = true;

	if (!OwnerPC.IsValid())
	{
		OwnerPC = Cast<APACS_PlayerController>(GetOwner());
	}

	if (!OwnerPC.IsValid())
	{
		ClearHover();
		return;
	}

	ProbeOnce(); // single cursor query @ ~30 Hz
}

void UPACS_HoverProbeComponent::ProbeOnce()
{
	FHitResult Hit;
	// Built-in cursor → world ray + filtered objects (no custom channel math)
	if (!OwnerPC->GetHitResultUnderCursorForObjects(HoverObjectTypes, /*bTraceComplex=*/false, Hit))
	{
		ClearHover();
		return;
	}


	// Optional LOS confirm: camera → impact point on Visibility
	if (bConfirmVisibility)
	{
		FVector CamLoc;
		FRotator CamRot;
		OwnerPC->GetPlayerViewPoint(CamLoc, CamRot);

		FHitResult Block;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(HoverLOS), /*bTraceComplex=*/false);
		Params.AddIgnoredActor(OwnerPC->GetPawn());

		if (GetWorld()->LineTraceSingleByChannel(Block, CamLoc, Hit.ImpactPoint, static_cast<ECollisionChannel>(EPACS_CollisionChannel::Selection), Params))
		{
			if (Block.GetActor() && Block.GetActor() != Hit.GetActor())
			{
				ClearHover();
				return; // occluded
			}
		}
	}

	// Convert hit to selectable interface
	IPACS_SelectableCharacterInterface* NewNPCInterface = nullptr;
	AActor* NewNPCActor = ResolveSelectableFrom(Hit, NewNPCInterface);

	if (NewNPCInterface && NewNPCActor)
	{
		if (NewNPCActor != CurrentNPCActor.Get())
		{
			// Clear previous hover with proper cleanup
			if (CurrentNPCInterface && CurrentNPCActor.IsValid())
			{
				UnbindNPCDelegates();
				CurrentNPCInterface->SetLocalHover(false);
			}

			// Set new hover with delegate binding
			CurrentNPCActor = NewNPCActor;
			CurrentNPCInterface = NewNPCInterface;
			if (CurrentNPCActor.IsValid())
			{
				CurrentNPCInterface->SetLocalHover(true); // purely local, no RPC
				CurrentNPCActor->OnDestroyed.AddDynamic(this, &UPACS_HoverProbeComponent::OnNPCDestroyed);
			}
		}
		return;
	}

	ClearHover();
}

AActor* UPACS_HoverProbeComponent::ResolveSelectableFrom(const FHitResult& Hit, IPACS_SelectableCharacterInterface*& OutInterface) const
{
	if (AActor* HitActor = Hit.GetActor())
	{
		// Direct selectable hit - works for both heavyweight and lightweight NPCs
		if (IPACS_SelectableCharacterInterface* Selectable = Cast<IPACS_SelectableCharacterInterface>(HitActor))
		{
			OutInterface = Selectable;
			return HitActor;
		}

		// Child component hit - check owner
		if (AActor* Owner = HitActor->GetOwner())
		{
			if (IPACS_SelectableCharacterInterface* Selectable = Cast<IPACS_SelectableCharacterInterface>(Owner))
			{
				OutInterface = Selectable;
				return Owner;
			}
		}
	}
	OutInterface = nullptr;
	return nullptr;
}


void UPACS_HoverProbeComponent::ClearHover()
{
	if (CurrentNPCInterface && CurrentNPCActor.IsValid())
	{
		UnbindNPCDelegates();
		CurrentNPCInterface->SetLocalHover(false);
	}
	CurrentNPCActor = nullptr;
	CurrentNPCInterface = nullptr;
}

void UPACS_HoverProbeComponent::OnNPCDestroyed(AActor* DestroyedActor)
{
	// Automatic cleanup when NPC is destroyed mid-hover
	if (CurrentNPCActor.Get() == DestroyedActor)
	{
		CurrentNPCActor = nullptr; // Don't call SetLocalHover on destroyed actor
		CurrentNPCInterface = nullptr;
	}
}

void UPACS_HoverProbeComponent::OnInputContextChanged()
{
	// Called when input context changes - clear hover if context becomes inactive
	if (!IsInputContextActive())
	{
		ClearHover();
		bIsCurrentlyActive = false;
	}
	else
	{
		bIsCurrentlyActive = true;
	}
}

void UPACS_HoverProbeComponent::UnbindNPCDelegates()
{
	if (CurrentNPCActor.IsValid())
	{
		CurrentNPCActor->OnDestroyed.RemoveDynamic(this, &UPACS_HoverProbeComponent::OnNPCDestroyed);
	}
}

void UPACS_HoverProbeComponent::UnbindInputDelegates()
{
	// Clean up any input context change delegates
	if (InputContextHandle.IsValid())
	{
		// Would unbind from InputHandler context change delegate here
		InputContextHandle.Reset();
	}
}

bool UPACS_HoverProbeComponent::IsInputContextActive() const
{
	// If no specific contexts are configured, always allow hover (legacy behavior)
	if (ActiveInputContexts.Num() == 0)
	{
		return true;
	}

	if (!OwnerPC.IsValid())
	{
		return false;
	}

	UPACS_InputHandlerComponent* InputHandler = OwnerPC->GetInputHandler();
	if (!InputHandler)
	{
		return false;
	}

	// Check if any of our configured contexts are currently active
	UInputMappingContext* CurrentContext = GetCurrentActiveContext();
	if (!CurrentContext)
	{
		return false;
	}

	// Check if current context is in our allowed list
	for (const TObjectPtr<UInputMappingContext>& AllowedContext : ActiveInputContexts)
	{
		if (AllowedContext.Get() == CurrentContext)
		{
			return true;
		}
	}

	return false;
}

UInputMappingContext* UPACS_HoverProbeComponent::GetCurrentActiveContext() const
{
	if (!OwnerPC.IsValid())
	{
		return nullptr;
	}

	UPACS_InputHandlerComponent* InputHandler = OwnerPC->GetInputHandler();
	if (!InputHandler)
	{
		return nullptr;
	}

	// Get the current active base context from InputHandler
	return InputHandler->GetCurrentBaseContext();
}