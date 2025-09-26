#include "Actors/NPC/PACS_NPCCharacter.h"
#include "Data/Configs/PACS_NPCConfig.h"
#include "Data/Settings/PACS_SelectionSystemSettings.h"
#include "Core/PACS_PlayerState.h"
#include "Core/PACS_SimpleNPCAIController.h"
#include "Core/PACS_OptimizationSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Blueprint.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "AIController.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Engine/World.h"
#include "Navigation/PathFollowingComponent.h"
#include "AITypes.h"
#include "NavigationSystem.h"

APACS_NPCCharacter::APACS_NPCCharacter()
{
	// Enable ticking for distance-based optimization
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f; // 10Hz tick rate for NPCs
	bReplicates = true;
	SetNetUpdateFrequency(10.0f);

	// Disable movement component tick by default - will enable on demand
	GetCharacterMovement()->SetComponentTickEnabled(false);

	// Reduce movement component overhead
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 270.0f, 0.0f); // Reduced rotation rate

	// Optimize movement settings for NPCs
	GetCharacterMovement()->bRunPhysicsWithNoController = false;
	GetCharacterMovement()->bForceMaxAccel = true; // Skip acceleration calculations
	GetCharacterMovement()->MaxSimulationIterations = 1; // Reduce physics iterations
	GetCharacterMovement()->bEnablePhysicsInteraction = false; // NPCs don't need physics interaction

	// Client-only visual components - not needed on dedicated server
#if !UE_SERVER
	// Create collision box component
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->SetupAttachment(GetMesh());
	CollisionBox->SetCollisionProfileName(TEXT("Pawn"));
	CollisionBox->SetRelativeLocation(FVector::ZeroVector);

	// Create decal component nested in collision box
	CollisionDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("CollisionDecal"));
	CollisionDecal->SetupAttachment(CollisionBox);
	CollisionDecal->SetRelativeLocation(FVector::ZeroVector);
	CollisionDecal->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f)); // Point downward by default
	CollisionDecal->DecalSize = FVector(100.0f, 100.0f, 100.0f); // Initial size, will be updated
#endif

	// Initialize movement state tracking
	bIsMoving = false;
	LastMovementTime = 0.0f;
	MovementTimeoutDuration = 2.0f; // 2 seconds without velocity change before stopping movement tick
}

void APACS_NPCCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Use standard replication instead of COND_InitialOnly to avoid dormancy race condition
	DOREPLIFETIME(APACS_NPCCharacter, VisualConfig);
	DOREPLIFETIME(APACS_NPCCharacter, CurrentSelector);
}

void APACS_NPCCharacter::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	if (HasAuthority())
	{
		checkf(NPCConfigAsset, TEXT("NPCConfigAsset must be set before startup"));
		BuildVisualConfigFromAsset_Server(); // fill VisualConfig here
	}
}

void APACS_NPCCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Configure AI controller for movement
	// Use simple custom AI controller that properly stops movement
	if (!AIControllerClass)
	{
		AIControllerClass = APACS_SimpleNPCAIController::StaticClass();
		// Disable automatic AI possession - NPCs should only move when commanded
		AutoPossessAI = EAutoPossessAI::Disabled;
	}

	// Apply visual settings earlier in initialization to prevent AI from overriding transforms
	// This runs after PreInitializeComponents (where VisualConfig is built) but before BeginPlay

	// Server: Apply global selection settings after base visual config is built
	if (HasAuthority())
	{
		ApplyGlobalSelectionSettings();
	}

	// Client: if VisualConfig already valid and not a pooled character, apply visuals
	// Pooled characters will have visuals applied directly by the pool
	if (!HasAuthority() && VisualConfig.FieldsMask != 0 && !bVisualsApplied && !bIsPooledCharacter)
	{
		ApplyVisuals_Client();
	}
}

void APACS_NPCCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Visual application moved to PostInitializeComponents for earlier timing
	// BeginPlay can now focus on other initialization that needs to happen after components are fully set up

	// AI controller is now set in constructor for proper pooling support

	// Set stable network frequency once (not changing per-tick)
	if (HasAuthority())
	{
		SetNetUpdateFrequency(10.0f);
		SetMinNetUpdateFrequency(2.0f);

		// Epic pattern: Start with dormancy for idle NPCs
		// Will wake on selection or movement
		SetNetDormancy(DORM_Initial);
	}

	// Client-side initial optimizations - SET ONCE, not every tick!
