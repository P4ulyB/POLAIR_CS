#include "Components/PACS_SelectionPlaneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Data/PACS_SelectionProfile.h"
#include "Actors/NPC/PACS_NPC_Base.h"
#include "Interfaces/PACS_SelectableCharacterInterface.h"

UPACS_SelectionPlaneComponent::UPACS_SelectionPlaneComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// This component replicates selection state only
	SetIsReplicatedByDefault(true);

	// Initialize StateVisuals with neutral defaults
	// These MUST be overridden by data asset values - the data asset is the source of truth
	// Using white with 0 alpha makes them invisible until proper colors are applied
	for (int32 i = 0; i < 4; i++)
	{
		StateVisuals[i] = {FLinearColor(1.0f, 1.0f, 1.0f, 0.0f), 1.0f};  // White, invisible
	}
}

void UPACS_SelectionPlaneComponent::BeginPlay()
{
	Super::BeginPlay();

	// Only initialize visuals on non-VR clients (not server, not VR)
	if (ShouldShowSelectionVisuals())
	{
		InitializeSelectionPlane();
	}
}

void UPACS_SelectionPlaneComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clean up dynamically created selection plane
	if (SelectionPlane)
	{
		SelectionPlane->DestroyComponent();
		SelectionPlane = nullptr;
	}

	// Clear cached references
	CachedSelectionMaterial = nullptr;
	CachedPlaneMesh = nullptr;
	bAssetsValidated = false;

	Super::EndPlay(EndPlayReason);
}

void UPACS_SelectionPlaneComponent::InitializeSelectionPlane()
{
	AActor* Owner = GetOwner();

	if (bIsInitialized || !ShouldShowSelectionVisuals() || !Owner)
	{
		return;
	}

	// CREATE selection plane component dynamically (client-only)
	SelectionPlane = NewObject<UStaticMeshComponent>(Owner, TEXT("SelectionPlaneMesh"), RF_Transient);

	if (!SelectionPlane)
	{
		UE_LOG(LogTemp, Error, TEXT("SelectionPlaneComponent: Failed to create selection plane for %s"), *Owner->GetName());
		return;
	}

	// Setup attachment
	if (USceneComponent* RootComp = Owner->GetRootComponent())
	{
		SelectionPlane->SetupAttachment(RootComp);
		SelectionPlane->RegisterComponent();
	}

	// Setup the selection plane with validated assets
	SetupSelectionPlane();
	bIsInitialized = true;
}

void UPACS_SelectionPlaneComponent::SetupSelectionPlane()
{
	if (!SelectionPlane)
	{
		return;
	}

	// Configure collision for selection detection
	SelectionPlane->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SelectionPlane->SetCollisionObjectType(ECC_GameTraceChannel2); // SelectionObject type
	SelectionPlane->SetCollisionResponseToAllChannels(ECR_Ignore);
	SelectionPlane->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block); // Block SelectionTrace channel
	SelectionPlane->SetCollisionProfileName(TEXT("SelectionProfile"));

	// Visual settings for performance
	SelectionPlane->SetCastShadow(false);
	SelectionPlane->SetReceivesDecals(false);
	SelectionPlane->bUseAsOccluder = false;
	SelectionPlane->SetGenerateOverlapEvents(false);

	// Mark as not replicated (client-only visual)
	SelectionPlane->SetIsReplicated(false);

	// Initialize CPD with default Available state values
	// Material expects: CPD[0-2] = RGB, CPD[3] = Brightness, CPD[4] = Alpha (optional)
	// Set default values first
	SelectionPlane->SetDefaultCustomPrimitiveDataFloat(0, 1.0f);  // R
	SelectionPlane->SetDefaultCustomPrimitiveDataFloat(1, 1.0f);  // G
	SelectionPlane->SetDefaultCustomPrimitiveDataFloat(2, 1.0f);  // B
	SelectionPlane->SetDefaultCustomPrimitiveDataFloat(3, 1.0f);  // Brightness
	SelectionPlane->SetDefaultCustomPrimitiveDataFloat(4, 0.8f);  // Alpha (optional)

	// Set initial CPD values from StateVisuals array (Available state - index 3)
	const FSelectionStateVisuals& AvailableVisuals = StateVisuals[3];
	SelectionPlane->SetCustomPrimitiveDataFloat(0, AvailableVisuals.Color.R);  // R
	SelectionPlane->SetCustomPrimitiveDataFloat(1, AvailableVisuals.Color.G);  // G
	SelectionPlane->SetCustomPrimitiveDataFloat(2, AvailableVisuals.Color.B);  // B
	SelectionPlane->SetCustomPrimitiveDataFloat(3, AvailableVisuals.Brightness);  // Brightness
	SelectionPlane->SetCustomPrimitiveDataFloat(4, AvailableVisuals.Color.A);  // Alpha

	// Start visible - appearance controlled by CPD values
	SelectionPlane->SetVisibility(true);
}

