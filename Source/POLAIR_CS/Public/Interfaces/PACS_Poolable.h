#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PACS_Poolable.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UPACS_Poolable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for actors that can be pooled
 * Provides lifecycle hooks for pool acquisition and release
 */
class POLAIR_CS_API IPACS_Poolable
{
	GENERATED_BODY()

public:
	/**
	 * Called when actor is acquired from the pool and about to be used
	 * Implement to reset actor state for gameplay use
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pool System")
	void OnAcquiredFromPool();

	/**
	 * Called when actor is returned to the pool
	 * Implement to clean up actor state before storage
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pool System")
	void OnReturnedToPool();
};