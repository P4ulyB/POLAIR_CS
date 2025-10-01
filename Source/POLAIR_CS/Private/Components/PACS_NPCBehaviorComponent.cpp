#include "Components/PACS_NPCBehaviorComponent.h"
#include "Components/PACS_InputHandlerComponent.h"
#include "Core/PACS_PlayerController.h"
#include "Core/PACS_PlayerState.h"
#include "Interfaces/PACS_SelectableCharacterInterface.h"
#include "Subsystems/PACS_SpawnOrchestrator.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Net/UnrealNetwork.h"

UPACS_NPCBehaviorComponent::UPACS_NPCBehaviorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false); // Component itself doesn't replicate, uses RPCs
}

// ========================================
// Component Lifecycle
// ========================================

void UPACS_NPCBehaviorComponent::BeginPlay()
{
	Super::BeginPlay();

	// Only initialize on clients and listen servers (not dedicated server)
	if (GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// Component is now directly on PlayerController
	if (APACS_PlayerController* PC = Cast<APACS_PlayerController>(GetOwner()))
	{
		OwningController = PC;

		// Find and register with input handler
		if (UPACS_InputHandlerComponent* Handler = PC->FindComponentByClass<UPACS_InputHandlerComponent>())
		{
			InputHandler = Handler;
			Handler->RegisterReceiver(this, GetInputPriority());
			bIsRegisteredWithInput = true;

			UE_LOG(LogTemp, Log, TEXT("PACS_NPCBehaviorComponent: Registered with InputHandler (Priority: %d)"),
				GetInputPriority());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("PACS_NPCBehaviorComponent: No InputHandlerComponent found on PlayerController"));
		}
	}
}

void UPACS_NPCBehaviorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unregister from input handler
	if (bIsRegisteredWithInput && InputHandler)
	{
		InputHandler->UnregisterReceiver(this);
		bIsRegisteredWithInput = false;
	}

	InputHandler = nullptr;
	OwningController = nullptr;

	Super::EndPlay(EndPlayReason);
}

// ========================================
// IPACS_InputReceiver Interface
// ========================================

EPACS_InputHandleResult UPACS_NPCBehaviorComponent::HandleInputAction(FName ActionName, const FInputActionValue& Value)
{
	// Handle right-click for movement commands
	if (ActionName == TEXT("RightClick"))
	{
		return HandleRightClick(Value);
	}

	// Future: Handle other context actions
	// if (ActionName == TEXT("ContextMenu") || ActionName == TEXT("Delete"))
	// {
	//     return HandleContextAction(ActionName, Value);
	// }

	return EPACS_InputHandleResult::NotHandled;
}

// ========================================
// Input Handling
// ========================================