void UPACS_SelectionPlaneComponent::ValidateAndApplyAssets()
{
	if (!SelectionPlane || bAssetsValidated || !CurrentProfileAsset)
	{
		return;
	}

	// Validate and apply plane mesh
	if (!CurrentProfileAsset->SelectionStaticMesh.IsNull())
	{
		CachedPlaneMesh = CurrentProfileAsset->SelectionStaticMesh.Get();
		if (!CachedPlaneMesh)
		{
			// PERFORMANCE WARNING: Synchronous load on game thread
			CachedPlaneMesh = CurrentProfileAsset->SelectionStaticMesh.LoadSynchronous();
			if (!CachedPlaneMesh)
			{
				UE_LOG(LogTemp, Warning, TEXT("PACS_SelectionPlaneComponent: Selection plane mesh not available"));
			}
		}

		if (CachedPlaneMesh)
		{
			SelectionPlane->SetStaticMesh(CachedPlaneMesh);
		}
	}

	// Validate and apply selection material
	if (!CurrentProfileAsset->SelectionMaterialInstance.IsNull())
	{
		CachedSelectionMaterial = CurrentProfileAsset->SelectionMaterialInstance.Get();
		if (!CachedSelectionMaterial)
		{
			// PERFORMANCE WARNING: Synchronous load on game thread
			CachedSelectionMaterial = CurrentProfileAsset->SelectionMaterialInstance.LoadSynchronous();
			if (!CachedSelectionMaterial)
			{
				UE_LOG(LogTemp, Warning, TEXT("PACS_SelectionPlaneComponent: Selection material not available"));
			}
		}

		if (CachedSelectionMaterial)
		{
			SelectionPlane->SetMaterial(0, CachedSelectionMaterial);
		}
	}

	bAssetsValidated = true;
}


void UPACS_SelectionPlaneComponent::ApplyCachedColorValues(
	const FLinearColor& InAvailableColor, float InAvailableBrightness,
	const FLinearColor& InHoveredColor, float InHoveredBrightness,
	const FLinearColor& InSelectedColor, float InSelectedBrightness,
	const FLinearColor& InUnavailableColor, float InUnavailableBrightness)
{
	// Skip on dedicated servers
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// CRITICAL: Store state visuals from data asset (via cached profile data)
	// These values come from the data asset and are the SOURCE OF TRUTH
	StateVisuals[0] = {InHoveredColor, InHoveredBrightness};			// Hovered
	StateVisuals[1] = {InSelectedColor, InSelectedBrightness};			// Selected
	StateVisuals[2] = {InUnavailableColor, InUnavailableBrightness};	// Unavailable
	StateVisuals[3] = {InAvailableColor, InAvailableBrightness};		// Available

	// Initialize selection plane if not already done (for late color application)
	if (!bIsInitialized && ShouldShowSelectionVisuals())
	{
		InitializeSelectionPlane();
	}

	// Update visuals with cached data - force immediate update
	if (SelectionPlane)
	{
		UpdateSelectionPlaneCPD();
		UpdateVisuals();
	}
}

