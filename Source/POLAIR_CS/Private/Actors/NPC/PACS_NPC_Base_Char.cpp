#include "Actors/NPC/PACS_NPC_Base_Char.h"
#include "Components/PACS_SelectionPlaneComponent.h"
#include "Data/PACS_SelectionProfile.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
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
	// This ensures CPD values are set before the material is applied
	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->ApplyCachedColorValues(
			CachedProfileData.AvailableColour, CachedProfileData.AvailableBrightness,
			CachedProfileData.HoveredColour, CachedProfileData.HoveredBrightness,
			CachedProfileData.SelectedColour, CachedProfileData.SelectedBrightness,
			CachedProfileData.UnavailableColour, CachedProfileData.UnavailableBrightness
		);
		UE_LOG(LogTemp, Warning, TEXT("ApplyCachedProfileData: Applied cached color values to SelectionPlaneComponent BEFORE material"));
	}

	// Apply selection plane settings from cached data (client-side)
	if (SelectionPlaneComponent && SelectionPlaneComponent->GetSelectionPlane())
	{
		UStaticMeshComponent* SelectionPlane = SelectionPlaneComponent->GetSelectionPlane();
		UE_LOG(LogTemp, Warning, TEXT("ApplyCachedProfileData: SelectionPlane component exists for %s"), *GetName());

		// Apply mesh
		if (!CachedProfileData.SelectionStaticMesh.IsNull())
		{
			UE_LOG(LogTemp, Warning, TEXT("  -> SelectionStaticMesh path: %s"), *CachedProfileData.SelectionStaticMesh.ToString());
			if (UStaticMesh* PlaneMesh = CachedProfileData.SelectionStaticMesh.LoadSynchronous())
			{
				SelectionPlane->SetStaticMesh(PlaneMesh);
				SelectionPlane->SetRelativeTransform(CachedProfileData.SelectionStaticMeshTransform);
				UE_LOG(LogTemp, Warning, TEXT("  -> Applied mesh: %s"), *PlaneMesh->GetName());
				UE_LOG(LogTemp, Warning, TEXT("  -> Transform: %s"), *CachedProfileData.SelectionStaticMeshTransform.ToString());
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("  -> FAILED to load SelectionStaticMesh!"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("  -> SelectionStaticMesh is NULL in cached data!"));
		}

		// Apply material
		if (!CachedProfileData.SelectionMaterialInstance.IsNull())
		{
			UE_LOG(LogTemp, Warning, TEXT("  -> SelectionMaterial path: %s"), *CachedProfileData.SelectionMaterialInstance.ToString());
			if (UMaterialInterface* Material = CachedProfileData.SelectionMaterialInstance.LoadSynchronous())
			{
				// Check CPD before material application
				UE_LOG(LogTemp, Warning, TEXT("  -> BEFORE SetMaterial - checking CPD values:"));
				const FCustomPrimitiveData& CPDBefore = SelectionPlane->GetCustomPrimitiveData();
				for (int32 i = 0; i < FMath::Min(6, CPDBefore.Data.Num()); i++)
				{
					UE_LOG(LogTemp, Warning, TEXT("    CPD[%d] = %.2f"), i, CPDBefore.Data[i]);
				}

				SelectionPlane->SetMaterial(0, Material);
				UE_LOG(LogTemp, Warning, TEXT("  -> Applied material: %s"), *Material->GetName());

				// Check CPD after material application
				UE_LOG(LogTemp, Warning, TEXT("  -> AFTER SetMaterial - checking CPD values:"));
				const FCustomPrimitiveData& CPDAfter = SelectionPlane->GetCustomPrimitiveData();
				for (int32 i = 0; i < FMath::Min(6, CPDAfter.Data.Num()); i++)
				{
					UE_LOG(LogTemp, Warning, TEXT("    CPD[%d] = %.2f"), i, CPDAfter.Data[i]);
				}

				// Force update the CPD again after material application to ensure it takes effect
				if (SelectionPlaneComponent)
				{
					SelectionPlaneComponent->UpdateSelectionPlaneCPD();
					UE_LOG(LogTemp, Warning, TEXT("  -> Forced CPD update after material application"));
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("  -> FAILED to load SelectionMaterial!"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("  -> SelectionMaterial is NULL in cached data!"));
		}

		// Apply collision - Always use SelectionTrace channel (ECC_GameTraceChannel1) for blocking
		// The object type is already set to SelectionObject in SetupSelectionPlane
		SelectionPlane->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block); // SelectionTrace channel
		UE_LOG(LogTemp, Warning, TEXT("  -> Set collision to block SelectionTrace channel (ECC_GameTraceChannel1)"));

		// Log current collision setup for debugging
		UE_LOG(LogTemp, Warning, TEXT("  -> Collision ObjectType: %d, ProfileName: %s"),
			(int32)SelectionPlane->GetCollisionObjectType(),
			*SelectionPlane->GetCollisionProfileName().ToString());

		// Selection plane is always visible - state controlled by material/CPD
		SelectionPlane->SetVisibility(true);
		UE_LOG(LogTemp, Warning, TEXT("  -> SelectionPlane visibility set to true"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ApplyCachedProfileData: SelectionPlane component is NULL for %s!"), *GetName());
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
		UE_LOG(LogTemp, Warning, TEXT("PACS_NPC_Base_Char::BeginPlay: Applying cached profile data on client for %s"), *GetName());
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

	// Notify selection component
	if (SelectionPlaneComponent)
	{
		SelectionPlaneComponent->OnAcquiredFromPool_Implementation();
	}
}

void APACS_NPC_Base_Char::OnReturnedToPool_Implementation()
{
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
		return;
	}

	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->MoveToLocation(TargetLocation, 5.0f, true, true, true, false);
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

