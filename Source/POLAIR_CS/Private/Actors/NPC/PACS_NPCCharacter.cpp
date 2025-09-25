#include "Actors/NPC/PACS_NPCCharacter.h"
#include "Data/Configs/PACS_NPCConfig.h"
#include "Data/Settings/PACS_SelectionSystemSettings.h"
#include "Core/PACS_PlayerState.h"
#include "Core/PACS_SimpleNPCAIController.h"
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
#include "AIController.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Engine/World.h"

APACS_NPCCharacter::APACS_NPCCharacter()
{
	// Enable ticking for distance-based optimization
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f; // 10Hz tick rate for NPCs
	bReplicates = true;
	SetNetUpdateFrequency(10.0f);

	// Keep movement component enabled for simplicity - optimization can come later
	GetCharacterMovement()->SetComponentTickEnabled(true);

	// Reduce movement component overhead
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 270.0f, 0.0f); // Reduced rotation rate

	// Optimize movement settings for NPCs
	GetCharacterMovement()->bRunPhysicsWithNoController = false;
	GetCharacterMovement()->bForceMaxAccel = true; // Skip acceleration calculations
	GetCharacterMovement()->MaxSimulationIterations = 1; // Reduce physics iterations
	GetCharacterMovement()->bEnablePhysicsInteraction = false; // NPCs don't need physics interaction

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

	// Initialize movement state tracking
	bIsMoving = false;
	LastMovementTime = 0.0f;
	MovementTimeoutDuration = 2.0f; // 2 seconds without velocity change before stopping movement tick

#if UE_SERVER
	// Server collision optional - disable for performance
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CollisionDecal->SetVisibility(false);
#endif
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

	// Set initial optimizations based on distance
	UpdateDistanceBasedOptimizations();
}

void APACS_NPCCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update optimizations every tick (but tick is only 10Hz)
	UpdateDistanceBasedOptimizations();

	// Check for movement completion and disable movement tick when not moving
	CheckMovementCompletion(DeltaTime);
}

void APACS_NPCCharacter::CheckMovementCompletion(float DeltaTime)
{
	// Only check movement on server authority
	if (!HasAuthority())
	{
		return;
	}

	// If we're supposed to be moving, check if we've actually stopped
	if (bIsMoving)
	{
		float CurrentVelocitySquared = GetVelocity().SizeSquared();
		float CurrentTime = GetWorld()->GetTimeSeconds();

		// If velocity is very low (essentially stopped), update last movement time
		if (CurrentVelocitySquared < 25.0f) // 5 units per second threshold
		{
			// If this is the first time we've detected low velocity, record the time
			if (LastMovementTime == 0.0f || (CurrentTime - LastMovementTime) < MovementTimeoutDuration)
			{
				LastMovementTime = CurrentTime;
			}
			// If we've been stationary long enough, disable movement
			else if ((CurrentTime - LastMovementTime) >= MovementTimeoutDuration)
			{
				bIsMoving = false;
				if (GetCharacterMovement())
				{
					GetCharacterMovement()->SetComponentTickEnabled(false);
				}

				// FIXED: Stop AI movement and unpossess controller to prevent perpetual movement
				if (AController* CurrentController = GetController())
				{
					// Stop any active movement
					if (AAIController* AIController = Cast<AAIController>(CurrentController))
					{
						AIController->StopMovement();
					}

					// Unpossess the controller to prevent autonomous behavior
					CurrentController->UnPossess();
				}

				UE_LOG(LogTemp, Verbose, TEXT("[NPC MOVE] %s stopped moving, disabled movement and unpossessed controller"), *GetName());
			}
		}
		else
		{
			// Reset timer if we're still moving
			LastMovementTime = CurrentTime;
		}
	}
}

