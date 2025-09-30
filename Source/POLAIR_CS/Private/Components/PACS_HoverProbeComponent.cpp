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

	UE_LOG(LogTemp, Warning, TEXT("HoverProbe BeginPlay: START - ConfigApplied=%d, TickEnabled=%d"),
		bConfigurationApplied, IsComponentTickEnabled());

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
			UE_LOG(LogTemp, Warning, TEXT("HoverProbe BeginPlay: Using default SelectionObject type (ECC_GameTraceChannel2)"));
		}
	}

	// Log configured object types
	for (int32 i = 0; i < HoverObjectTypes.Num(); i++)
	{
		UE_LOG(LogTemp, Log, TEXT("HoverProbe BeginPlay: Using ObjectType[%d] = %d"), i, (int32)HoverObjectTypes[i]);
	}

	// Force enable ticking as failsafe
	SetComponentTickEnabled(true);

	// Final status log
	UE_LOG(LogTemp, Warning, TEXT("HoverProbe BeginPlay: END - Owner=%s, TickEnabled=%d, TickInterval=%.3f, ObjectTypes=%d"),
		OwnerPC.IsValid() ? *OwnerPC->GetName() : TEXT("INVALID"),
		IsComponentTickEnabled(),
		GetComponentTickInterval(),
		HoverObjectTypes.Num());
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

	// Heartbeat log every second (30 ticks at 30Hz)
	static int32 TickCounter = 0;
	if (++TickCounter >= 30)
	{
		TickCounter = 0;
		UE_LOG(LogTemp, Log, TEXT("HoverProbe: HEARTBEAT - Ticking at %.1fHz, Owner=%s"),
			RateHz, OwnerPC.IsValid() ? *OwnerPC->GetName() : TEXT("INVALID"));
	}

	ProbeOnce();
}