void UPACS_SelectionPlaneComponent::ApplyProfileAsset(UPACS_SelectionProfileAsset* ProfileAsset)
{
	if (!ProfileAsset)
	{
		return;
	}

	// Store the current profile
	CurrentProfileAsset = ProfileAsset;

	// Skip visual application on dedicated servers
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!SelectionPlane)
	{
		UE_LOG(LogTemp, Error, TEXT("SelectionPlaneComponent: SelectionPlane is NULL for %s"), *GetOwner()->GetName());
		return;
	}

	// Apply selection plane mesh (assume pre-loaded by SpawnOrchestrator)
	if (!ProfileAsset->SelectionStaticMesh.IsNull())
	{
		if (UStaticMesh* PlaneMesh = ProfileAsset->SelectionStaticMesh.Get())
		{
			SelectionPlane->SetStaticMesh(PlaneMesh);
			SelectionPlane->SetRelativeTransform(ProfileAsset->SelectionStaticMeshTransform);
		}
	}

	// Apply selection material (assume pre-loaded)
	if (!ProfileAsset->SelectionMaterialInstance.IsNull())
	{
		if (UMaterialInterface* Material = ProfileAsset->SelectionMaterialInstance.Get())
		{
			SelectionPlane->SetMaterial(0, Material);
		}
	}

	// Apply collision settings from profile
	if (ProfileAsset->SelectionTraceChannel != ECC_GameTraceChannel1)
	{
		SelectionPlane->SetCollisionResponseToChannel(ProfileAsset->SelectionTraceChannel, ECR_Block);
	}

	// Apply color values from profile - SOURCE OF TRUTH
	StateVisuals[0] = {ProfileAsset->HoveredColour, ProfileAsset->HoveredBrightness};
	StateVisuals[1] = {ProfileAsset->SelectedColour, ProfileAsset->SelectedBrightness};
	StateVisuals[2] = {ProfileAsset->UnavailableColour, ProfileAsset->UnavailableBrightness};
	StateVisuals[3] = {ProfileAsset->AvailableColour, ProfileAsset->AvailableBrightness};

	RenderDistance = ProfileAsset->RenderDistance;

	// Update visuals with new profile data
	UpdateSelectionPlaneCPD();
	UpdateVisuals();  // Apply initial Available state visibility
}


void UPACS_SelectionPlaneComponent::SetSelectionState(ESelectionVisualState NewState)
{
	// Only server can set selection state
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	SelectionState = (uint8)NewState;

	// Apply locally on server (for listen server)
	OnRep_SelectionState();
}

void UPACS_SelectionPlaneComponent::SetHoverState(bool bHovered)
{
	// Client-side only hover state
	if (!ShouldShowSelectionVisuals())
	{
		return;
	}

	// Only allow hover when NPC is in Available state (SelectionState == 3)
	// This prevents hover on Selected (1) or Unavailable (2) NPCs
	if (SelectionState != 3)
	{
		// Force clear any existing hover state when not Available
		if (LocalHoverState != 0)
		{
			LocalHoverState = 0;
			UpdateVisuals();
		}
		return;
	}

	LocalHoverState = bHovered ? 1 : 0;
	UpdateVisuals();
}


