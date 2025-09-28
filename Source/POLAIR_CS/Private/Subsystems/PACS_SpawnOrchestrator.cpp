#include "Subsystems/PACS_SpawnOrchestrator.h"
#include "Interfaces/PACS_Poolable.h"
#include "Data/PACS_SpawnConfig.h"
#include "Data/PACS_SelectionProfile.h"
#include "Subsystems/PACS_MemoryTracker.h"
#include "Actors/NPC/PACS_NPC_Base.h"
#include "Actors/NPC/PACS_NPC_Base_Char.h"
#include "Actors/NPC/PACS_NPC_Base_Veh.h"
#include "Actors/NPC/PACS_NPC_Base_LW.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "Net/UnrealNetwork.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "Engine/AssetManager.h"

void UPACS_SpawnOrchestrator::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogTemp, Log, TEXT("PACS_SpawnOrchestrator: Initialized for World %s"),
		*GetWorld()->GetName());
}

void UPACS_SpawnOrchestrator::Deinitialize()
{
	// Clean up all pools on shutdown
	FlushAllPools();

	// Cancel any pending loads
	for (auto& LoadPair : LoadHandles)
	{
		if (LoadPair.Value.IsValid())
		{
			LoadPair.Value->CancelHandle();
		}
	}
	LoadHandles.Empty();

	Super::Deinitialize();
}

bool UPACS_SpawnOrchestrator::ShouldCreateSubsystem(UObject* Outer) const
{
	// Only create on server in networked games, or always in standalone
	UWorld* World = Cast<UWorld>(Outer);
	if (!World)
	{
		return false;
	}

	// Create for dedicated servers, listen servers, and standalone games
	return World->GetNetMode() != NM_Client;
}

AActor* UPACS_SpawnOrchestrator::AcquireActor(FGameplayTag SpawnTag, const FSpawnRequestParams& Params)
{
	// Server authority check
	if (!ensure(GetWorld()->GetAuthGameMode() != nullptr))
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnOrchestrator: AcquireActor called on non-authoritative context"));
		return nullptr;
	}

	// Validate tag
	if (!SpawnTag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnOrchestrator: Invalid spawn tag"));
		return nullptr;
	}

	// Check memory budget before acquiring
	if (UPACS_MemoryTracker* MemoryTracker = GetWorld()->GetSubsystem<UPACS_MemoryTracker>())
	{
		// Estimate memory for this actor type (use 1MB as default estimate)
		float EstimatedMemoryMB = 1.0f;
		if (!MemoryTracker->CanAllocateMemoryMB(EstimatedMemoryMB))
		{
			UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnOrchestrator: Memory budget exceeded, cannot acquire actor for tag %s"),
				*SpawnTag.ToString());
			MemoryTracker->CheckMemoryCompliance();
			return nullptr;
		}
	}

	// Initialize pool if needed
	if (!Pools.Contains(SpawnTag))
	{
		InitializePool(SpawnTag);
	}

	FPoolEntry* Pool = Pools.Find(SpawnTag);
	if (!Pool)
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnOrchestrator: Failed to initialize pool for tag %s"),
			*SpawnTag.ToString());
		return nullptr;
	}

	// If class is still loading, queue the request
	if (Pool->bIsLoading)
	{
		UE_LOG(LogTemp, Log, TEXT("PACS_SpawnOrchestrator: Class still loading for tag %s, queuing request"),
			*SpawnTag.ToString());

		// Queue request with callback
		Pool->PendingRequests.Add(TPair<FTransform, TFunction<void(AActor*)>>(
			Params.Transform,
			[this, Params](AActor* Actor) {
				if (Actor)
				{
					PrepareActorForUse(Actor, Params);
				}
			}
		));
		return nullptr;
	}

	// Check if class is loaded
	if (!Pool->ResolvedClass)
	{
		LoadActorClass(SpawnTag);
		return nullptr;
	}

	AActor* Actor = nullptr;

	// Try to get from available pool
	while (Pool->AvailableActors.Num() > 0 && !Actor)
	{
		TWeakObjectPtr<AActor> WeakActor = Pool->AvailableActors.Pop();
		if (WeakActor.IsValid())
		{
			Actor = WeakActor.Get();
			break;
		}
	}

	// Create new actor if needed and under max size
	if (!Actor && Pool->CurrentSize < Pool->MaxSize)
	{
		Actor = CreatePooledActor(SpawnTag, Pool->ResolvedClass);
		if (Actor)
		{
			Pool->CurrentSize++;
		}
	}

	// Prepare and activate actor
	if (Actor)
	{
		Pool->ActiveActors.Add(Actor);
		ActorToTagMap.Add(Actor, SpawnTag);
		PrepareActorForUse(Actor, Params);

		// Register with memory tracker
		if (UPACS_MemoryTracker* MemoryTracker = GetWorld()->GetSubsystem<UPACS_MemoryTracker>())
		{
			MemoryTracker->RegisterPooledActor(SpawnTag, Actor);
			MemoryTracker->MarkActorActive(SpawnTag, Actor, true);
		}

		UE_LOG(LogTemp, Verbose, TEXT("PACS_SpawnOrchestrator: Acquired actor %s for tag %s"),
			*Actor->GetName(), *SpawnTag.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnOrchestrator: Pool exhausted for tag %s"),
			*SpawnTag.ToString());
	}

	return Actor;
}