#if !UE_SERVER
	// Apply optimization settings once at startup
	UpdateDistanceBasedOptimizations();

	// Register with optimization subsystem for animation budget and significance
	if (!IsRunningDedicatedServer())
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UPACS_OptimizationSubsystem* OptSubsystem = GI->GetSubsystem<UPACS_OptimizationSubsystem>())
			{
				OptSubsystem->RegisterNPCForOptimization(this);
			}
		}
	}
#endif

	// Configure Character Movement Component for NPCs
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		// Don't disable CMC at start - it breaks initial movement
		// It will be managed during movement start/stop

		// Reduce tick interval for NPCs - they don't need 60Hz movement updates
		CMC->SetComponentTickInterval(0.05f); // 20Hz is a good balance

		// Additional movement optimizations
		CMC->bUseRVOAvoidance = false; // Disable expensive avoidance for NPCs
		CMC->bCanWalkOffLedges = true; // Simpler movement
		CMC->MaxSimulationIterations = 1; // Reduce physics iterations
	}
}

void APACS_NPCCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// REMOVED per-tick optimization updates - this was killing performance!
	// Optimizations are now handled by SignificanceManager and AnimationBudgetAllocator

	// Server-side movement completion check
	if (HasAuthority())
	{
		CheckMovementCompletion(DeltaTime);
	}
}

void APACS_NPCCharacter::CheckMovementCompletion(float DeltaTime)
{
	// Deprecated - movement completion now handled by OnAIMoveCompleted callback
	// This function kept for compatibility but will be removed
}

void APACS_NPCCharacter::OnAIMoveCompleted()
{
	// Epic pattern: Server authority check
	if (!HasAuthority())
	{
		return;
	}

	// Disable CMC tick when movement completes
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->SetComponentTickEnabled(false);
	}

	// Clear movement state
	bIsMoving = false;
	LastMovementTime = 0.0f;

	// Return to dormancy when movement completes and NPC is idle
	if (!CurrentSelector)
	{
		SetNetDormancy(DORM_Initial);
	}

	UE_LOG(LogTemp, Verbose, TEXT("[NPC MOVE] %s completed movement, dormancy: %s"),
		*GetName(),
		CurrentSelector ? TEXT("Active (selected)") : TEXT("Dormant"));
}

void APACS_NPCCharacter::UpdateDistanceBasedOptimizations()
{
	// Client-only visual optimizations - server handles via RepGraph
#if !UE_SERVER
	if (IsRunningDedicatedServer())
	{
		return;
	}

	// CRITICAL: Aggressive optimization settings to fix RHI/Render thread bottleneck
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		// Enable Update Rate Optimizations (URO)
		MeshComp->bEnableUpdateRateOptimizations = true;

		// Only tick pose when rendered
		MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

		// Enable component use of fixed skel bounds to avoid bounds calculation
		MeshComp->bComponentUseFixedSkelBounds = true;

		// Disable ALL expensive features
		MeshComp->bDisableClothSimulation = true;

		// CRITICAL: Disable MESH collision but keep capsule collision for movement
		// The mesh doesn't need collision, only the capsule component does
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// Ensure the capsule component still has collision for movement
		if (UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
		{
			// Keep capsule collision for movement system
			CapsuleComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			CapsuleComp->SetCollisionResponseToAllChannels(ECR_Block);
			CapsuleComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		}

		// Reduce capsule shadows overhead
		MeshComp->bCastDynamicShadow = false;
		MeshComp->CastShadow = false;

		// Use lowest LOD by default
		MeshComp->SetForcedLOD(1);

		// Disable morphs/blend shapes
		MeshComp->bDisableMorphTarget = true;

		// Reduce skeletal mesh evaluation rate
		MeshComp->bNoSkeletonUpdate = false; // Keep skeleton updates but optimize them
		MeshComp->bUpdateJointsFromAnimation = false; // Don't update physics from animation

		// Disable unnecessary bone updates
		MeshComp->KinematicBonesUpdateType = EKinematicBonesUpdateToPhysics::SkipAllBones;

		// Lower render priority
		MeshComp->TranslucencySortPriority = -100;

		UE_LOG(LogTemp, Verbose, TEXT("PACS_NPCCharacter: Applied aggressive optimizations to %s"), *GetName());
	}
#endif
}

