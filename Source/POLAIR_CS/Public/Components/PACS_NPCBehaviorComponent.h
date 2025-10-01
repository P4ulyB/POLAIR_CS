#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/PACS_Poolable.h"
#include "Data/PACS_InputTypes.h"
#include "PACS_NPCBehaviorComponent.generated.h"

class UPACS_InputHandlerComponent;
class APACS_PlayerController;
class APACS_PlayerState;

/**
 * NPC Behavior Component with integrated input handling
 *
 * Handles:
 * - Right-click movement commands for selected NPCs
 * - Animation montage playback (future)
 * - Pool-safe cleanup and reset
 *
 * Integrates with PACS Input System via IPACS_InputReceiver interface
 * Server-authoritative with client prediction support
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class POLAIR_CS_API UPACS_NPCBehaviorComponent : public UActorComponent,
	public IPACS_Poolable,
	public IPACS_InputReceiver
{
	GENERATED_BODY()

public:
	UPACS_NPCBehaviorComponent();

	// ========================================
	// IPACS_InputReceiver Interface
	// ========================================
	virtual EPACS_InputHandleResult HandleInputAction(FName ActionName, const FInputActionValue& Value) override;
	virtual int32 GetInputPriority() const override { return 350; } // Below Gameplay (400)

	// ========================================
	// Movement Commands
	// ========================================

	// Request NPC to move to location (client callable)
	UFUNCTION(BlueprintCallable, Category = "NPC Behavior")
	void RequestNPCMove(AActor* NPC, const FVector& TargetLocation);

	// Request NPC to stop movement (client callable)
	UFUNCTION(BlueprintCallable, Category = "NPC Behavior")
	void RequestNPCStop(AActor* NPC);

	// ========================================
	// Animation Commands (Future)
	// ========================================

	// Play animation montage on NPC (client callable)
	UFUNCTION(BlueprintCallable, Category = "NPC Behavior", meta = (DisplayName = "Request Play Montage"))
	void RequestPlayMontage(AActor* NPC, class UAnimMontage* Montage, float PlayRate = 1.0f);

	// ========================================
	// Pool Management
	// ========================================

	// Remove NPC from level (return to pool)
	UFUNCTION(BlueprintCallable, Category = "NPC Behavior")
	void RequestRemoveFromLevel(AActor* NPC);

	// IPACS_Poolable interface
	virtual void OnAcquiredFromPool_Implementation() override;
	virtual void OnReturnedToPool_Implementation() override;

	// ========================================
	// Selection Tracking (Client-side)
	// ========================================

	// Update locally tracked selection (called by PlayerController when selection changes)
	UFUNCTION(BlueprintCallable, Category = "Selection")
	void SetLocallySelectedNPC(AActor* NPC);

	// Clear local selection (e.g., when NPC is deselected or destroyed)
	UFUNCTION(BlueprintCallable, Category = "Selection")
	void ClearLocalSelection();

protected:
	// Component lifecycle
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ========================================
	// Server RPCs
	// ========================================

	UFUNCTION(Server, Reliable)
	void ServerRequestNPCMove(AActor* NPC, FVector_NetQuantize TargetLocation);

	UFUNCTION(Server, Reliable)
	void ServerRequestNPCStop(AActor* NPC);

	UFUNCTION(Server, Reliable)
	void ServerRequestPlayMontage(AActor* NPC, class UAnimMontage* Montage, float PlayRate);

	UFUNCTION(Server, Reliable)
	void ServerRequestRemoveFromLevel(AActor* NPC);

	// ========================================
	// Multicast RPCs
	// ========================================

	// Notify all clients to play montage (unreliable for animations)
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayMontage(AActor* NPC, class UAnimMontage* Montage, float PlayRate);

private:
	// ========================================
	// Input Handling
	// ========================================

	// Handle right-click for movement commands
	EPACS_InputHandleResult HandleRightClick(const FInputActionValue& Value);

	// Handle context menu or other actions (future)
	EPACS_InputHandleResult HandleContextAction(FName ActionName, const FInputActionValue& Value);

	// ========================================
	// Helper Methods
	// ========================================

	// Get the currently selected NPC for this player
	AActor* GetSelectedNPC() const;

	// Check if an actor is a valid NPC that can be commanded
	bool IsValidCommandTarget(AActor* Actor) const;

	// Validate if location is valid for NPC movement
	bool IsValidMoveLocation(const FVector& Location, const FHitResult& HitResult) const;

	// Command NPC to move (server-side execution)
	void ExecuteNPCMove(AActor* NPC, const FVector& TargetLocation);

	// Command NPC to stop (server-side execution)
	void ExecuteNPCStop(AActor* NPC);

	// ========================================
	// Cached References
	// ========================================

	// Input handler component (for registration/unregistration)
	UPROPERTY()
	TObjectPtr<UPACS_InputHandlerComponent> InputHandler;

	// Owning player controller
	UPROPERTY()
	TObjectPtr<APACS_PlayerController> OwningController;

	// ========================================
	// Configuration
	// ========================================

	// Show debug visualization for movement commands
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bShowDebugVisualization = false;

	// Duration to show movement target debug sphere
	UPROPERTY(EditAnywhere, Category = "Debug", meta = (EditCondition = "bShowDebugVisualization"))
	float DebugVisualizationDuration = 2.0f;

	// ========================================
	// State
	// ========================================

	// Track if we're registered with input handler
	bool bIsRegisteredWithInput = false;

	// Last movement command time (for rate limiting)
	float LastMoveCommandTime = 0.0f;

	// Minimum time between movement commands (prevents spam)
	static constexpr float MoveCommandCooldown = 0.1f;

	// Client-side tracking of what this player has selected
	// Using TWeakObjectPtr for automatic null handling if NPC is destroyed
	TWeakObjectPtr<AActor> LocallySelectedNPC;
};