void UPACS_SpawnOrchestrator::ReleaseActor(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	// Server authority check
	if (!ensure(GetWorld()->GetAuthGameMode() != nullptr))
	{
		return;
	}

	// Find the tag for this actor
	FGameplayTag* TagPtr = ActorToTagMap.Find(Actor);
	if (!TagPtr)
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnOrchestrator: Attempting to release unmanaged actor %s"),
			*Actor->GetName());
		return;
	}

	FGameplayTag Tag = *TagPtr;
	FPoolEntry* Pool = Pools.Find(Tag);
	if (!Pool)
	{
		return;
	}

	// Move from active to available
	Pool->ActiveActors.RemoveSwap(Actor);

	// Update memory tracking - mark as inactive (pooled)
	if (UPACS_MemoryTracker* MemoryTracker = GetWorld()->GetSubsystem<UPACS_MemoryTracker>())
	{
		MemoryTracker->MarkActorActive(Tag, Actor, false);
	}

	ReturnActorToPool(Actor, Tag);

	UE_LOG(LogTemp, Verbose, TEXT("PACS_SpawnOrchestrator: Released actor %s to pool %s"),
		*Actor->GetName(), *Tag.ToString());
}

void UPACS_SpawnOrchestrator::PrewarmPool(FGameplayTag SpawnTag, int32 Count)
{
	if (!SpawnTag.IsValid() || Count <= 0)
	{
		return;
	}

	// Server authority check
	if (!ensure(GetWorld()->GetAuthGameMode() != nullptr))
	{
		return;
	}

	// Initialize pool if needed
	if (!Pools.Contains(SpawnTag))
	{
		InitializePool(SpawnTag);
	}

	FPoolEntry* Pool = Pools.Find(SpawnTag);
	if (!Pool || !Pool->ResolvedClass)
	{
		// Try loading the class
		LoadActorClass(SpawnTag);
		return;
	}

	// Create actors up to requested count
	int32 ActorsToCreate = FMath::Min(Count, Pool->MaxSize - Pool->CurrentSize);
	for (int32 i = 0; i < ActorsToCreate; i++)
	{
		AActor* NewActor = CreatePooledActor(SpawnTag, Pool->ResolvedClass);
		if (NewActor)
		{
			Pool->CurrentSize++;
			ReturnActorToPool(NewActor, SpawnTag);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("PACS_SpawnOrchestrator: Prewarmed %d actors for tag %s"),
		ActorsToCreate, *SpawnTag.ToString());
}

void UPACS_SpawnOrchestrator::FlushPool(FGameplayTag SpawnTag)
{
	FPoolEntry* Pool = Pools.Find(SpawnTag);
	if (!Pool)
	{
		return;
	}

	// Destroy all pooled actors
	for (auto& WeakActor : Pool->AvailableActors)
	{
		if (AActor* Actor = WeakActor.Get())
		{
			Actor->Destroy();
		}
	}

	// Note: We don't destroy active actors as they're in use
	// Just clear our tracking
	for (auto& WeakActor : Pool->ActiveActors)
	{
		if (AActor* Actor = WeakActor.Get())
		{
			ActorToTagMap.Remove(Actor);
		}
	}

	Pool->Reset();
	Pools.Remove(SpawnTag);
}

void UPACS_SpawnOrchestrator::FlushAllPools()
{
	TArray<FGameplayTag> TagsToFlush;
	Pools.GenerateKeyArray(TagsToFlush);

	for (const FGameplayTag& Tag : TagsToFlush)
	{
		FlushPool(Tag);
	}
}

void UPACS_SpawnOrchestrator::SetSpawnConfig(UPACS_SpawnConfig* InConfig)
{
	SpawnConfig = InConfig;

	// Pre-load all selection profiles on non-dedicated servers
	if (SpawnConfig && GetWorld() && GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		PreloadSelectionProfiles();
	}
}

void UPACS_SpawnOrchestrator::GetPoolStatistics(FGameplayTag SpawnTag, int32& OutActive, int32& OutAvailable, int32& OutTotal) const
{
	const FPoolEntry* Pool = Pools.Find(SpawnTag);
	if (Pool)
	{
		OutActive = Pool->ActiveActors.Num();
		OutAvailable = Pool->AvailableActors.Num();
		OutTotal = Pool->CurrentSize;
	}
	else
	{
		OutActive = 0;
		OutAvailable = 0;
		OutTotal = 0;
	}
}

void UPACS_SpawnOrchestrator::InitializePool(FGameplayTag SpawnTag)
{
	// Create pool entry
	FPoolEntry& NewPool = Pools.Add(SpawnTag);

	// Load configuration if available
	if (SpawnConfig)
	{
		FSpawnClassConfig Config;
		if (SpawnConfig->GetConfigForTag(SpawnTag, Config))
		{
			NewPool.InitialSize = Config.PoolSettings.InitialSize;
			NewPool.MaxSize = Config.PoolSettings.MaxSize;
			NewPool.ActorClass = Config.ActorClass;

			UE_LOG(LogTemp, Log, TEXT("PACS_SpawnOrchestrator: Initialized pool for tag %s (Initial: %d, Max: %d)"),
				*SpawnTag.ToString(), NewPool.InitialSize, NewPool.MaxSize);
		}
		else
		{
			// Default values if no config found
			NewPool.InitialSize = 5;
			NewPool.MaxSize = 20;

			UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnOrchestrator: No config for tag %s, using defaults"),
				*SpawnTag.ToString());
		}
	}
	else
	{
		// Default values if no config set
		NewPool.InitialSize = 5;
		NewPool.MaxSize = 20;

		UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnOrchestrator: No SpawnConfig set, using defaults for tag %s"),
			*SpawnTag.ToString());
	}

	// Start loading the class
	LoadActorClass(SpawnTag);
}