bool UPACS_SelectionPlaneComponent::ShouldShowSelectionVisuals() const
{
	// Never show on dedicated server
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	// Never show selection plane on VR/HMD clients
	if (UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
	{
		return false;
	}

	// Show on regular (flat screen) clients and listen servers
	return true;
}


void UPACS_SelectionPlaneComponent::UpdateSelectionPlaneCPD()
{
	if (!SelectionPlane)
	{
		return;
	}

	// ========================================
	// DETERMINE DISPLAY STATE
	// ========================================
	uint8 DisplayState;

	// PRIORITY 1: VR Late Joiner Override (MISSION CRITICAL)
	if (UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
	{
		DisplayState = 3; // Force Available state (neutral visibility for VR clients)
	}
	// PRIORITY 2: Local Hover (client-side)
	else if (LocalHoverState == 1)
	{
		DisplayState = 0; // Hovered
	}
	// PRIORITY 3: Selection State Differentiation
	else if (SelectionState == 1) // Selected
	{
		// Check if local player selected this NPC
		bool bIsMySelection = false;

		if (AActor* Owner = GetOwner())
		{
			// Get the NPC's CurrentSelector (who selected this NPC)
			APlayerState* NPCSelector = nullptr;

			// Try to get CurrentSelector via interface (works for all NPC types)
			if (IPACS_SelectableCharacterInterface* Selectable = Cast<IPACS_SelectableCharacterInterface>(Owner))
			{
				NPCSelector = Selectable->GetCurrentSelector();
			}

			// Get local player's PlayerState
			if (UWorld* World = GetWorld())
			{
				if (APlayerController* PC = World->GetFirstPlayerController())
				{
					if (APlayerState* LocalPS = PC->GetPlayerState<APlayerState>())
					{
						bIsMySelection = (NPCSelector == LocalPS);
					}
				}
			}
		}

		// Apply appropriate state
		if (bIsMySelection)
		{
			DisplayState = 1; // Selected by me (green/selected color)
		}
		else
		{
			DisplayState = 2; // Selected by other (red/unavailable color)
		}
	}
	// PRIORITY 4: Default to replicated state
	else
	{
		DisplayState = SelectionState; // Available, Unavailable, etc.
	}

	// Clamp to valid range (0-3 for 4 states)
	DisplayState = FMath::Clamp(DisplayState, 0, 3);

	// ========================================
	// APPLY CUSTOM PRIMITIVE DATA
	// ========================================
	const FSelectionStateVisuals& Visuals = StateVisuals[DisplayState];

	// VALIDATION: Check if colors are invalid (all zeros = not loaded from data asset)
	bool bInvalidColors = (Visuals.Color.R == 0.0f && Visuals.Color.G == 0.0f &&
	                       Visuals.Color.B == 0.0f && Visuals.Color.A == 0.0f &&
	                       Visuals.Brightness == 0.0f);

	// Set CPD values matching material's expected indices
	// Material expects: CPD[0-2] = RGB, CPD[3] = Brightness, CPD[4] = Alpha
	SelectionPlane->SetCustomPrimitiveDataFloat(0, Visuals.Color.R);      // R
	SelectionPlane->SetCustomPrimitiveDataFloat(1, Visuals.Color.G);      // G
	SelectionPlane->SetCustomPrimitiveDataFloat(2, Visuals.Color.B);      // B
	SelectionPlane->SetCustomPrimitiveDataFloat(3, Visuals.Brightness);   // Brightness
	SelectionPlane->SetCustomPrimitiveDataFloat(4, Visuals.Color.A);      // Alpha
}

void UPACS_SelectionPlaneComponent::UpdateVisuals()
{
	if (!SelectionPlane)
	{
		return;
	}

	// Update CPD (handles VR check internally)
	UpdateSelectionPlaneCPD();

	// Always keep the plane visible - state appearance controlled by material/CPD
	SelectionPlane->SetVisibility(true);
}

void UPACS_SelectionPlaneComponent::OnRep_SelectionState()
{
	// Client-side visual update based on replicated state
	UpdateVisuals();
}


void UPACS_SelectionPlaneComponent::OnAcquiredFromPool_Implementation()
{
	// Reset visual state for pool acquisition
	if (ShouldShowSelectionVisuals())
	{
		// Create selection plane if it doesn't exist yet
		if (!SelectionPlane && !bIsInitialized)
		{
			InitializeSelectionPlane();
		}

		// Re-validate assets in case they were cleared
		bAssetsValidated = false;
		if (SelectionPlane)
		{
			ValidateAndApplyAssets();
		}

		// Reset to available state
		LocalHoverState = 0;
		UpdateVisuals();
	}
}

void UPACS_SelectionPlaneComponent::OnReturnedToPool_Implementation()
{
	// Clear visual state for pool return
	LocalHoverState = 0;

	if (SelectionPlane)
	{
		SelectionPlane->SetVisibility(false);
		// Don't destroy the component - keep it for reuse
	}
}

void UPACS_SelectionPlaneComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UPACS_SelectionPlaneComponent, SelectionState);
}