#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "PACS_NPCAIController.generated.h"

UCLASS(BlueprintType)
class POLAIR_CS_API APACS_NPCAIController : public AAIController
{
	GENERATED_BODY()

public:
	APACS_NPCAIController();

protected:
	// Epic pattern: Override movement completion to properly stop NPCs
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

public:
	// Server-authoritative movement control
	UFUNCTION(Server, Reliable)
	void ServerMoveToLocation(const FVector& Destination);

	UFUNCTION(Server, Reliable)
	void ServerStopMovement();

private:
	// Track movement state for optimization
	UPROPERTY()
	bool bIsCurrentlyMoving = false;
};