EPACS_InputHandleResult UPACS_NPCBehaviorComponent::HandleRightClick(const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Log, TEXT("PACS_NPCBehaviorComponent::HandleRightClick - Right-click detected"));

	if (!OwningController)
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_NPCBehaviorComponent::HandleRightClick - No owning controller"));
		return EPACS_InputHandleResult::NotHandled;
	}

	// Get currently selected NPCs
	TArray<AActor*> SelectedNPCs = GetSelectedNPCs();
	if (SelectedNPCs.Num() == 0)
	{
		// No NPCs selected, let other handlers process this
		UE_LOG(LogTemp, Log, TEXT("PACS_NPCBehaviorComponent::HandleRightClick - No NPCs selected, passing to other handlers"));
		return EPACS_InputHandleResult::NotHandled;
	}

	UE_LOG(LogTemp, Log, TEXT("PACS_NPCBehaviorComponent::HandleRightClick - %d NPCs selected"), SelectedNPCs.Num());

	// Rate limiting to prevent command spam
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastMoveCommandTime < MoveCommandCooldown)
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_NPCBehaviorComponent::HandleRightClick - Rate limited (cooldown: %.2f)"), MoveCommandCooldown);
		return EPACS_InputHandleResult::HandledConsume;
	}
	LastMoveCommandTime = CurrentTime;

	// Get hit result under cursor
	FHitResult HitResult;
	if (!OwningController->GetHitResultUnderCursor(ECC_Visibility, false, HitResult))
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_NPCBehaviorComponent::HandleRightClick - Failed to get hit result under cursor"));
		return EPACS_InputHandleResult::NotHandled;
	}

	UE_LOG(LogTemp, Log, TEXT("PACS_NPCBehaviorComponent::HandleRightClick - Hit location: %s, Hit actor: %s"),
		*HitResult.Location.ToString(),
		HitResult.GetActor() ? *HitResult.GetActor()->GetName() : TEXT("None"));

	// Check if clicking on valid move location (not another NPC)
	if (!IsValidMoveLocation(HitResult.Location, HitResult))
	{
		// Clicking on another NPC or invalid location - let selection system handle it
		UE_LOG(LogTemp, Log, TEXT("PACS_NPCBehaviorComponent::HandleRightClick - Invalid move location (clicked on another NPC?)"));
		return EPACS_InputHandleResult::NotHandled;
	}

	// Send movement command for all selected NPCs
	UE_LOG(LogTemp, Log, TEXT("PACS_NPCBehaviorComponent::HandleRightClick - Sending move commands for %d NPCs to %s"),
		SelectedNPCs.Num(), *HitResult.Location.ToString());

	// Use batch movement RPC for efficiency
	if (OwningController)
	{
		OwningController->ServerRequestMoveMultiple(SelectedNPCs, FVector_NetQuantize(HitResult.Location));
	}

	// Show debug visualization if enabled
	if (bShowDebugVisualization && GetWorld())
	{
		DrawDebugSphere(GetWorld(), HitResult.Location, 50.0f, 12, FColor::Green, false, DebugVisualizationDuration);

		// Draw lines from each selected NPC to the target
		for (AActor* NPC : SelectedNPCs)
		{
			if (NPC)
			{
				DrawDebugLine(GetWorld(), NPC->GetActorLocation(), HitResult.Location, FColor::Green, false, DebugVisualizationDuration);
			}
		}
	}

	// Consume the input so it doesn't propagate to lower priority handlers
	return EPACS_InputHandleResult::HandledConsume;
}

// ========================================
// Movement Commands
// ========================================

void UPACS_NPCBehaviorComponent::RequestNPCMove(AActor* NPC, const FVector& TargetLocation)
{
	if (!IsValidCommandTarget(NPC))
	{
		return;
	}

	if (GetOwner()->HasAuthority())
	{
		// Server: Execute directly
		ExecuteNPCMove(NPC, TargetLocation);
	}
	else
	{
		// Client: Send to server
		ServerRequestNPCMove(NPC, FVector_NetQuantize(TargetLocation));
	}
}

void UPACS_NPCBehaviorComponent::RequestNPCStop(AActor* NPC)
{
	if (!IsValidCommandTarget(NPC))
	{
		return;
	}

	if (GetOwner()->HasAuthority())
	{
		// Server: Execute directly
		ExecuteNPCStop(NPC);
	}
	else
	{
		// Client: Send to server
		ServerRequestNPCStop(NPC);
	}
}

void UPACS_NPCBehaviorComponent::ServerRequestNPCMove_Implementation(AActor* NPC, FVector_NetQuantize TargetLocation)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	// Validate the requesting player owns the selection
	if (APACS_PlayerState* PS = OwningController ? OwningController->GetPlayerState<APACS_PlayerState>() : nullptr)
	{
		// Check if this player has this NPC selected
		if (PS->GetSelectedActor() != NPC)
		{
			UE_LOG(LogTemp, Warning, TEXT("PACS_NPCBehaviorComponent: Player %s tried to command NPC %s they don't have selected"),
				*PS->GetPlayerName(), NPC ? *NPC->GetName() : TEXT("NULL"));
			return;
		}
	}

	ExecuteNPCMove(NPC, TargetLocation);
}

void UPACS_NPCBehaviorComponent::ServerRequestNPCStop_Implementation(AActor* NPC)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	// Validate ownership
	if (APACS_PlayerState* PS = OwningController ? OwningController->GetPlayerState<APACS_PlayerState>() : nullptr)
	{
		if (PS->GetSelectedActor() != NPC)
		{
			return;
		}
	}

	ExecuteNPCStop(NPC);
}

void UPACS_NPCBehaviorComponent::ExecuteNPCMove(AActor* NPC, const FVector& TargetLocation)
{
	if (!NPC || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_NPCBehaviorComponent::ExecuteNPCMove - Invalid NPC or not authority"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("PACS_NPCBehaviorComponent::ExecuteNPCMove - Server executing move for %s to %s"),
		*NPC->GetName(), *TargetLocation.ToString());

	// Use the IPACS_SelectableCharacterInterface to command movement
	if (IPACS_SelectableCharacterInterface* Selectable = Cast<IPACS_SelectableCharacterInterface>(NPC))
	{
		Selectable->MoveToLocation(TargetLocation);

		UE_LOG(LogTemp, Log, TEXT("PACS_NPCBehaviorComponent::ExecuteNPCMove - Movement command sent to NPC %s"),
			*NPC->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("PACS_NPCBehaviorComponent::ExecuteNPCMove - NPC %s doesn't implement IPACS_SelectableCharacterInterface"),
			*NPC->GetName());
	}
}

