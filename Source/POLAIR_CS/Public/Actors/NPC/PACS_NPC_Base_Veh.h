#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "Interfaces/PACS_Poolable.h"
#include "PACS_NPC_Base_Veh.generated.h"

class UBoxComponent;
class UPACS_SelectionPlaneComponent;

/**
 * Base Vehicle class for vehicle NPCs in POLAIR_CS
 * Extends AWheeledVehiclePawn with pooling support and selection system
 * Selection visuals are client-side only, excluded from VR/HMD clients
 * Uses CustomPrimitiveData for per-actor visual customization
 */
UCLASS(Abstract)
class POLAIR_CS_API APACS_NPC_Base_Veh : public AWheeledVehiclePawn, public IPACS_Poolable
{
	GENERATED_BODY()

public:
	APACS_NPC_Base_Veh();

protected:
	// AActor interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// IPACS_Poolable interface
	virtual void OnAcquiredFromPool_Implementation() override;
	virtual void OnReturnedToPool_Implementation() override;

public:
	// Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> BoxCollision;

	// Selection plane component for visual indicators (manages state and client-side visuals)
	// This component handles creating visual elements ONLY on non-VR clients
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPACS_SelectionPlaneComponent> SelectionPlaneComponent;

protected:
	// Selection state
	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	bool bIsSelected = false;

	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	TWeakObjectPtr<class APlayerState> CurrentSelector;

	// Vehicle state for pooling
	UPROPERTY()
	bool bEngineStartedByDefault = true;

public:
	// Selection interface
	UFUNCTION(BlueprintCallable, Category = "Selection")
	virtual void SetSelected(bool bNewSelected, class APlayerState* Selector = nullptr);

	UFUNCTION(BlueprintPure, Category = "Selection")
	bool IsSelected() const { return bIsSelected; }

	UFUNCTION(BlueprintPure, Category = "Selection")
	class APlayerState* GetCurrentSelector() const { return CurrentSelector.Get(); }

	// Vehicle control
	UFUNCTION(BlueprintCallable, Category = "NPC Vehicle")
	virtual void DriveToLocation(const FVector& TargetLocation);

	UFUNCTION(BlueprintCallable, Category = "NPC Vehicle")
	virtual void StopVehicle();

	UFUNCTION(BlueprintCallable, Category = "NPC Vehicle")
	virtual void SetHandbrake(bool bEngaged);

protected:
	// Visual feedback
	virtual void UpdateSelectionVisuals();

	// Pool state management
	virtual void ResetForPool();
	virtual void PrepareForUse();

	// Vehicle-specific reset
	virtual void ResetVehicleState();
	virtual void ResetVehiclePhysics();

private:
	// Default component setup
	void SetupDefaultCollision();
};