#include "Actors/NPC/PACS_NPC_Base_Char.h"
#include "Components/PACS_SelectionPlaneComponent.h"
#include "Data/PACS_SelectionProfile.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "Net/UnrealNetwork.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Components/SkeletalMeshComponent.h"

APACS_NPC_Base_Char::APACS_NPC_Base_Char()
{
	PrimaryActorTick.bCanEverTick = false;

	SelectionPlaneComponent = CreateDefaultSubobject<UPACS_SelectionPlaneComponent>(TEXT("SelectionPlaneComponent"));
	SelectionPlaneComponent->SetIsReplicated(true);

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->bOrientRotationToMovement = true;
		GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
		GetCharacterMovement()->MaxWalkSpeed = DefaultMaxWalkSpeed;
		GetCharacterMovement()->MinAnalogWalkSpeed = 20.0f;
		GetCharacterMovement()->BrakingDecelerationWalking = 2000.0f;
	}

	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(10.0f);
	SetMinNetUpdateFrequency(2.0f);

	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void APACS_NPC_Base_Char::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Replicate the cached profile data struct (atomic replication)
	DOREPLIFETIME(APACS_NPC_Base_Char, CachedProfileData);
	DOREPLIFETIME(APACS_NPC_Base_Char, CurrentSelector);
}

void APACS_NPC_Base_Char::OnRep_CachedProfileData()
{
	ApplyCachedProfileData();
}

void APACS_NPC_Base_Char::ApplyCachedProfileData()
{
	if (!CachedProfileData.IsValid())
	{
		return;
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}

	// Load and apply skeletal mesh
	if (!CachedProfileData.SkeletalMeshAsset.IsNull())
	{
		if (USkeletalMesh* LoadedMesh = CachedProfileData.SkeletalMeshAsset.LoadSynchronous())
		{
			MeshComp->SetSkeletalMesh(LoadedMesh);
		}
	}

	// Apply transform using ACharacter's BaseOffset system
	// CRITICAL: ACharacter overrides direct mesh transforms with BaseTranslationOffset/BaseRotationOffset
	BaseTranslationOffset = CachedProfileData.SkeletalMeshLocation;
	BaseRotationOffset = CachedProfileData.SkeletalMeshRotation.Quaternion();

	// Apply base offsets to mesh component with capsule height adjustment
	FVector AdjustedLocation = BaseTranslationOffset;
	if (GetCapsuleComponent())
	{
		const float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		AdjustedLocation.Z = BaseTranslationOffset.Z - CapsuleHalfHeight;
	}

	MeshComp->SetRelativeLocationAndRotation(AdjustedLocation, BaseRotationOffset);
	MeshComp->SetRelativeScale3D(CachedProfileData.SkeletalMeshScale);

	// Apply AnimInstance class if specified
	if (!CachedProfileData.AnimInstanceClass.IsNull())
	{
		if (TSubclassOf<UAnimInstance> AnimClass = CachedProfileData.AnimInstanceClass.LoadSynchronous())
		{
			MeshComp->SetAnimInstanceClass(AnimClass);
		}
	}

	// Apply cached color/brightness values to the SelectionPlaneComponent FIRST (before material)
	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->ApplyCachedColorValues(
			CachedProfileData.AvailableColour, CachedProfileData.AvailableBrightness,
			CachedProfileData.HoveredColour, CachedProfileData.HoveredBrightness,
			CachedProfileData.SelectedColour, CachedProfileData.SelectedBrightness,
			CachedProfileData.UnavailableColour, CachedProfileData.UnavailableBrightness
		);
	}

	// Apply selection plane settings from cached data (client-side)
	if (SelectionPlaneComponent && SelectionPlaneComponent->GetSelectionPlane())
	{
		UStaticMeshComponent* SelectionPlane = SelectionPlaneComponent->GetSelectionPlane();

		// Apply mesh
		if (!CachedProfileData.SelectionStaticMesh.IsNull())
		{
			if (UStaticMesh* PlaneMesh = CachedProfileData.SelectionStaticMesh.LoadSynchronous())
			{
				SelectionPlane->SetStaticMesh(PlaneMesh);
				SelectionPlane->SetRelativeTransform(CachedProfileData.SelectionStaticMeshTransform);
			}
		}

		// Apply material
		if (!CachedProfileData.SelectionMaterialInstance.IsNull())
		{
			if (UMaterialInterface* Material = CachedProfileData.SelectionMaterialInstance.LoadSynchronous())
			{
				SelectionPlane->SetMaterial(0, Material);

				// Force update the CPD again after material application to ensure it takes effect
				if (SelectionPlaneComponent)
				{
					SelectionPlaneComponent->UpdateSelectionPlaneCPD();
				}
			}
		}

		// Apply collision - Always use SelectionTrace channel (ECC_GameTraceChannel1) for blocking
		SelectionPlane->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);

		// Selection plane is always visible - state controlled by material/CPD
		SelectionPlane->SetVisibility(true);
	}
}