AActor* UPACS_SpawnOrchestrator::CreatePooledActor(FGameplayTag SpawnTag, TSubclassOf<AActor> ActorClass)
{
	if (!ActorClass)
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// Spawn with deferred initialization
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.bDeferConstruction = true;

	AActor* NewActor = World->SpawnActor<AActor>(ActorClass, FTransform::Identity, SpawnParams);
	if (!NewActor)
	{
		return nullptr;
	}

	// Finish spawning but keep inactive
	NewActor->FinishSpawning(FTransform::Identity);

	// Immediately reset for pool storage
	ResetActorForPool(NewActor);

	return NewActor;
}

void UPACS_SpawnOrchestrator::ReturnActorToPool(AActor* Actor, FGameplayTag SpawnTag)
{
	if (!Actor)
	{
		return;
	}

	// Reset the actor
	ResetActorForPool(Actor);

	// Call poolable interface if implemented
	if (Actor->GetClass()->ImplementsInterface(UPACS_Poolable::StaticClass()))
	{
		IPACS_Poolable::Execute_OnReturnedToPool(Actor);
	}

	// Add back to available pool
	FPoolEntry* Pool = Pools.Find(SpawnTag);
	if (Pool)
	{
		Pool->AvailableActors.Add(Actor);
	}

	// Remove from active tracking
	ActorToTagMap.Remove(Actor);
}