void APACS_NPCCharacter::UpdateDistanceBasedOptimizations()
{
	// Get local player location for distance check
	APlayerController* LocalPC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!LocalPC || !LocalPC->GetPawn())
	{
		return;
	}

	float DistanceToPlayer = FVector::Dist(GetActorLocation(), LocalPC->GetPawn()->GetActorLocation());

	// Configure mesh animation updates based on distance
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (DistanceToPlayer > 5000.0f) // 50 meters
		{
			// Far: Minimal updates
			MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
			MeshComp->bEnableUpdateRateOptimizations = true;

			// Set very low tick rate for distant characters
			SetActorTickInterval(1.0f); // Once per second

			// Disable movement component for stationary distant NPCs
			if (GetCharacterMovement())
			{
				GetCharacterMovement()->SetComponentTickEnabled(false);
			}

			// Reduce network update frequency
			SetNetUpdateFrequency(2.0f);

			// Completely disable mesh ticking if very far
			if (DistanceToPlayer > 10000.0f)
			{
				MeshComp->bNoSkeletonUpdate = true;
				MeshComp->SetComponentTickEnabled(false);
			}
		}
		else if (DistanceToPlayer > 2000.0f) // 20 meters
		{
			// Medium: Reduced updates
			MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
			MeshComp->bEnableUpdateRateOptimizations = true;
			MeshComp->bNoSkeletonUpdate = false;
			MeshComp->SetComponentTickEnabled(true);

			// Medium tick rate
			SetActorTickInterval(0.2f); // 5 times per second

			// Keep movement disabled unless moving
			if (GetCharacterMovement() && GetVelocity().SizeSquared() < 10.0f)
			{
				GetCharacterMovement()->SetComponentTickEnabled(false);
			}

			// Medium network frequency
			SetNetUpdateFrequency(5.0f);
		}
		else
		{
			// Close: Normal updates but still optimized
			MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
			MeshComp->bEnableUpdateRateOptimizations = true;
			MeshComp->bNoSkeletonUpdate = false;
			MeshComp->SetComponentTickEnabled(true);

			// Normal tick rate
			SetActorTickInterval(0.1f); // 10 times per second

			// Enable movement only when actually moving
			if (GetCharacterMovement())
			{
				bool bHasVelocity = GetVelocity().SizeSquared() > 10.0f;
				GetCharacterMovement()->SetComponentTickEnabled(bHasVelocity);
			}

			// Normal network frequency
			SetNetUpdateFrequency(10.0f);
		}
	}
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
	// Server authority check
	if (!HasAuthority())
	{
		return;
	}

	// Basic validation - check if target location is reasonable
	FVector CurrentLocation = GetActorLocation();
	float Distance = FVector::Dist(CurrentLocation, TargetLocation);

	// Reasonable distance check (prevent teleporting across map)
	if (Distance > 10000.0f) // 100 meters max
	{
		UE_LOG(LogTemp, Warning, TEXT("[NPC MOVE] Target location too far: %f units"), Distance);
		return;
	}

	// Simple AI controller possession
	if (!GetController())
	{
		// Use our simple AI controller that properly stops movement
		APACS_SimpleNPCAIController* NewAI = GetWorld()->SpawnActor<APACS_SimpleNPCAIController>(APACS_SimpleNPCAIController::StaticClass());
		if (NewAI)
		{
			NewAI->Possess(this);
			UE_LOG(LogTemp, Verbose, TEXT("[NPC MOVE] Spawned simple AI controller for %s"), *GetName());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[NPC MOVE] Failed to spawn AI controller for %s"), *GetName());
			return;
		}
	}

	// Simple movement using Epic's SimpleMoveToLocation
	UAIBlueprintHelperLibrary::SimpleMoveToLocation(GetController(), TargetLocation);

	// Track movement state
	bIsMoving = true;
	LastMovementTime = GetWorld()->GetTimeSeconds();

	UE_LOG(LogTemp, Verbose, TEXT("[NPC MOVE] %s moving to location %s (Distance: %f)"),
		*GetName(), *TargetLocation.ToString(), Distance);
}