void APACS_NPC_Base_Char::BeginPlay()
{
	Super::BeginPlay();

	if (GetCharacterMovement())
	{
		DefaultMaxWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
	}

	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->InitializeSelectionPlane();
		// Visibility handled by component
	}

	// Apply cached profile data on clients (handles case where RepNotify doesn't fire on initial replication)
	if (!HasAuthority() && CachedProfileData.IsValid())
	{
		ApplyCachedProfileData();
	}
}

void APACS_NPC_Base_Char::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clear selection if selected
	if (bIsSelected)
	{
		SetSelected(false, nullptr);
	}

	// Stop any movement
	StopMovement();

	Super::EndPlay(EndPlayReason);
}

void APACS_NPC_Base_Char::OnAcquiredFromPool_Implementation()
{
	PrepareForUse();

	// Ensure AI Controller is spawned and possessed (critical for movement)
	if (HasAuthority())
	{
		if (!GetController())
		{
			// Spawn and possess AI controller for pooled NPCs
			if (AIControllerClass)
			{
				FActorSpawnParameters SpawnParams;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

				AAIController* NewController = GetWorld()->SpawnActor<AAIController>(AIControllerClass, SpawnParams);
				if (NewController)
				{
					NewController->Possess(this);
					UE_LOG(LogTemp, Log, TEXT("PACS_NPC_Base_Char::OnAcquiredFromPool - Spawned and possessed AI Controller for %s"), *GetName());
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("PACS_NPC_Base_Char::OnAcquiredFromPool - Failed to spawn AI Controller for %s"), *GetName());
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::OnAcquiredFromPool - No AIControllerClass set for %s"), *GetName());
			}
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("PACS_NPC_Base_Char::OnAcquiredFromPool - %s already has controller: %s"),
				*GetName(), *GetController()->GetName());
		}
	}

	// Notify selection component
	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->OnAcquiredFromPool_Implementation();
	}
}

void APACS_NPC_Base_Char::OnReturnedToPool_Implementation()
{
	// Properly unpossess and destroy AI controller when returning to pool
	if (HasAuthority())
	{
		if (AAIController* AIController = Cast<AAIController>(GetController()))
		{
			AIController->UnPossess();
			AIController->Destroy();
			UE_LOG(LogTemp, Log, TEXT("PACS_NPC_Base_Char::OnReturnedToPool - Unpossessed and destroyed AI Controller for %s"), *GetName());
		}
	}

	ResetForPool();

	// Notify selection component
	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->OnReturnedToPool_Implementation();
	}
}

void APACS_NPC_Base_Char::SetSelected(bool bNewSelected, APlayerState* Selector)
{
	if (bIsSelected == bNewSelected && CurrentSelector == Selector)
	{
		return;
	}

	bIsSelected = bNewSelected;
	CurrentSelector = bNewSelected ? Selector : nullptr;

	UpdateSelectionVisuals();

	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->SetSelectionState(bIsSelected ? ESelectionVisualState::Selected : ESelectionVisualState::Available);
	}
}

void APACS_NPC_Base_Char::MoveToLocation(const FVector& TargetLocation)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::MoveToLocation - Called on client for %s, ignoring"), *GetName());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("PACS_NPC_Base_Char::MoveToLocation - %s attempting to move to %s"),
		*GetName(), *TargetLocation.ToString());

	// Check if we have a controller
	AController* CurrentController = GetController();
	if (!CurrentController)
	{
		UE_LOG(LogTemp, Error, TEXT("PACS_NPC_Base_Char::MoveToLocation - %s has no controller! Cannot move."), *GetName());
		return;
	}

	// Cast to AI Controller
	AAIController* AIController = Cast<AAIController>(CurrentController);
	if (!AIController)
	{
		UE_LOG(LogTemp, Error, TEXT("PACS_NPC_Base_Char::MoveToLocation - %s controller is not an AIController (Type: %s)"),
			*GetName(), *CurrentController->GetClass()->GetName());
		return;
	}

	// Check navigation system availability
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		UE_LOG(LogTemp, Error, TEXT("PACS_NPC_Base_Char::MoveToLocation - No navigation system found in world!"));
		return;
	}

	// Attempt to move
	UE_LOG(LogTemp, Log, TEXT("PACS_NPC_Base_Char::MoveToLocation - Issuing move command for %s to location %s"),
		*GetName(), *TargetLocation.ToString());

	EPathFollowingRequestResult::Type MoveResult = AIController->MoveToLocation(TargetLocation, 5.0f, true, true, true, false);

	// Log the result
	switch (MoveResult)
	{
	case EPathFollowingRequestResult::Failed:
		UE_LOG(LogTemp, Error, TEXT("PACS_NPC_Base_Char::MoveToLocation - Move request FAILED for %s"), *GetName());
		break;
	case EPathFollowingRequestResult::AlreadyAtGoal:
		UE_LOG(LogTemp, Log, TEXT("PACS_NPC_Base_Char::MoveToLocation - %s is already at goal"), *GetName());
		break;
	case EPathFollowingRequestResult::RequestSuccessful:
		UE_LOG(LogTemp, Log, TEXT("PACS_NPC_Base_Char::MoveToLocation - Move request SUCCESSFUL for %s"), *GetName());
		break;
	default:
		UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::MoveToLocation - Unknown result for %s"), *GetName());
		break;
	}
}