void UPACS_SpawnOrchestrator::ResetActorForPool(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	// Hide and disable
	Actor->SetActorHiddenInGame(true);
	Actor->SetActorEnableCollision(false);
	Actor->SetActorTickEnabled(false);

	// Reset location to prevent spatial query issues
	Actor->SetActorLocation(FVector(0, 0, -10000));

	// Clear relationships
	Actor->SetOwner(nullptr);
	Actor->SetInstigator(nullptr);

	// Reset physics if applicable
	if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
	{
		RootPrimitive->SetPhysicsLinearVelocity(FVector::ZeroVector);
		RootPrimitive->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}

	// Reset replication state
	ResetReplicationState(Actor);
}

void UPACS_SpawnOrchestrator::PrepareActorForUse(AActor* Actor, const FSpawnRequestParams& Params)
{
	if (!Actor)
	{
		UE_LOG(LogTemp, Error, TEXT("PACS_SpawnOrchestrator::PrepareActorForUse: Null actor provided"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnOrchestrator::PrepareActorForUse: Starting for %s on %s"),
		*Actor->GetName(),
		GetWorld()->GetNetMode() == NM_DedicatedServer ? TEXT("DedicatedServer") :
		GetWorld()->GetNetMode() == NM_ListenServer ? TEXT("ListenServer") :
		GetWorld()->GetNetMode() == NM_Client ? TEXT("Client") : TEXT("Standalone"));

	// Set transform
	Actor->SetActorTransform(Params.Transform);
	UE_LOG(LogTemp, Verbose, TEXT("PACS_SpawnOrchestrator::PrepareActorForUse: Set transform for %s"), *Actor->GetName());

	// Set ownership
	if (Params.Owner)
	{
		Actor->SetOwner(Params.Owner);
		UE_LOG(LogTemp, Verbose, TEXT("PACS_SpawnOrchestrator::PrepareActorForUse: Set owner for %s to %s"),
			*Actor->GetName(), *Params.Owner->GetName());
	}
	if (Params.Instigator)
	{
		Actor->SetInstigator(Params.Instigator);
		UE_LOG(LogTemp, Verbose, TEXT("PACS_SpawnOrchestrator::PrepareActorForUse: Set instigator for %s to %s"),
			*Actor->GetName(), *Params.Instigator->GetName());
	}

	// Apply selection profile from spawn config
	// IMPORTANT: Do NOT skip on dedicated server - SK mesh must be set for replication!
	// We only skip expensive visual effects on DS, not replicated data
	{
		// Find the spawn tag for this actor to get its config
		FGameplayTag* TagPtr = ActorToTagMap.Find(Actor);
		if (TagPtr && SpawnConfig)
		{
			UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnOrchestrator::PrepareActorForUse: Found spawn tag %s for actor %s"),
				*TagPtr->ToString(), *Actor->GetName());

			FSpawnClassConfig Config;
			if (SpawnConfig->GetConfigForTag(*TagPtr, Config))
			{
				UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnOrchestrator::PrepareActorForUse: Got config for tag %s"),
					*TagPtr->ToString());

				// Apply the preloaded selection profile from config
				if (!Config.SelectionProfile.IsNull())
				{
					UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnOrchestrator::PrepareActorForUse: Selection profile soft ptr exists: %s"),
						*Config.SelectionProfile.ToString());

					// Use Get() instead of LoadSynchronous since profiles are preloaded
					UPACS_SelectionProfileAsset* ProfileAsset = Config.SelectionProfile.Get();
					if (ProfileAsset)
					{
						UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnOrchestrator::PrepareActorForUse: Got profile asset %s, applying to %s"),
							*ProfileAsset->GetName(), *Actor->GetName());

						// Use the new organized method for profile application
						ApplySelectionProfileToActor(Actor, ProfileAsset);
					}
					else
					{
						// Profile wasn't preloaded, log warning
						UE_LOG(LogTemp, Error, TEXT("PACS_SpawnOrchestrator::PrepareActorForUse: Selection profile not preloaded for tag %s (soft ptr: %s)"),
							*TagPtr->ToString(), *Config.SelectionProfile.ToString());
					}
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnOrchestrator::PrepareActorForUse: No selection profile configured for tag %s"),
						*TagPtr->ToString());
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnOrchestrator::PrepareActorForUse: No config found for tag %s"),
					*TagPtr->ToString());
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnOrchestrator::PrepareActorForUse: No spawn tag or config for actor %s (TagPtr: %s, SpawnConfig: %s)"),
				*Actor->GetName(),
				TagPtr ? TEXT("Valid") : TEXT("Null"),
				SpawnConfig ? TEXT("Valid") : TEXT("Null"));
		}
	}

	// Enable actor
	Actor->SetActorHiddenInGame(false);
	Actor->SetActorEnableCollision(true);
	Actor->SetActorTickEnabled(true);
	UE_LOG(LogTemp, Verbose, TEXT("PACS_SpawnOrchestrator::PrepareActorForUse: Enabled actor %s"), *Actor->GetName());

	// Prepare replication
	PrepareReplicationState(Actor);
	UE_LOG(LogTemp, Verbose, TEXT("PACS_SpawnOrchestrator::PrepareActorForUse: Prepared replication for %s"), *Actor->GetName());

	// Call poolable interface if implemented
	if (Actor->GetClass()->ImplementsInterface(UPACS_Poolable::StaticClass()))
	{
		IPACS_Poolable::Execute_OnAcquiredFromPool(Actor);
		UE_LOG(LogTemp, Verbose, TEXT("PACS_SpawnOrchestrator::PrepareActorForUse: Called OnAcquiredFromPool for %s"),
			*Actor->GetName());
	}

	UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnOrchestrator::PrepareActorForUse: Completed for %s"), *Actor->GetName());
}

