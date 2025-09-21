#include "Components/PACS_HoverProbe.h"
#include "PACS_PlayerController.h"
#include "Pawns/NPC/PACS_NPCCharacter.h"
#include "Actors/PACS_SelectionCueProxy.h"
#include "PACS_InputHandlerComponent.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "CollisionQueryParams.h"

UPACS_HoverProbe::UPACS_HoverProbe()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UPACS_HoverProbe::BeginPlay()
{
	Super::BeginPlay();

	OwnerPC = Cast<APACS_PlayerController>(GetOwner());

	// Run at ~30 Hz irrespective of frame rate
	const float Interval = 1.0f / FMath::Max(1.0f, RateHz);
	SetComponentTickInterval(Interval);

	// Default object type if not set in editor: GameTraceChannel1
	if (HoverObjectTypes.Num() == 0)
	{
		HoverObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_GameTraceChannel1));
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

void UPACS_HoverProbe::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Comprehensive cleanup on component end
	ClearHover();
	UnbindInputDelegates();

	Super::EndPlay(EndPlayReason);
}

void UPACS_HoverProbe::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	// Final cleanup safety net
	ClearHover();
	UnbindInputDelegates();

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UPACS_HoverProbe::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
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

void UPACS_HoverProbe::ProbeOnce()
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

		if (GetWorld()->LineTraceSingleByChannel(Block, CamLoc, Hit.ImpactPoint, ECC_Visibility, Params))
		{
			if (Block.GetActor() && Block.GetActor() != Hit.GetActor())
			{
				ClearHover();
				return; // occluded
			}
		}
	}

	if (APACS_SelectionCueProxy* NewProxy = ResolveProxyFrom(Hit))
	{
		if (NewProxy != CurrentProxy.Get())
		{
			// Clear previous hover with proper cleanup
			if (CurrentProxy.IsValid())
			{
				UnbindProxyDelegates();
				CurrentProxy->SetLocalHovered(false);
			}

			// Set new hover with delegate binding
			CurrentProxy = NewProxy;
			if (CurrentProxy.IsValid())
			{
				CurrentProxy->SetLocalHovered(true); // purely local, no RPC
				CurrentProxy->OnDestroyed.AddDynamic(this, &UPACS_HoverProbe::OnProxyDestroyed);
			}
		}
		return;
	}

	ClearHover();
}

APACS_SelectionCueProxy* UPACS_HoverProbe::ResolveProxyFrom(const FHitResult& Hit) const
{
	if (AActor* HitActor = Hit.GetActor())
	{
		// Direct proxy hit
		if (APACS_SelectionCueProxy* Proxy = Cast<APACS_SelectionCueProxy>(HitActor))
		{
			return Proxy;
		}

		// NPC hit - get its proxy
		if (APACS_NPCCharacter* NPC = Cast<APACS_NPCCharacter>(HitActor))
		{
			// Note: This would need a GetSelectionProxy() method on NPCCharacter
			// return NPC->GetSelectionProxy();
		}

		// Child component hit - check owner
		if (AActor* Owner = HitActor->GetOwner())
		{
			if (APACS_NPCCharacter* NPC = Cast<APACS_NPCCharacter>(Owner))
			{
				// return NPC->GetSelectionProxy();
			}
		}
	}
	return nullptr;
}

void UPACS_HoverProbe::ClearHover()
{
	if (CurrentProxy.IsValid())
	{
		UnbindProxyDelegates();
		CurrentProxy->SetLocalHovered(false);
	}
	CurrentProxy = nullptr;
}

void UPACS_HoverProbe::OnProxyDestroyed(AActor* DestroyedActor)
{
	// Automatic cleanup when proxy is destroyed mid-hover
	if (CurrentProxy.Get() == DestroyedActor)
	{
		CurrentProxy = nullptr; // Don't call SetLocalHovered on destroyed actor
	}
}

void UPACS_HoverProbe::OnInputContextChanged()
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

void UPACS_HoverProbe::UnbindProxyDelegates()
{
	if (CurrentProxy.IsValid())
	{
		CurrentProxy->OnDestroyed.RemoveDynamic(this, &UPACS_HoverProbe::OnProxyDestroyed);
	}
}

void UPACS_HoverProbe::UnbindInputDelegates()
{
	// Clean up any input context change delegates
	if (InputContextHandle.IsValid())
	{
		// Would unbind from InputHandler context change delegate here
		InputContextHandle.Reset();
	}
}

bool UPACS_HoverProbe::IsInputContextActive() const
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

UInputMappingContext* UPACS_HoverProbe::GetCurrentActiveContext() const
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