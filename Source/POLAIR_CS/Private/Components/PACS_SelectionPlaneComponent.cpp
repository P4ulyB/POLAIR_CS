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

UPACS_SelectionPlaneComponent::UPACS_SelectionPlaneComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// This component replicates selection state only
	SetIsReplicatedByDefault(true);
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
	UE_LOG(LogTemp, Warning, TEXT("PACS_SelectionPlaneComponent::InitializeSelectionPlane() - Owner: %s"), Owner ? *Owner->GetName() : TEXT("NULL"));

	if (bIsInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("  -> Already initialized, skipping"));
		return;
	}

	if (!ShouldShowSelectionVisuals())
	{
		UE_LOG(LogTemp, Warning, TEXT("  -> ShouldShowSelectionVisuals() returned false, skipping"));
		return;
	}

	if (!Owner)
	{
		UE_LOG(LogTemp, Error, TEXT("  -> No Owner!"));
		return;
	}

	// CREATE selection plane component dynamically (client-only)
	// This ensures it NEVER exists on servers
	// Use unique name to avoid conflicts with other components
	SelectionPlane = NewObject<UStaticMeshComponent>(Owner, TEXT("SelectionPlaneMesh"), RF_Transient);

	if (!SelectionPlane)
	{
		UE_LOG(LogTemp, Error, TEXT("  -> Failed to create selection plane mesh component"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("  -> SelectionPlane mesh component created: %s"), *SelectionPlane->GetName());

	// Setup attachment
	if (USceneComponent* RootComp = Owner->GetRootComponent())
	{
		SelectionPlane->SetupAttachment(RootComp);
		SelectionPlane->RegisterComponent();
		UE_LOG(LogTemp, Warning, TEXT("  -> Attached to root: %s"), *RootComp->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("  -> No root component!"));
	}

	// Setup the selection plane with validated assets
	SetupSelectionPlane();
	bIsInitialized = true;
	UE_LOG(LogTemp, Warning, TEXT("  -> Initialization complete"));
}

void UPACS_SelectionPlaneComponent::SetupSelectionPlane()
{
	if (!SelectionPlane)
	{
		return;
	}

	// Configure collision (trace channel set later in ApplyProfileAsset)
	SelectionPlane->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SelectionPlane->SetCollisionResponseToAllChannels(ECR_Ignore);
	// Specific channel response set in ApplyProfileAsset()

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

	// Now set actual initial values (Available state - white with brightness 1.0)
	SelectionPlane->SetCustomPrimitiveDataFloat(0, 1.0f);  // R
	SelectionPlane->SetCustomPrimitiveDataFloat(1, 1.0f);  // G
	SelectionPlane->SetCustomPrimitiveDataFloat(2, 1.0f);  // B
	SelectionPlane->SetCustomPrimitiveDataFloat(3, 1.0f);  // Brightness
	SelectionPlane->SetCustomPrimitiveDataFloat(4, 0.8f);  // Alpha

	// Start visible - appearance controlled by CPD values
	SelectionPlane->SetVisibility(true);

	UE_LOG(LogTemp, Warning, TEXT("SetupSelectionPlane: Set initial CPD - Color=(1.0,1.0,1.0), Brightness=1.0, Alpha=0.8"));

	// Verify CPD was set (only check indices 0-4)
	const FCustomPrimitiveData& CPD = SelectionPlane->GetCustomPrimitiveData();
	for (int32 i = 0; i < FMath::Min(5, CPD.Data.Num()); i++)
	{
		UE_LOG(LogTemp, Warning, TEXT("  CPD[%d] = %.2f"), i, CPD.Data[i]);
	}
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

	// Store state visuals for CPD updates (indexed by ESelectionVisualState)
	StateVisuals[0] = {InHoveredColor, InHoveredBrightness};			// Hovered
	StateVisuals[1] = {InSelectedColor, InSelectedBrightness};			// Selected
	StateVisuals[2] = {InUnavailableColor, InUnavailableBrightness};	// Unavailable
	StateVisuals[3] = {InAvailableColor, InAvailableBrightness};		// Available

	// Log the cached values for debugging
	UE_LOG(LogTemp, Warning, TEXT("ApplyCachedColorValues: Applying cached color values to SelectionPlaneComponent:"));
	UE_LOG(LogTemp, Warning, TEXT("  Available: Color=(%.2f,%.2f,%.2f,%.2f), Brightness=%.2f"),
		StateVisuals[3].Color.R, StateVisuals[3].Color.G, StateVisuals[3].Color.B, StateVisuals[3].Color.A, StateVisuals[3].Brightness);
	UE_LOG(LogTemp, Warning, TEXT("  Hovered: Color=(%.2f,%.2f,%.2f,%.2f), Brightness=%.2f"),
		StateVisuals[0].Color.R, StateVisuals[0].Color.G, StateVisuals[0].Color.B, StateVisuals[0].Color.A, StateVisuals[0].Brightness);
	UE_LOG(LogTemp, Warning, TEXT("  Selected: Color=(%.2f,%.2f,%.2f,%.2f), Brightness=%.2f"),
		StateVisuals[1].Color.R, StateVisuals[1].Color.G, StateVisuals[1].Color.B, StateVisuals[1].Color.A, StateVisuals[1].Brightness);
	UE_LOG(LogTemp, Warning, TEXT("  Unavailable: Color=(%.2f,%.2f,%.2f,%.2f), Brightness=%.2f"),
		StateVisuals[2].Color.R, StateVisuals[2].Color.G, StateVisuals[2].Color.B, StateVisuals[2].Color.A, StateVisuals[2].Brightness);

	// Update visuals with cached data
	if (SelectionPlane)
	{
		UpdateSelectionPlaneCPD();
		UpdateVisuals();
	}
}

void UPACS_SelectionPlaneComponent::ApplyProfileAsset(UPACS_SelectionProfileAsset* ProfileAsset)
{
	AActor* Owner = GetOwner();
	UE_LOG(LogTemp, Warning, TEXT("PACS_SelectionPlaneComponent::ApplyProfileAsset() - Owner: %s, Profile: %s"),
		Owner ? *Owner->GetName() : TEXT("NULL"),
		ProfileAsset ? *ProfileAsset->GetName() : TEXT("NULL"));

	if (!ProfileAsset)
	{
		UE_LOG(LogTemp, Error, TEXT("  -> No ProfileAsset provided!"));
		return;
	}

	// Store the current profile
	CurrentProfileAsset = ProfileAsset;

	// Skip visual application on dedicated servers
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		UE_LOG(LogTemp, Warning, TEXT("  -> Skipping (Dedicated Server)"));
		return;
	}

	// ========================================
	// SELECTION PLANE VISUALS ONLY
	// (NPC mesh assignment removed - handled by NPC classes)
	// ========================================

	if (!SelectionPlane)
	{
		UE_LOG(LogTemp, Error, TEXT("  -> SelectionPlane is NULL! Was InitializeSelectionPlane() called?"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("  -> SelectionPlane exists: %s"), *SelectionPlane->GetName());

	// Apply selection plane mesh (assume pre-loaded by SpawnOrchestrator)
	if (!ProfileAsset->SelectionStaticMesh.IsNull())
	{
		if (UStaticMesh* PlaneMesh = ProfileAsset->SelectionStaticMesh.Get())
		{
			SelectionPlane->SetStaticMesh(PlaneMesh);
			SelectionPlane->SetRelativeTransform(ProfileAsset->SelectionStaticMeshTransform);
			UE_LOG(LogTemp, Warning, TEXT("  -> Static Mesh applied: %s"), *PlaneMesh->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("  -> Static Mesh is NULL (not loaded)"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("  -> No Static Mesh in profile"));
	}

	// Apply selection material (assume pre-loaded)
	if (!ProfileAsset->SelectionMaterialInstance.IsNull())
	{
		if (UMaterialInterface* Material = ProfileAsset->SelectionMaterialInstance.Get())
		{
			SelectionPlane->SetMaterial(0, Material);
			UE_LOG(LogTemp, Warning, TEXT("  -> Material applied: %s"), *Material->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("  -> Material is NULL (not loaded)"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("  -> No Material in profile"));
	}

	// Apply collision channel from profile
	SelectionPlane->SetCollisionResponseToChannel(ProfileAsset->SelectionTraceChannel, ECR_Block);
	UE_LOG(LogTemp, Warning, TEXT("  -> Collision channel set"));

	// Store state visuals for CPD updates (indexed by ESelectionVisualState)
	StateVisuals[0] = {ProfileAsset->HoveredColour, ProfileAsset->HoveredBrightness};			// Hovered
	StateVisuals[1] = {ProfileAsset->SelectedColour, ProfileAsset->SelectedBrightness};			// Selected
	StateVisuals[2] = {ProfileAsset->UnavailableColour, ProfileAsset->UnavailableBrightness};	// Unavailable
	StateVisuals[3] = {ProfileAsset->AvailableColour, ProfileAsset->AvailableBrightness};		// Available

	// Log the loaded profile values for debugging
	UE_LOG(LogTemp, Warning, TEXT("ApplyProfileAsset: Loaded state visuals from profile:"));
	UE_LOG(LogTemp, Warning, TEXT("  Available: Color=(%.2f,%.2f,%.2f,%.2f), Brightness=%.2f"),
		StateVisuals[3].Color.R, StateVisuals[3].Color.G, StateVisuals[3].Color.B, StateVisuals[3].Color.A, StateVisuals[3].Brightness);
	UE_LOG(LogTemp, Warning, TEXT("  Hovered: Color=(%.2f,%.2f,%.2f,%.2f), Brightness=%.2f"),
		StateVisuals[0].Color.R, StateVisuals[0].Color.G, StateVisuals[0].Color.B, StateVisuals[0].Color.A, StateVisuals[0].Brightness);
	UE_LOG(LogTemp, Warning, TEXT("  Selected: Color=(%.2f,%.2f,%.2f,%.2f), Brightness=%.2f"),
		StateVisuals[1].Color.R, StateVisuals[1].Color.G, StateVisuals[1].Color.B, StateVisuals[1].Color.A, StateVisuals[1].Brightness);
	UE_LOG(LogTemp, Warning, TEXT("  Unavailable: Color=(%.2f,%.2f,%.2f,%.2f), Brightness=%.2f"),
		StateVisuals[2].Color.R, StateVisuals[2].Color.G, StateVisuals[2].Color.B, StateVisuals[2].Color.A, StateVisuals[2].Brightness);

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

	LocalHoverState = bHovered ? 1 : 0;
	UpdateVisuals();
}


bool UPACS_SelectionPlaneComponent::ShouldShowSelectionVisuals() const
{
	// Never show on dedicated server
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_SelectionPlaneComponent::ShouldShowSelectionVisuals() -> FALSE (Dedicated Server)"));
		return false;
	}

	// CRITICAL: Never show selection plane on VR/HMD clients
	// But they WILL see the actual NPC meshes
	if (UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_SelectionPlaneComponent::ShouldShowSelectionVisuals() -> FALSE (HMD Enabled)"));
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("PACS_SelectionPlaneComponent::ShouldShowSelectionVisuals() -> TRUE"));

	// Only show on regular (flat screen) clients and listen servers
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

			// Try to get CurrentSelector from NPC base class
			if (APACS_NPC_Base* NPC = Cast<APACS_NPC_Base>(Owner))
			{
				NPCSelector = NPC->GetCurrentSelector();
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

	// Log what we're about to set (matching material expectations)
	UE_LOG(LogTemp, Warning, TEXT("UpdateSelectionPlaneCPD: Updating for state %d (%s)"),
		DisplayState,
		DisplayState == 0 ? TEXT("Hovered") :
		DisplayState == 1 ? TEXT("Selected") :
		DisplayState == 2 ? TEXT("Unavailable") : TEXT("Available"));
	UE_LOG(LogTemp, Warning, TEXT("  Color=(%.2f,%.2f,%.2f), Brightness=%.2f, Alpha=%.2f"),
		Visuals.Color.R, Visuals.Color.G, Visuals.Color.B, Visuals.Brightness, Visuals.Color.A);

	// Set CPD values matching material's expected indices
	// Material expects: CPD[0-2] = RGB, CPD[3] = Brightness, CPD[4] = Alpha
	SelectionPlane->SetCustomPrimitiveDataFloat(0, Visuals.Color.R);      // R
	SelectionPlane->SetCustomPrimitiveDataFloat(1, Visuals.Color.G);      // G
	SelectionPlane->SetCustomPrimitiveDataFloat(2, Visuals.Color.B);      // B
	SelectionPlane->SetCustomPrimitiveDataFloat(3, Visuals.Brightness);   // Brightness
	SelectionPlane->SetCustomPrimitiveDataFloat(4, Visuals.Color.A);      // Alpha

	// Verify what was actually set (only check the 5 indices we use)
	UE_LOG(LogTemp, Warning, TEXT("UpdateSelectionPlaneCPD: AFTER setting CPD values:"));
	const FCustomPrimitiveData& CPD = SelectionPlane->GetCustomPrimitiveData();
	if (CPD.Data.Num() >= 5)
	{
		UE_LOG(LogTemp, Warning, TEXT("  CPD[0] R = %.2f"), CPD.Data[0]);
		UE_LOG(LogTemp, Warning, TEXT("  CPD[1] G = %.2f"), CPD.Data[1]);
		UE_LOG(LogTemp, Warning, TEXT("  CPD[2] B = %.2f"), CPD.Data[2]);
		UE_LOG(LogTemp, Warning, TEXT("  CPD[3] Brightness = %.2f"), CPD.Data[3]);
		UE_LOG(LogTemp, Warning, TEXT("  CPD[4] Alpha = %.2f"), CPD.Data[4]);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("  Not enough CPD floats allocated! Only %d available"), CPD.Data.Num());
	}
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