void UPACS_SpawnOrchestrator::LoadActorClass(FGameplayTag SpawnTag)
{
	FPoolEntry* Pool = Pools.Find(SpawnTag);
	if (!Pool)
	{
		return;
	}

	// Mark as loading
	Pool->bIsLoading = true;

	// Get the class from spawn config
	if (!SpawnConfig)
	{
		UE_LOG(LogTemp, Error, TEXT("PACS_SpawnOrchestrator: No SpawnConfig set, cannot load class for tag %s"),
			*SpawnTag.ToString());
		Pool->bIsLoading = false;
		return;
	}

	// Get config for this tag
	FSpawnClassConfig Config;
	if (!SpawnConfig->GetConfigForTag(SpawnTag, Config))
	{
		UE_LOG(LogTemp, Error, TEXT("PACS_SpawnOrchestrator: No config found for tag %s"),
			*SpawnTag.ToString());
		Pool->bIsLoading = false;
		return;
	}

	// Store the soft class reference
	Pool->ActorClass = Config.ActorClass;

	// Update pool settings from config
	Pool->InitialSize = Config.PoolSettings.InitialSize;
	Pool->MaxSize = Config.PoolSettings.MaxSize;

	// Check if already loaded
	if (!Pool->ActorClass.IsNull() && Pool->ActorClass.IsValid())
	{
		Pool->ResolvedClass = Pool->ActorClass.Get();
		Pool->bIsLoading = false;
		OnActorClassLoaded(SpawnTag);
		return;
	}

	// Async load the class
	TSharedPtr<FStreamableHandle> Handle = StreamableManager.RequestAsyncLoad(
		Pool->ActorClass.ToSoftObjectPath(),
		FStreamableDelegate::CreateUObject(this, &UPACS_SpawnOrchestrator::OnActorClassLoaded, SpawnTag)
	);

	if (Handle.IsValid())
	{
		LoadHandles.Add(SpawnTag, Handle);
		UE_LOG(LogTemp, Log, TEXT("PACS_SpawnOrchestrator: Async loading class for tag %s"),
			*SpawnTag.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("PACS_SpawnOrchestrator: Failed to start async load for tag %s"),
			*SpawnTag.ToString());
		Pool->bIsLoading = false;
	}
}

