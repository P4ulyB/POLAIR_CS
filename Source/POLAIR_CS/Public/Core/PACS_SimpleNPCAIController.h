#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "PACS_SimpleNPCAIController.generated.h"

// Dead simple AI controller for NPC movement
UCLASS()
class POLAIR_CS_API APACS_SimpleNPCAIController : public AAIController
{
	GENERATED_BODY()

public:
	APACS_SimpleNPCAIController();

protected:
	// Override to stop movement when destination reached
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;
};