void UPACS_HoverProbeComponent::ProbeOnce()
{
	// Log entry to ProbeOnce every 10 probes
	static int32 ProbeCounter = 0;
	static int32 DebugCounter = 0;
	if (++ProbeCounter >= 10)
	{
		ProbeCounter = 0;
		UE_LOG(LogTemp, Verbose, TEXT("HoverProbe: ProbeOnce() executing (Owner=%s)"),
			OwnerPC.IsValid() ? *OwnerPC->GetName() : TEXT("INVALID"));
	}

	if (!OwnerPC.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("HoverProbe: ProbeOnce() - OwnerPC is invalid, aborting"));
		return;
	}

	// Log trace config every 60 frames (2 seconds at 30Hz)
	if (++DebugCounter >= 60)
	{
		DebugCounter = 0;
		FString ObjectTypeList;
		for (int32 i = 0; i < HoverObjectTypes.Num(); i++)
		{
			ObjectTypeList += FString::Printf(TEXT("%d "), (int32)HoverObjectTypes[i]);
		}
		UE_LOG(LogTemp, Warning, TEXT("HoverProbe DEBUG: Tracing with ObjectTypes=[%s]"), *ObjectTypeList);
	}

	// Perform line trace from cursor using object type query for selection planes
	FHitResult HitResult;
	bool bHit = false;

	// Use object type query if configured
	if (HoverObjectTypes.Num() > 0)
	{
		bHit = OwnerPC->GetHitResultUnderCursorForObjects(HoverObjectTypes, false, HitResult);

		// Debug: Log trace attempt with object types
		if (!bHit)
		{
			// Try ALL possible channels to find what works
			static int32 ChannelTestCounter = 0;
			if (++ChannelTestCounter >= 30) // Every second
			{
				ChannelTestCounter = 0;

				// Test all channels
				FHitResult TestHit;
				bool bHitAny = false;

				// Test SelectionTrace channel (ECC_GameTraceChannel1)
				if (OwnerPC->GetHitResultUnderCursor(ECC_GameTraceChannel1, false, TestHit))
				{
					UE_LOG(LogTemp, Warning, TEXT("HoverProbe: HIT on SelectionTrace (ECC_GameTraceChannel1)! Actor=%s"),
						TestHit.GetActor() ? *TestHit.GetActor()->GetName() : TEXT("null"));
					bHitAny = true;
				}

				// Test SelectionObject channel (ECC_GameTraceChannel2)
				if (OwnerPC->GetHitResultUnderCursor(ECC_GameTraceChannel2, false, TestHit))
				{
					UE_LOG(LogTemp, Warning, TEXT("HoverProbe: HIT on SelectionObject (ECC_GameTraceChannel2)! Actor=%s"),
						TestHit.GetActor() ? *TestHit.GetActor()->GetName() : TEXT("null"));
					bHitAny = true;
				}

				// Test Visibility channel
				if (OwnerPC->GetHitResultUnderCursor(ECC_Visibility, false, TestHit))
				{
					UE_LOG(LogTemp, Warning, TEXT("HoverProbe: HIT on Visibility channel! Actor=%s"),
						TestHit.GetActor() ? *TestHit.GetActor()->GetName() : TEXT("null"));
					bHitAny = true;
				}

				if (!bHitAny)
				{
					UE_LOG(LogTemp, Warning, TEXT("HoverProbe: NO HITS on any channel (GT1, GT2, or Visibility)"));
				}
			}

			// Original fallback logic
			bHit = OwnerPC->GetHitResultUnderCursor(ECC_GameTraceChannel1, false, HitResult);
			if (bHit)
			{
				UE_LOG(LogTemp, Warning, TEXT("HoverProbe: Fallback HIT via SelectionTrace channel!"));
			}
		}
		else
		{
			// We got a hit with object type query - log it!
			UE_LOG(LogTemp, Warning, TEXT("HoverProbe: ObjectType query SUCCESS! Hit actor=%s"),
				HitResult.GetActor() ? *HitResult.GetActor()->GetName() : TEXT("null"));
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

		if (bHit)
		{
			UE_LOG(LogTemp, Warning, TEXT("HoverProbe: Direct channel trace hit! Actor=%s"),
				HitResult.GetActor() ? *HitResult.GetActor()->GetName() : TEXT("null"));
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
					// Check if NPC is already selected by another player - don't hover if unavailable
					bool bCanHover = true;

					// Try to get the NPC via interface to check selection state (works for all NPC types)
					if (IPACS_SelectableCharacterInterface* Selectable = Cast<IPACS_SelectableCharacterInterface>(HitActor))
					{
						if (Selectable->IsSelectedBy(nullptr)) // Check if selected by anyone
						{
							// Check if it's selected by someone else
							if (UWorld* World = GetWorld())
							{
								if (APlayerController* PC = World->GetFirstPlayerController())
								{
									if (APlayerState* LocalPS = PC->GetPlayerState<APlayerState>())
									{
										APlayerState* NPCSelector = Selectable->GetCurrentSelector();

										// If selected by someone else, don't allow hover
										if (NPCSelector && NPCSelector != LocalPS)
										{
											bCanHover = false;
											UE_LOG(LogTemp, Verbose, TEXT("HoverProbe: Cannot hover - NPC '%s' selected by another player"),
												*HitActor->GetName());
										}
									}
								}
							}
						}
					}

					if (bCanHover)
					{
						// Log successful hit on compatible collision with detailed info
						UActorComponent* HitComp = HitResult.GetComponent();
						FString CollisionInfo = TEXT("N/A");
						if (HitComp && HitComp->IsA<UPrimitiveComponent>())
						{
							UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(HitComp);
							CollisionInfo = FString::Printf(TEXT("ObjectType=%d, ProfileName=%s"),
								(int32)PrimComp->GetCollisionObjectType(),
								*PrimComp->GetCollisionProfileName().ToString());
						}

						UE_LOG(LogTemp, Log, TEXT("HoverProbe: Hit poolable actor '%s' with SelectionPlane at distance %.1f (Component: %s, %s)"),
							*HitActor->GetName(),
							HitResult.Distance,
							*GetNameSafe(HitResult.GetComponent()),
							*CollisionInfo);
					}
					else
					{
						// Don't set this as hit plane if we can't hover
						HitPlaneComponent = nullptr;
					}
				}
				else
				{
					UE_LOG(LogTemp, Verbose, TEXT("HoverProbe: Hit poolable actor '%s' but no SelectionPlane found"),
						*HitActor->GetName());
				}
			}
			else
			{
				// Hit something but it's not a poolable actor
				UE_LOG(LogTemp, Verbose, TEXT("HoverProbe: Hit non-poolable actor '%s' (Class: %s)"),
					*HitActor->GetName(),
					*HitActor->GetClass()->GetName());
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

				UE_LOG(LogTemp, Log, TEXT("HoverProbe: Activated hover state on '%s'"), *HitActor->GetName());
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