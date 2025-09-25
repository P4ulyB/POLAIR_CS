#include "Core/PACS_SimpleNPCAIController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

APACS_SimpleNPCAIController::APACS_SimpleNPCAIController()
{
	// Simple setup - no fancy optimization
	PrimaryActorTick.bCanEverTick = false;
}

void APACS_SimpleNPCAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	// Server only
	if (!HasAuthority())
	{
		return;
	}

	// Dead simple stop - just stop the movement
	if (ACharacter* ControlledCharacter = GetCharacter())
	{
		if (UCharacterMovementComponent* MovementComp = ControlledCharacter->GetCharacterMovement())
		{
			// Stop all movement
			MovementComp->StopMovementImmediately();
			MovementComp->Velocity = FVector::ZeroVector;

			UE_LOG(LogTemp, Log, TEXT("[Simple AI] Movement stopped for %s"), *ControlledCharacter->GetName());
		}
	}
}