void APACS_NPCCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clean up selection on server when NPC is destroyed
	if (HasAuthority() && CurrentSelector)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] NPC EndPlay - %s was selected by %s, cleaning up"),
			*GetName(), *CurrentSelector->GetPlayerName());

		// Clear the selector's reference to this NPC
		if (APACS_PlayerState* PS = Cast<APACS_PlayerState>(CurrentSelector))
		{
			if (PS->GetSelectedNPC() == this)
			{
				UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] Clearing PlayerState selection reference"));
				PS->SetSelectedNPC(nullptr);
			}
		}

		// Clear our selector reference and push the update
		CurrentSelector = nullptr;
		ForceNetUpdate();
	}
	else if (HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SELECTION DEBUG] NPC EndPlay - %s had no selector"), *GetName());
	}

	Super::EndPlay(EndPlayReason);
}


void APACS_NPCCharacter::OnRep_VisualConfig()
{
	// Skip if this is a pooled character (visuals already applied by pool)
	if (!HasAuthority() && !bVisualsApplied && !bIsPooledCharacter)
	{
		ApplyVisuals_Client();
	}
}

void APACS_NPCCharacter::ApplyVisuals_Client()
{
	// Skip all visual loading on dedicated server
#if UE_SERVER
	return;
#else
	if (IsRunningDedicatedServer() || !GetMesh())
	{
		return;
	}

	TArray<FSoftObjectPath> ToLoad;
	if (VisualConfig.FieldsMask & 0x1) ToLoad.Add(VisualConfig.MeshPath);
	if (VisualConfig.FieldsMask & 0x2) ToLoad.Add(VisualConfig.AnimClassPath);
	if (VisualConfig.FieldsMask & 0x8) ToLoad.Add(VisualConfig.DecalMaterialPath);
	if (ToLoad.Num() == 0) return;

	auto& StreamableManager = UAssetManager::GetStreamableManager();
	AssetLoadHandle = StreamableManager.RequestAsyncLoad(ToLoad, FStreamableDelegate::CreateWeakLambda(this, [this]()
	{
		USkeletalMesh* Mesh = Cast<USkeletalMesh>(VisualConfig.MeshPath.TryLoad());
		UObject* AnimObj = VisualConfig.AnimClassPath.TryLoad();

		UClass* AnimClass = nullptr;
		if (UBlueprint* BP = Cast<UBlueprint>(AnimObj)) AnimClass = BP->GeneratedClass;
		else AnimClass = Cast<UClass>(AnimObj);

		if (Mesh) GetMesh()->SetSkeletalMesh(Mesh, /*bReinitPose*/true);
		if (AnimClass) GetMesh()->SetAnimInstanceClass(AnimClass);

		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		GetMesh()->bEnableUpdateRateOptimizations = true;

		// Apply mesh transform if specified
		if (VisualConfig.FieldsMask & 0x10)
		{
			GetMesh()->SetRelativeLocation(VisualConfig.MeshLocation);
			GetMesh()->SetRelativeRotation(VisualConfig.MeshRotation);
			GetMesh()->SetRelativeScale3D(VisualConfig.MeshScale);

			UE_LOG(LogTemp, Log, TEXT("[NPC MESH] Applied mesh transforms from data asset for %s"),
				*GetName());
		}

		// Apply loaded decal material if specified
		if (VisualConfig.FieldsMask & 0x8)
		{
			UObject* DecalMaterialObj = VisualConfig.DecalMaterialPath.TryLoad();
			if (UMaterialInterface* DecalMat = Cast<UMaterialInterface>(DecalMaterialObj))
			{
				if (CollisionDecal)
				{
					// Check if selection parameters are specified
					if (VisualConfig.FieldsMask & 0x20)
					{
						// Create dynamic material instance to apply parameters
						// Epic pattern: UMaterialInstanceDynamic::Create with outer as this actor
						UMaterialInstanceDynamic* DynamicDecalMat = UMaterialInstanceDynamic::Create(DecalMat, this);
						if (DynamicDecalMat)
						{
							// Cache for direct hover access
							CachedDecalMaterial = DynamicDecalMat;

							// Apply brightness scalar parameter
							DynamicDecalMat->SetScalarParameterValue(FName(TEXT("Brightness")), VisualConfig.SelectionBrightness);

							// Apply color vector parameter
							DynamicDecalMat->SetVectorParameterValue(FName(TEXT("Colour")), VisualConfig.SelectionColour);

							// Set the dynamic material instance on the decal
							CollisionDecal->SetDecalMaterial(DynamicDecalMat);
						}
					}
					else
					{
						// No parameters specified, use material as-is
						CollisionDecal->SetDecalMaterial(DecalMat);
					}
				}
			}
		}

		// Apply collision after mesh is loaded
		ApplyCollisionFromMesh();

		bVisualsApplied = true;
	}));
#endif // !UE_SERVER
}