void UPACS_SpawnOrchestrator::OnActorClassLoaded(FGameplayTag SpawnTag)
{
	FPoolEntry* Pool = Pools.Find(SpawnTag);
	if (!Pool)
	{
		return;
	}

	// Get the loaded class
	if (!Pool->ActorClass.IsNull())
	{
		Pool->ResolvedClass = Pool->ActorClass.Get();

		if (Pool->ResolvedClass)
		{
			UE_LOG(LogTemp, Log, TEXT("PACS_SpawnOrchestrator: Successfully loaded class %s for tag %s"),
				*Pool->ResolvedClass->GetName(), *SpawnTag.ToString());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("PACS_SpawnOrchestrator: Failed to resolve class for tag %s"),
				*SpawnTag.ToString());
		}
	}

	Pool->bIsLoading = false;

	// Remove load handle
	LoadHandles.Remove(SpawnTag);

	// Process any pending requests
	ProcessPendingRequests(SpawnTag);
}

void UPACS_SpawnOrchestrator::ProcessPendingRequests(FGameplayTag SpawnTag)
{
	FPoolEntry* Pool = Pools.Find(SpawnTag);
	if (!Pool || !Pool->ResolvedClass)
	{
		return;
	}

	// Process all pending requests
	for (const auto& Request : Pool->PendingRequests)
	{
		FSpawnRequestParams Params;
		Params.Transform = Request.Key;

		AActor* Actor = AcquireActor(SpawnTag, Params);

		// Execute callback
		if (Request.Value)
		{
			Request.Value(Actor);
		}
	}

	Pool->PendingRequests.Empty();
}

void UPACS_SpawnOrchestrator::ResetReplicationState(AActor* Actor)
{
	if (!Actor || !Actor->GetIsReplicated())
	{
		return;
	}

	// Set dormancy to fully dormant
	if (Actor->NetDormancy != DORM_Never)
	{
		Actor->SetNetDormancy(DORM_DormantAll);
	}

	// Clear any pending net updates
	Actor->ForceNetUpdate();
}

void UPACS_SpawnOrchestrator::PrepareReplicationState(AActor* Actor)
{
	if (!Actor || !Actor->GetIsReplicated())
	{
		return;
	}

	// Wake up dormancy
	if (Actor->NetDormancy != DORM_Never)
	{
		Actor->SetNetDormancy(DORM_Awake);
	}

	// Force immediate replication
	Actor->ForceNetUpdate();
}

