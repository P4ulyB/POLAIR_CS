#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

/**
 * PACS Collision Channel Definitions
 * Maps custom collision channels to UE5 GameTraceChannels for type safety
 *
 * Usage:
 *   static_cast<ECollisionChannel>(EPACS_CollisionChannel::Selection)
 *
 * Configuration:
 *   See Config/DefaultEngine.ini [/Script/Engine.CollisionProfile] section
 */
UENUM(BlueprintType)
enum class EPACS_CollisionChannel : uint8
{
	// Default/None entry required by UE5 reflection system
	None = 0 UMETA(Hidden),

	// Selection system collision channel - Query Only for hover detection and selection
	// Maps to ECC_GameTraceChannel1 as configured in DefaultEngine.ini
	Selection = ECC_GameTraceChannel1 UMETA(DisplayName="Selection"),
};

/**
 * PACS Collision Profile Names
 * Centralized string constants for collision profiles defined in DefaultEngine.ini
 */
namespace PACS_CollisionProfiles
{
	// Profile for objects that only respond to Selection channel queries
	// Ignores all other collision, uses QueryOnly collision enabled
	static const FName SelectionProfile(TEXT("SelectionProfile"));
}