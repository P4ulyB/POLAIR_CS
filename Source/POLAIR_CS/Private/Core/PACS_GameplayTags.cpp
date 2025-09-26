#include "Core/PACS_GameplayTags.h"
#include "GameplayTagsManager.h"

// Static instance
FPACS_GameplayTags FPACS_GameplayTags::GameplayTags;

void FPACS_GameplayTags::InitializeNativeTags()
{
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

	// Root tag
	RegisterTag(GameplayTags.Spawn, "PACS.Spawn", "Root tag for all spawnable types");

	// Category roots
	RegisterTag(GameplayTags.Spawn_Human, "PACS.Spawn.Human", "Human NPCs category");
	RegisterTag(GameplayTags.Spawn_Vehicle, "PACS.Spawn.Vehicle", "Vehicle category");
	RegisterTag(GameplayTags.Spawn_Environment, "PACS.Spawn.Environment", "Environmental effects category");
	RegisterTag(GameplayTags.Spawn_Reserved, "PACS.Spawn.Reserved", "Reserved for future spawnable types");

	// Human spawn types
	RegisterTag(GameplayTags.Spawn_Human_Police, "PACS.Spawn.Human.Police", "Police officer NPC");
	RegisterTag(GameplayTags.Spawn_Human_QAS, "PACS.Spawn.Human.QAS", "Queensland Ambulance Service NPC");
	RegisterTag(GameplayTags.Spawn_Human_QFRS, "PACS.Spawn.Human.QFRS", "Queensland Fire and Rescue Service NPC");
	RegisterTag(GameplayTags.Spawn_Human_Civ, "PACS.Spawn.Human.Civ", "Civilian NPC");
	RegisterTag(GameplayTags.Spawn_Human_POI, "PACS.Spawn.Human.POI", "Person of Interest NPC");

	// Vehicle spawn types
	RegisterTag(GameplayTags.Spawn_Vehicle_Police, "PACS.Spawn.Vehicle.Police", "Police vehicle");
	RegisterTag(GameplayTags.Spawn_Vehicle_QAS, "PACS.Spawn.Vehicle.QAS", "Ambulance vehicle");
	RegisterTag(GameplayTags.Spawn_Vehicle_QFRS, "PACS.Spawn.Vehicle.QFRS", "Fire truck");
	RegisterTag(GameplayTags.Spawn_Vehicle_Civ, "PACS.Spawn.Vehicle.Civ", "Civilian vehicle");
	RegisterTag(GameplayTags.Spawn_Vehicle_VOI, "PACS.Spawn.Vehicle.VOI", "Vehicle of Interest");

	// Environment spawn types (for Niagara effects)
	RegisterTag(GameplayTags.Spawn_Environment_Fire, "PACS.Spawn.Environment.Fire", "Fire effect");
	RegisterTag(GameplayTags.Spawn_Environment_Smoke, "PACS.Spawn.Environment.Smoke", "Smoke effect");

	// Reserved slots for future use
	RegisterTag(GameplayTags.Spawn_Reserved_1, "PACS.Spawn.Reserved.1", "Reserved slot 1");
	RegisterTag(GameplayTags.Spawn_Reserved_2, "PACS.Spawn.Reserved.2", "Reserved slot 2");
	RegisterTag(GameplayTags.Spawn_Reserved_3, "PACS.Spawn.Reserved.3", "Reserved slot 3");
	RegisterTag(GameplayTags.Spawn_Reserved_4, "PACS.Spawn.Reserved.4", "Reserved slot 4");
	RegisterTag(GameplayTags.Spawn_Reserved_5, "PACS.Spawn.Reserved.5", "Reserved slot 5");
}

void FPACS_GameplayTags::RegisterTag(FGameplayTag& OutTag, const FName& TagName, const FString& TagComment)
{
	OutTag = UGameplayTagsManager::Get().AddNativeGameplayTag(TagName, TagComment);
}