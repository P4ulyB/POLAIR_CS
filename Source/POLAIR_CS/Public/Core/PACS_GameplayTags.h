#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/**
 * Singleton containing native Gameplay Tags for POLAIR_CS spawn system
 * Provides compile-time safe tag references and editor integration
 */
struct POLAIR_CS_API FPACS_GameplayTags
{
public:
	// Singleton accessor
	static const FPACS_GameplayTags& Get() { return GameplayTags; }

	// Call this from module startup to register tags
	static void InitializeNativeTags();

	// Root tags
	FGameplayTag Spawn;

	// Category roots
	FGameplayTag Spawn_Human;
	FGameplayTag Spawn_Vehicle;
	FGameplayTag Spawn_Environment;
	FGameplayTag Spawn_Reserved;

	// Human spawn types
	FGameplayTag Spawn_Human_Police;
	FGameplayTag Spawn_Human_QAS;
	FGameplayTag Spawn_Human_QFRS;
	FGameplayTag Spawn_Human_Civ;
	FGameplayTag Spawn_Human_POI;

	// Vehicle spawn types
	FGameplayTag Spawn_Vehicle_Police;
	FGameplayTag Spawn_Vehicle_QAS;
	FGameplayTag Spawn_Vehicle_QFRS;
	FGameplayTag Spawn_Vehicle_Civ;
	FGameplayTag Spawn_Vehicle_VOI;

	// Environment spawn types (for Niagara effects)
	FGameplayTag Spawn_Environment_Fire;
	FGameplayTag Spawn_Environment_Smoke;

	// Reserved for future use (Res.1-5)
	FGameplayTag Spawn_Reserved_1;
	FGameplayTag Spawn_Reserved_2;
	FGameplayTag Spawn_Reserved_3;
	FGameplayTag Spawn_Reserved_4;
	FGameplayTag Spawn_Reserved_5;

private:
	static FPACS_GameplayTags GameplayTags;

	// Internal registration helper
	static void RegisterTag(FGameplayTag& OutTag, const FName& TagName, const FString& TagComment);
};