void UPACS_NPCBehaviorComponent::ExecuteNPCStop(AActor* NPC)
{
	if (!NPC || !GetOwner()->HasAuthority())
	{
		return;
	}

	// Stop movement via CharacterMovement if it's a character
	if (ACharacter* Character = Cast<ACharacter>(NPC))
	{
		if (UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement())
		{
			MovementComp->StopMovementImmediately();
		}
	}

	UE_LOG(LogTemp, Log, TEXT("PACS_NPCBehaviorComponent: Server stopped NPC %s movement"),
		*NPC->GetName());
}

// ========================================
// Animation Commands
// ========================================

void UPACS_NPCBehaviorComponent::RequestPlayMontage(AActor* NPC, UAnimMontage* Montage, float PlayRate)
{
	if (!IsValidCommandTarget(NPC) || !Montage)
	{
		return;
	}

	if (GetOwner()->HasAuthority())
	{
		// Server: Multicast to all clients
		MulticastPlayMontage(NPC, Montage, PlayRate);
	}
	else
	{
		// Client: Request server to multicast
		ServerRequestPlayMontage(NPC, Montage, PlayRate);
	}
}

void UPACS_NPCBehaviorComponent::ServerRequestPlayMontage_Implementation(AActor* NPC, UAnimMontage* Montage, float PlayRate)
{
	if (!GetOwner()->HasAuthority() || !Montage)
	{
		return;
	}

	// Validate ownership
	if (APACS_PlayerState* PS = OwningController ? OwningController->GetPlayerState<APACS_PlayerState>() : nullptr)
	{
		if (PS->GetSelectedActor() != NPC)
		{
			return;
		}
	}

	// Broadcast to all clients
	MulticastPlayMontage(NPC, Montage, PlayRate);
}

void UPACS_NPCBehaviorComponent::MulticastPlayMontage_Implementation(AActor* NPC, UAnimMontage* Montage, float PlayRate)
{
	if (!NPC || !Montage)
	{
		return;
	}

	// Play montage on character
	if (ACharacter* Character = Cast<ACharacter>(NPC))
	{
		if (UAnimInstance* AnimInstance = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
		{
			AnimInstance->Montage_Play(Montage, PlayRate);

			UE_LOG(LogTemp, Log, TEXT("PACS_NPCBehaviorComponent: Playing montage %s on NPC %s"),
				*Montage->GetName(), *NPC->GetName());
		}
	}
}

// ========================================
// Pool Management
// ========================================

void UPACS_NPCBehaviorComponent::RequestRemoveFromLevel(AActor* NPC)
{
	if (!IsValidCommandTarget(NPC))
	{
		return;
	}

	if (GetOwner()->HasAuthority())
	{
		// Server: Return to pool directly
		if (UWorld* World = GetWorld())
		{
			if (UPACS_SpawnOrchestrator* Orchestrator = World->GetSubsystem<UPACS_SpawnOrchestrator>())
			{
				Orchestrator->ReleaseActor(NPC);
				UE_LOG(LogTemp, Log, TEXT("PACS_NPCBehaviorComponent: Returned NPC %s to pool"),
					*NPC->GetName());
			}
		}
	}
	else
	{
		// Client: Request server to remove
		ServerRequestRemoveFromLevel(NPC);
	}
}

void UPACS_NPCBehaviorComponent::ServerRequestRemoveFromLevel_Implementation(AActor* NPC)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	// Validate ownership
	if (APACS_PlayerState* PS = OwningController ? OwningController->GetPlayerState<APACS_PlayerState>() : nullptr)
	{
		if (PS->GetSelectedActor() != NPC)
		{
			return;
		}
	}

	// Return to pool
	if (UWorld* World = GetWorld())
	{
		if (UPACS_SpawnOrchestrator* Orchestrator = World->GetSubsystem<UPACS_SpawnOrchestrator>())
		{
			// Clear selection first
			if (APACS_PlayerState* PS = OwningController->GetPlayerState<APACS_PlayerState>())
			{
				PS->SetSelectedActor(nullptr);
			}

			Orchestrator->ReleaseActor(NPC);
		}
	}
}