void APACS_NPC_Base_Char::StopMovement()
{
	if (!HasAuthority())
	{
		return;
	}

	// Stop AI movement
	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->StopMovement();
	}

	// Stop character movement
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->StopMovementImmediately();
	}
}

bool APACS_NPC_Base_Char::IsMoving() const
{
	// Check if AI is currently following a path
	if (const AAIController* AIController = Cast<AAIController>(GetController()))
	{
		if (UPathFollowingComponent* PathFollowing = AIController->GetPathFollowingComponent())
		{
			return PathFollowing->HasValidPath();
		}
	}

	// Fallback: check velocity
	if (const UCharacterMovementComponent* MovementComp = GetCharacterMovement())
	{
		return !MovementComp->Velocity.IsNearlyZero(1.0f);
	}

	return false;
}

void APACS_NPC_Base_Char::SetLocalHover(bool bHovered)
{
	bIsLocallyHovered = bHovered;
	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->SetHoverState(bHovered);
	}
}

void APACS_NPC_Base_Char::OnRep_CurrentSelector()
{
	// Trigger visual update when selector changes on clients
	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->UpdateVisuals();
	}
}

void APACS_NPC_Base_Char::UpdateSelectionVisuals()
{
	// Visibility handled automatically by component
}

void APACS_NPC_Base_Char::ResetForPool()
{
	bIsSelected = false;
	CurrentSelector = nullptr;

	// Selection plane handled by component pooling

	StopMovement();
	ResetCharacterMovement();
	ResetCharacterAnimation();
	SetActorTransform(FTransform::Identity);

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->Velocity = FVector::ZeroVector;
		GetCharacterMovement()->StopMovementImmediately();
	}
}

void APACS_NPC_Base_Char::PrepareForUse()
{
	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = DefaultMaxWalkSpeed;
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}

	// Selection plane handled by component
}

void APACS_NPC_Base_Char::SetSelectionProfile(UPACS_SelectionProfileAsset* InProfile)
{
	if (!HasAuthority() || !InProfile)
	{
		return;
	}

	// Apply behavior configuration (server-only, CharacterMovement replicates MaxWalkSpeed)
	if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
	{
		float Speed = InProfile->BehaviorConfig.MovementSpeed;
		if (Speed > 0.0f)  // 0 = use class default
		{
			MovementComp->MaxWalkSpeed = Speed;
			DefaultMaxWalkSpeed = Speed;

			UE_LOG(LogTemp, Log, TEXT("PACS_NPC_Base_Char: Applied movement speed %.1f to %s"),
				Speed, *GetName());
		}
	}

	CachedProfileData.PopulateFromProfile(InProfile);
	ApplyCachedProfileData();

	// NOTE: Do NOT call ApplyProfileAsset here - colors are already applied via ApplyCachedColorValues
	// The data asset values flow through CachedProfileData -> ApplyCachedColorValues -> StateVisuals
	// Calling ApplyProfileAsset would override the colors we just set
}

void APACS_NPC_Base_Char::ResetCharacterMovement()
{
	if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
	{
		// Reset movement mode
		MovementComp->SetMovementMode(MOVE_Walking);

		// Clear movement state
		MovementComp->StopMovementImmediately();
		MovementComp->Velocity = FVector::ZeroVector;
		// Acceleration is protected, cleared by StopMovementImmediately

		// Reset movement properties
		MovementComp->MaxWalkSpeed = DefaultMaxWalkSpeed;
		MovementComp->bOrientRotationToMovement = true;
	}
}

void APACS_NPC_Base_Char::ResetCharacterAnimation()
{
	if (GetMesh() && GetMesh()->GetAnimInstance())
	{
		GetMesh()->GetAnimInstance()->StopAllMontages(0.0f);
		GetMesh()->GetAnimInstance()->ResetDynamics(ETeleportType::ResetPhysics);
	}
}