void UPACS_SpawnOrchestrator::PreloadSelectionProfiles()
{
	// Skip on dedicated server - selection profiles are visual only
	if (!SpawnConfig || !GetWorld())
	{
		return;
	}

	if (GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// Get all spawn configs from the spawn config asset
	const TArray<FSpawnClassConfig>& Configs = SpawnConfig->GetSpawnConfigs();

	// Collect all unique selection profiles
	TSet<FSoftObjectPath> ProfilesToLoad;
	for (const FSpawnClassConfig& Config : Configs)
	{
		if (!Config.SelectionProfile.IsNull())
		{
			ProfilesToLoad.Add(Config.SelectionProfile.ToSoftObjectPath());
		}
	}

	if (ProfilesToLoad.Num() == 0)
	{
		return;
	}

	// Batch load all selection profiles asynchronously
	FStreamableManager& AssetStreamableManager = UAssetManager::GetStreamableManager();
	TArray<FSoftObjectPath> ProfilePathArray = ProfilesToLoad.Array();

	// First load the profiles themselves, then extract and load their SK meshes
	TSharedPtr<FStreamableHandle> ProfileHandle = AssetStreamableManager.RequestAsyncLoad(
		ProfilePathArray,
		FStreamableDelegate::CreateLambda([this, ProfilePathArray]()
		{
			int32 LoadedProfileCount = 0;
			TSet<FSoftObjectPath> SKMeshesToLoad;

			// Iterate through loaded profiles to extract SK mesh assets
			for (const FSoftObjectPath& ProfilePath : ProfilePathArray)
			{
				if (UObject* LoadedObject = ProfilePath.ResolveObject())
				{
					LoadedProfileCount++;

					// Cast to selection profile asset
					if (UPACS_SelectionProfileAsset* ProfileAsset = Cast<UPACS_SelectionProfileAsset>(LoadedObject))
					{
						// Extract SK mesh asset if present (Principle #1: Pre-load all visual assets)
						if (!ProfileAsset->SkeletalMeshAsset.IsNull())
						{
							SKMeshesToLoad.Add(ProfileAsset->SkeletalMeshAsset.ToSoftObjectPath());
							UE_LOG(LogTemp, Verbose, TEXT("PACS_SpawnOrchestrator: Found SK mesh to preload: %s"),
								*ProfileAsset->SkeletalMeshAsset.ToString());
						}
					}
				}
			}

			UE_LOG(LogTemp, Log, TEXT("PACS_SpawnOrchestrator: Pre-loaded %d/%d selection profiles"),
				LoadedProfileCount, ProfilePathArray.Num());

			// Now load the SK meshes referenced by the profiles
			// Principle #1 & #5: Pre-load Selection Profiles including proper SK asset handling
			if (SKMeshesToLoad.Num() > 0)
			{
				TArray<FSoftObjectPath> SKMeshPathArray = SKMeshesToLoad.Array();

				FStreamableManager& Manager = UAssetManager::GetStreamableManager();
				TSharedPtr<FStreamableHandle> SKMeshHandle = Manager.RequestAsyncLoad(
					SKMeshPathArray,
					FStreamableDelegate::CreateLambda([SKMeshPathArray]()
					{
						int32 LoadedMeshCount = 0;
						for (const FSoftObjectPath& MeshPath : SKMeshPathArray)
						{
							if (MeshPath.ResolveObject())
							{
								LoadedMeshCount++;
							}
						}

						UE_LOG(LogTemp, Log, TEXT("PACS_SpawnOrchestrator: Pre-loaded %d/%d SK meshes from selection profiles"),
							LoadedMeshCount, SKMeshPathArray.Num());
					})
				);

				// Store handle to keep SK meshes loaded (Principle #4: Pool Pre-configured NPCs)
				if (SKMeshHandle.IsValid())
				{
					LoadHandles.Add(FGameplayTag::RequestGameplayTag(TEXT("PACS.Preload.SKMeshes")), SKMeshHandle);
				}
			}
		})
	);

	// Store handle to keep profile assets loaded
	if (ProfileHandle.IsValid())
	{
		LoadHandles.Add(FGameplayTag::RequestGameplayTag(TEXT("PACS.Preload.SelectionProfiles")), ProfileHandle);
	}

	UE_LOG(LogTemp, Log, TEXT("PACS_SpawnOrchestrator: Started pre-loading %d selection profiles"), ProfilesToLoad.Num());
}

// === PROFILE APPLICATION SECTION ===
// Organized methods for clean separation of profile application logic

void UPACS_SpawnOrchestrator::ApplySelectionProfileToActor(AActor* Actor, UPACS_SelectionProfileAsset* Profile)
{
	if (!Actor || !Profile)
	{
		LogProfileApplicationStatus(Actor, false, TEXT("Null actor or profile"));
		return;
	}

	// Log the start of profile application
	UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnOrchestrator: Starting profile application for actor %s with profile %s"),
		*Actor->GetName(), *Profile->GetName());

	// Verify assets are loaded before attempting application
	if (!VerifyProfileAssetsLoaded(Profile))
	{
		LogProfileApplicationStatus(Actor, false, TEXT("Profile assets not loaded"));
		return;
	}

	// Apply profile based on actor type
	bool bProfileApplied = false;

	if (APACS_NPC_Base_Char* CharNPC = Cast<APACS_NPC_Base_Char>(Actor))
	{
		ApplyProfileToCharacterNPC(CharNPC, Profile);
		bProfileApplied = true;
	}
	else if (APACS_NPC_Base_Veh* VehNPC = Cast<APACS_NPC_Base_Veh>(Actor))
	{
		ApplyProfileToVehicleNPC(VehNPC, Profile);
		bProfileApplied = true;
	}
	else if (APACS_NPC_Base_LW* LightweightNPC = Cast<APACS_NPC_Base_LW>(Actor))
	{
		ApplyProfileToLightweightNPC(LightweightNPC, Profile);
		bProfileApplied = true;
	}
	else
	{
		LogProfileApplicationStatus(Actor, false, TEXT("Unknown actor type"));
	}

	if (bProfileApplied)
	{
		LogProfileApplicationStatus(Actor, true, TEXT("Profile applied successfully"));
	}
}