void APACS_NPCCharacter::BuildVisualConfigFromAsset_Server()
{
	if (!HasAuthority() || !NPCConfigAsset)
	{
		return;
	}

	// Do not load assets on dedicated server - only convert IDs
	NPCConfigAsset->ToVisualConfig(VisualConfig);

	// Optional: Apply visuals on listen server for testing
	if (!IsRunningDedicatedServer())
	{
		ApplyVisuals_Client();
	}
}

void APACS_NPCCharacter::ApplyCollisionFromMesh()
{
	// Client-only collision visualization
#if !UE_SERVER
	if (!GetMesh() || !GetMesh()->GetSkeletalMeshAsset() || !CollisionBox)
	{
		return;
	}

	// Get bounds from the skeletal mesh
	const FBoxSphereBounds Bounds = GetMesh()->GetSkeletalMeshAsset()->GetBounds();

	// Find the highest dimension from the box extent
	const FVector& BoxExtent = Bounds.BoxExtent;
	const float MaxDimension = FMath::Max3(BoxExtent.X, BoxExtent.Y, BoxExtent.Z);

	// Calculate scale factor from CollisionScaleSteps (0 = 1.0, 1 = 1.1, 10 = 2.0)
	const float ScaleFactor = 1.0f + (0.1f * static_cast<float>(VisualConfig.CollisionScaleSteps));

	// Apply uniform scaled extents using the highest dimension for all sides
	const float UniformExtent = MaxDimension * ScaleFactor;
	CollisionBox->SetBoxExtent(FVector(UniformExtent, UniformExtent, UniformExtent), true);

	// Align collision box center with mesh bounds center
	CollisionBox->SetRelativeLocation(Bounds.Origin);

	// Apply same dimensions to decal if it exists
	if (CollisionDecal)
	{
		// Decal size uses the same uniform extent for all dimensions to match collision box
		CollisionDecal->DecalSize = FVector(UniformExtent, UniformExtent, UniformExtent);
	}
#endif // !UE_SERVER
}

void APACS_NPCCharacter::ApplyGlobalSelectionSettings()
{
	// Epic pattern: Server authority check at start of mutating method
	if (!HasAuthority())
	{
		return;
	}

	// Apply global selection configuration to this character
	// This extends the existing VisualConfig with selection parameters
	VisualConfig.ApplySelectionFromGlobalSettings(GetClass());

	// Note: No need to call OnRep_VisualConfig() here as the replication
	// will be triggered automatically when VisualConfig is modified
	// The clients will receive the updated VisualConfig and apply the selection materials
}

void APACS_NPCCharacter::SetLocalHover(bool bHovered)
{
	bIsLocallyHovered = bHovered;
	UpdateVisualState();
}

void APACS_NPCCharacter::OnRep_CurrentSelector()
{
	// Dedicated servers don't need visuals
	if (IsRunningDedicatedServer())
	{
		return;
	}

	// Update visual state based on new selection
	UpdateVisualState();
}

void APACS_NPCCharacter::UpdateVisualState()
{
	// Client-only visual updates
#if !UE_SERVER
	if (!CachedDecalMaterial)
	{
		return;
	}

	// Determine which visual state to apply based on priority
	EVisualPriority Priority = GetCurrentVisualPriority();

	// Apply material parameters based on priority
	switch (Priority)
	{
	case EVisualPriority::Selected:
		CachedDecalMaterial->SetScalarParameterValue(FName("Brightness"), VisualConfig.SelectedBrightness);
		CachedDecalMaterial->SetVectorParameterValue(FName("Colour"), VisualConfig.SelectedColour);
		break;

	case EVisualPriority::Unavailable:
		CachedDecalMaterial->SetScalarParameterValue(FName("Brightness"), VisualConfig.UnavailableBrightness);
		CachedDecalMaterial->SetVectorParameterValue(FName("Colour"), VisualConfig.UnavailableColour);
		break;

	case EVisualPriority::Hovered:
		CachedDecalMaterial->SetScalarParameterValue(FName("Brightness"), VisualConfig.HoveredBrightness);
		CachedDecalMaterial->SetVectorParameterValue(FName("Colour"), VisualConfig.HoveredColour);
		break;

	case EVisualPriority::Available:
	default:
		CachedDecalMaterial->SetScalarParameterValue(FName("Brightness"), VisualConfig.AvailableBrightness);
		CachedDecalMaterial->SetVectorParameterValue(FName("Colour"), VisualConfig.AvailableColour);
		break;
	}
#endif // !UE_SERVER
}

