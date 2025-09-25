#include "Core/PACS_NPCAIController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"

APACS_NPCAIController::APACS_NPCAIController()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// Epic pattern: Configure AI controller for optimal performance
	SetActorTickEnabled(false);
}

void APACS_NPCAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	// Epic pattern: Authority check first for all state changes
	if (!HasAuthority())
	{
		return;
	}

	// Epic pattern: Movement completion logging
	UE_LOG(LogTemp, Log, TEXT("[NPC AI] Movement completed for %s with result: %s"),
		GetPawn() ? *GetPawn()->GetName() : TEXT("NULL"),
		*Result.ToString());

	// Simple stop - just zero the velocity and stop movement
	if (ACharacter* ControlledCharacter = GetCharacter())
	{
		if (UCharacterMovementComponent* MovComp = ControlledCharacter->GetCharacterMovement())
		{
			// Dead simple stopping
			MovComp->StopMovementImmediately();
			MovComp->Velocity = FVector::ZeroVector;

			UE_LOG(LogTemp, Verbose, TEXT("[NPC AI] Stopped movement for %s"),
				*ControlledCharacter->GetName());
		}
	}

	// Update movement tracking
	bIsCurrentlyMoving = false;

	// Epic pattern: Stop pathfinding component if still active
	if (UPathFollowingComponent* PathComp = GetPathFollowingComponent())
	{
		// Use proper Epic stopping method for PathFollowingComponent
		PathComp->AbortMove(*this, FPathFollowingResultFlags::MovementStop);
	}
}

void APACS_NPCAIController::ServerMoveToLocation_Implementation(const FVector& Destination)
{
	// Epic pattern: Server authority check
	if (!HasAuthority())
	{
		return;
	}

	// Epic pattern: Validate destination is reasonable
	if (GetPawn())
	{
		float Distance = FVector::Dist(GetPawn()->GetActorLocation(), Destination);
		if (Distance > 10000.0f) // 100 meters max
		{
			UE_LOG(LogTemp, Warning, TEXT("[NPC AI] Destination too far: %f units"), Distance);
			return;
		}
	}

	// Stop any existing movement before starting new movement
	ServerStopMovement();

	// Track movement state
	bIsCurrentlyMoving = true;

	// Use Epic's movement system
	EPathFollowingRequestResult::Type MoveResult = MoveToLocation(Destination);

	if (MoveResult == EPathFollowingRequestResult::Failed)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NPC AI] Failed to start movement to location"));
		bIsCurrentlyMoving = false;
	}
	else
	{
		UE_LOG(LogTemp, Verbose, TEXT("[NPC AI] Started movement to %s"), *Destination.ToString());
	}
}

void APACS_NPCAIController::ServerStopMovement_Implementation()
{
	// Epic pattern: Authority check
	if (!HasAuthority())
	{
		return;
	}

	// Epic pattern: Stop all movement systems
	StopMovement();

	if (ACharacter* ControlledCharacter = GetCharacter())
	{
		if (UCharacterMovementComponent* MovComp = ControlledCharacter->GetCharacterMovement())
		{
			MovComp->StopMovementImmediately();
			MovComp->Velocity = FVector::ZeroVector;
		}
	}

	bIsCurrentlyMoving = false;

	UE_LOG(LogTemp, Verbose, TEXT("[NPC AI] Movement stopped on server"));
}