void UPACS_SpawnOrchestrator::ApplyProfileToCharacterNPC(APACS_NPC_Base_Char* CharNPC, UPACS_SelectionProfileAsset* Profile)
{
	if (!CharNPC || !Profile)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnOrchestrator: Applying profile to Character NPC %s"), *CharNPC->GetName());

	// Call SetSelectionProfile on the character NPC
	// This will handle SK mesh application and other profile settings
	CharNPC->SetSelectionProfile(Profile);

	// Additional logging to verify SK mesh status
	if (USkeletalMeshComponent* MeshComp = CharNPC->GetMesh())
	{
		if (USkeletalMesh* CurrentMesh = MeshComp->GetSkeletalMeshAsset())
		{
			UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnOrchestrator: Character NPC %s now has SK mesh: %s"),
				*CharNPC->GetName(), *CurrentMesh->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("PACS_SpawnOrchestrator: Character NPC %s has NO SK mesh after profile application!"),
				*CharNPC->GetName());
		}
	}
}

void UPACS_SpawnOrchestrator::ApplyProfileToVehicleNPC(APACS_NPC_Base_Veh* VehNPC, UPACS_SelectionProfileAsset* Profile)
{
	if (!VehNPC || !Profile)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnOrchestrator: Applying profile to Vehicle NPC %s"), *VehNPC->GetName());

	// Call SetSelectionProfile on the vehicle NPC
	VehNPC->SetSelectionProfile(Profile);
}

void UPACS_SpawnOrchestrator::ApplyProfileToLightweightNPC(APACS_NPC_Base_LW* LightweightNPC, UPACS_SelectionProfileAsset* Profile)
{
	if (!LightweightNPC || !Profile)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnOrchestrator: Applying profile to Lightweight NPC %s"), *LightweightNPC->GetName());

	// Call SetSelectionProfile on the lightweight NPC
	// This will handle loading the skeletal mesh and animations
	LightweightNPC->SetSelectionProfile(Profile);

	// Log the skeletal mesh application status
	if (USkeletalMeshComponent* MeshComp = LightweightNPC->SkeletalMeshComponent)
	{
		if (USkeletalMesh* CurrentMesh = MeshComp->GetSkeletalMeshAsset())
		{
			UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnOrchestrator: Lightweight NPC %s has SK mesh %s after profile application"),
				*LightweightNPC->GetName(), *CurrentMesh->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("PACS_SpawnOrchestrator: Lightweight NPC %s has NO SK mesh after profile application!"),
				*LightweightNPC->GetName());
		}
	}
}

bool UPACS_SpawnOrchestrator::VerifyProfileAssetsLoaded(UPACS_SelectionProfileAsset* Profile)
{
	if (!Profile)
	{
		return false;
	}

	// Check if the profile's SK mesh is loaded
	bool bSKMeshLoaded = true;
	if (!Profile->SkeletalMeshAsset.IsNull())
	{
		USkeletalMesh* LoadedMesh = Profile->SkeletalMeshAsset.Get();
		if (!LoadedMesh)
		{
			// Try synchronous load as fallback
			LoadedMesh = Profile->SkeletalMeshAsset.LoadSynchronous();
			if (LoadedMesh)
			{
				UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnOrchestrator: Had to synchronously load SK mesh for profile %s"),
					*Profile->GetName());
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("PACS_SpawnOrchestrator: Failed to load SK mesh for profile %s"),
					*Profile->GetName());
				bSKMeshLoaded = false;
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnOrchestrator: SK mesh already loaded for profile %s: %s"),
				*Profile->GetName(), *LoadedMesh->GetName());
		}
	}

	return bSKMeshLoaded;
}

void UPACS_SpawnOrchestrator::LogProfileApplicationStatus(AActor* Actor, bool bSuccess, const FString& Reason)
{
	const FString ActorName = Actor ? Actor->GetName() : TEXT("NULL");

	if (bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnOrchestrator: Profile application SUCCESS for %s: %s"),
			*ActorName, *Reason);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("PACS_SpawnOrchestrator: Profile application FAILED for %s: %s"),
			*ActorName, *Reason);
	}
}