// ========================================
// IPACS_Poolable Interface
// ========================================

void UPACS_NPCBehaviorComponent::OnAcquiredFromPool_Implementation()
{
	// Reset state when acquired from pool
	LastMoveCommandTime = 0.0f;

	// Re-register with input if needed
	if (!bIsRegisteredWithInput && InputHandler)
	{
		InputHandler->RegisterReceiver(this, GetInputPriority());
		bIsRegisteredWithInput = true;
	}
}

void UPACS_NPCBehaviorComponent::OnReturnedToPool_Implementation()
{
	// Unregister from input when returning to pool
	if (bIsRegisteredWithInput && InputHandler)
	{
		InputHandler->UnregisterReceiver(this);
		bIsRegisteredWithInput = false;
	}

	// Clear references
	LastMoveCommandTime = 0.0f;

	// Clear local selections - important for memory management
	LocallySelectedNPCs.Empty();
}

// ========================================
// Helper Methods
// ========================================

AActor* UPACS_NPCBehaviorComponent::GetSelectedNPC() const
{
	// Return first selected NPC for backward compatibility
	if (LocallySelectedNPCs.Num() > 0)
	{
		AActor* SelectedActor = LocallySelectedNPCs[0].Get();
		if (SelectedActor)
		{
			UE_LOG(LogTemp, VeryVerbose, TEXT("PACS_NPCBehaviorComponent::GetSelectedNPC - Returning first selected NPC: %s"),
				*SelectedActor->GetName());
			return SelectedActor;
		}
	}

	return nullptr;
}

TArray<AActor*> UPACS_NPCBehaviorComponent::GetSelectedNPCs() const
{
	TArray<AActor*> Result;
	for (const TWeakObjectPtr<AActor>& WeakActor : LocallySelectedNPCs)
	{
		if (AActor* Actor = WeakActor.Get())
		{
			Result.Add(Actor);
		}
	}
	return Result;
}

void UPACS_NPCBehaviorComponent::SetLocallySelectedNPC(AActor* NPC)
{
	// Clear previous selection and set single NPC
	LocallySelectedNPCs.Empty();
	if (NPC)
	{
		LocallySelectedNPCs.Add(NPC);
	}

	UE_LOG(LogTemp, Log, TEXT("PACS_NPCBehaviorComponent::SetLocallySelectedNPC - Set single NPC: %s"),
		NPC ? *NPC->GetName() : TEXT("None"));
}

void UPACS_NPCBehaviorComponent::SetLocallySelectedNPCs(const TArray<AActor*>& NPCs)
{
	// Clear previous selections and set new ones
	LocallySelectedNPCs.Empty();
	for (AActor* NPC : NPCs)
	{
		if (NPC)
		{
			LocallySelectedNPCs.Add(NPC);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("PACS_NPCBehaviorComponent::SetLocallySelectedNPCs - Set %d NPCs"),
		LocallySelectedNPCs.Num());
}

void UPACS_NPCBehaviorComponent::ClearLocalSelection()
{
	int32 PreviousCount = LocallySelectedNPCs.Num();
	LocallySelectedNPCs.Empty();

	UE_LOG(LogTemp, Log, TEXT("PACS_NPCBehaviorComponent::ClearLocalSelection - Cleared %d selections"),
		PreviousCount);
}

bool UPACS_NPCBehaviorComponent::IsValidCommandTarget(AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}

	// Check if actor implements selectable interface
	if (!Actor->Implements<UPACS_SelectableCharacterInterface>())
	{
		return false;
	}

	// Check if actor is valid and not pending kill
	if (!IsValid(Actor))
	{
		return false;
	}

	return true;
}

bool UPACS_NPCBehaviorComponent::IsValidMoveLocation(const FVector& Location, const FHitResult& HitResult) const
{
	// Check if we hit another NPC (invalid move target)
	if (AActor* HitActor = HitResult.GetActor())
	{
		// If the hit actor is a selectable NPC, it's not a valid move location
		if (HitActor->Implements<UPACS_SelectableCharacterInterface>())
		{
			return false;
		}
	}

	// Could add additional validation here:
	// - Check if location is within play area bounds
	// - Check if location is on navigable mesh
	// - Check if location is not in restricted zone

	return true;
}