APACS_NPCCharacter::EVisualPriority APACS_NPCCharacter::GetCurrentVisualPriority() const
{
	// Priority order: Selected > Unavailable > Hovered > Available

	// Get local player state to determine if we're the selector
	APlayerController* LocalPC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	APlayerState* LocalPlayerState = LocalPC ? LocalPC->PlayerState : nullptr;

	// Check if we're selected by local player
	if (CurrentSelector && CurrentSelector == LocalPlayerState)
	{
		return EVisualPriority::Selected;
	}

	// Check if we're selected by someone else (unavailable)
	if (CurrentSelector && CurrentSelector != LocalPlayerState)
	{
		return EVisualPriority::Unavailable;
	}

	// Check if we're being hovered (local only)
	if (bIsLocallyHovered)
	{
		return EVisualPriority::Hovered;
	}

	// Default to available
	return EVisualPriority::Available;
}

void APACS_NPCCharacter::ServerMoveToLocation_Implementation(FVector TargetLocation)
{
	// Epic pattern: Server authority check at start of mutating method
	if (!HasAuthority())
	{
		return;
	}

	// Validate target location
	FVector CurrentLocation = GetActorLocation();
	float Distance = FVector::Dist(CurrentLocation, TargetLocation);

	// Reasonable distance check
	if (Distance > 10000.0f) // 100 meters max
	{
		UE_LOG(LogTemp, Warning, TEXT("[NPC MOVE] Target location too far: %f units"), Distance);
		return;
	}

	// Validate navigation path exists
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation ProjectedLocation;
		if (!NavSys->ProjectPointToNavigation(TargetLocation, ProjectedLocation))
		{
			UE_LOG(LogTemp, Warning, TEXT("[NPC MOVE] Target location not on navmesh"));
			return;
		}
		TargetLocation = ProjectedLocation.Location;
	}

	// Event-driven CMC tick enable
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->SetComponentTickEnabled(true);
	}

	// Ensure AI controller exists and is possessed
	AAIController* AIController = Cast<AAIController>(GetController());
	if (!AIController)
	{
		// Spawn and possess AI controller
		AIController = GetWorld()->SpawnActor<APACS_SimpleNPCAIController>(APACS_SimpleNPCAIController::StaticClass());
		if (AIController)
		{
			AIController->Possess(this);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[NPC MOVE] Failed to spawn AI controller"));
			return;
		}
	}

	// Use proper MoveTo with completion callback instead of SimpleMoveToLocation
	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalLocation(TargetLocation);
	MoveRequest.SetAcceptanceRadius(75.0f);

	// Request movement and bind completion delegate
	FPathFollowingRequestResult RequestResult = AIController->MoveTo(MoveRequest);
	if (RequestResult.Code == EPathFollowingRequestResult::RequestSuccessful)
	{
		// TODO: Bind movement completion when Epic pattern is researched

		// Wake from dormancy when starting movement
		FlushNetDormancy();

		// Track movement state
		bIsMoving = true;
		LastMovementTime = GetWorld()->GetTimeSeconds();

		UE_LOG(LogTemp, Verbose, TEXT("[NPC MOVE] %s started move to %s (Distance: %f)"),
			*GetName(), *TargetLocation.ToString(), Distance);
	}
	else
	{
		// Movement request failed, disable CMC tick
		if (UCharacterMovementComponent* CMC = GetCharacterMovement())
		{
			CMC->SetComponentTickEnabled(false);
		}

		UE_LOG(LogTemp, Warning, TEXT("[NPC MOVE] Movement request failed for %s"), *GetName());
	}
}

// IPACS_SelectableCharacterInterface Implementation
void APACS_NPCCharacter::MoveToLocation(const FVector& TargetLocation)
{
	ServerMoveToLocation(TargetLocation);
}