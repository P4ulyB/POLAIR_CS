#include "Actors/PACS_SpawnPoint.h"
#include "Subsystems/PACS_SpawnOrchestrator.h"
#include "Engine/World.h"
#include "TimerManager.h"

#if WITH_EDITORONLY_DATA
#include "Components/BillboardComponent.h"
#include "Components/ArrowComponent.h"
#include "UObject/ConstructorHelpers.h"
#endif

APACS_SpawnPoint::APACS_SpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false; // Spawn points don't need replication

	// Root component
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

#if WITH_EDITORONLY_DATA
	// Billboard for editor visibility
	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	if (BillboardComponent)
	{
		BillboardComponent->SetupAttachment(RootComponent);
		BillboardComponent->bHiddenInGame = true;

		// Set default spawn icon
		static ConstructorHelpers::FObjectFinder<UTexture2D> SpawnIconFinder(
			TEXT("/Engine/EditorResources/S_Actor"));
		if (SpawnIconFinder.Succeeded())
		{
			BillboardComponent->SetSprite(SpawnIconFinder.Object);
		}
	}

	// Arrow to show spawn direction
	ArrowComponent = CreateDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	if (ArrowComponent)
	{
		ArrowComponent->SetupAttachment(RootComponent);
		ArrowComponent->ArrowColor = FColor::Green;
		ArrowComponent->ArrowSize = 1.5f;
		ArrowComponent->bHiddenInGame = true;
	}
#endif
}

void APACS_SpawnPoint::BeginPlay()
{
	Super::BeginPlay();

	// Only spawn on server
	if (!HasAuthority())
	{
		return;
	}

	// Get spawn orchestrator
	UWorld* World = GetWorld();
	if (World)
	{
		SpawnOrchestrator = World->GetSubsystem<UPACS_SpawnOrchestrator>();
	}

	if (!SpawnOrchestrator)
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnPoint: Could not find SpawnOrchestrator subsystem"));
		return;
	}

	// Validate spawn tag
	if (!SpawnTag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnPoint: Invalid spawn tag on %s"), *GetName());
		return;
	}

	// Auto-start if configured, but wait for orchestrator to be ready
	if (bAutoStart && SpawnPattern != ESpawnPattern::Manual)
	{
		// Check if orchestrator is ready immediately
		if (SpawnOrchestrator->IsReady())
		{
			StartSpawning();
		}
		else
		{
			// Set up a timer to check for readiness
			GetWorld()->GetTimerManager().SetTimer(
				ReadyCheckTimerHandle,
				this,
				&APACS_SpawnPoint::CheckOrchestratorReady,
				0.5f, // Check every half second
				true   // Repeat
			);
		}
	}
}

void APACS_SpawnPoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clean up timers
	GetWorld()->GetTimerManager().ClearTimer(SpawnTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(WaveTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(RespawnTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(ReadyCheckTimerHandle);

	// Release spawned actor back to pool
	if (SpawnedActor && SpawnOrchestrator)
	{
		SpawnOrchestrator->ReleaseActor(SpawnedActor);
		SpawnedActor = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void APACS_SpawnPoint::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Update arrow color based on spawn tag category
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(APACS_SpawnPoint, SpawnTag))
	{
		if (ArrowComponent)
		{
			FString TagString = SpawnTag.ToString();
			if (TagString.Contains("Human"))
			{
				ArrowComponent->ArrowColor = FColor::Blue;
			}
			else if (TagString.Contains("Vehicle"))
			{
				ArrowComponent->ArrowColor = FColor::Orange;
			}
			else if (TagString.Contains("Environment"))
			{
				ArrowComponent->ArrowColor = FColor::Red;
			}
			else if (TagString.Contains("Equipment"))
			{
				ArrowComponent->ArrowColor = FColor::Green;
			}
			else
			{
				ArrowComponent->ArrowColor = FColor::White;
			}
		}
	}
}
#endif

AActor* APACS_SpawnPoint::SpawnActor()
{
	// Server authority check
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnPoint: SpawnActor called on non-authoritative context"));
		return nullptr;
	}

	if (!SpawnOrchestrator)
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnPoint: No SpawnOrchestrator available"));
		return nullptr;
	}

	if (!SpawnTag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnPoint: Invalid spawn tag"));
		return nullptr;
	}

	// Release existing actor if any
	if (SpawnedActor)
	{
		DespawnActor();
	}

	// Prepare spawn parameters
	FSpawnRequestParams Params;
	Params.Transform = GetSpawnTransform();

	if (bOverrideOwner && SpawnOwner)
	{
		Params.Owner = SpawnOwner;
	}

	// Request actor from pool
	SpawnedActor = SpawnOrchestrator->AcquireActor(SpawnTag, Params);

	if (SpawnedActor)
	{
		UE_LOG(LogTemp, Log, TEXT("PACS_SpawnPoint: Spawned %s with tag %s"),
			*SpawnedActor->GetName(), *SpawnTag.ToString());

		// Handle continuous respawn
		if (SpawnPattern == ESpawnPattern::Continuous)
		{
			// Set up respawn timer when actor is destroyed
			// Note: We can't use OnDestroyed for pooled actors since they're not actually destroyed
			// Instead, we'll handle respawn differently
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnPoint: Failed to spawn actor with tag %s"),
			*SpawnTag.ToString());
	}

	return SpawnedActor;
}

void APACS_SpawnPoint::DespawnActor()
{
	if (!SpawnedActor || !SpawnOrchestrator)
	{
		return;
	}

	// Note: Pooled actors are not destroyed, just returned to pool

	// Return to pool
	SpawnOrchestrator->ReleaseActor(SpawnedActor);
	SpawnedActor = nullptr;

	UE_LOG(LogTemp, Log, TEXT("PACS_SpawnPoint: Despawned actor"));
}

void APACS_SpawnPoint::StartSpawning()
{
	if (!HasAuthority() || bIsSpawnActive)
	{
		return;
	}

	bIsSpawnActive = true;
	CurrentWaveCount = 0;

	switch (SpawnPattern)
	{
		case ESpawnPattern::Immediate:
			ExecuteSpawn();
			break;

		case ESpawnPattern::Delayed:
			GetWorld()->GetTimerManager().SetTimer(
				SpawnTimerHandle,
				this,
				&APACS_SpawnPoint::ExecuteSpawn,
				SpawnDelay,
				false
			);
			break;

		case ESpawnPattern::Wave:
			HandleWaveSpawn();
			break;

		case ESpawnPattern::Continuous:
			ExecuteSpawn();
			break;

		case ESpawnPattern::Manual:
			// Do nothing - manual spawning only
			break;
	}
}

void APACS_SpawnPoint::StopSpawning()
{
	if (!HasAuthority())
	{
		return;
	}

	bIsSpawnActive = false;

	// Clear all timers
	GetWorld()->GetTimerManager().ClearTimer(SpawnTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(WaveTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(RespawnTimerHandle);

	// Optionally despawn current actor
	// DespawnActor();
}

void APACS_SpawnPoint::ExecuteSpawn()
{
	SpawnActor();

	// Schedule next spawn if pattern requires it
	ScheduleNextSpawn();
}

void APACS_SpawnPoint::ScheduleNextSpawn()
{
	if (!bIsSpawnActive)
	{
		return;
	}

	switch (SpawnPattern)
	{
		case ESpawnPattern::Wave:
			// Handled by HandleWaveSpawn
			break;

		case ESpawnPattern::Continuous:
			// Respawn timer set when actor is destroyed
			break;

		default:
			// Single spawn patterns don't schedule next spawn
			break;
	}
}

void APACS_SpawnPoint::HandleWaveSpawn()
{
	if (CurrentWaveCount >= WaveCount)
	{
		// Wave complete
		bIsSpawnActive = false;
		CurrentWaveCount = 0;
		return;
	}

	// Spawn current wave actor
	ExecuteSpawn();
	CurrentWaveCount++;

	// Schedule next wave
	if (CurrentWaveCount < WaveCount)
	{
		GetWorld()->GetTimerManager().SetTimer(
			WaveTimerHandle,
			this,
			&APACS_SpawnPoint::HandleWaveSpawn,
			WaveInterval,
			false
		);
	}
}

void APACS_SpawnPoint::HandleContinuousRespawn()
{
	if (!bIsSpawnActive || SpawnPattern != ESpawnPattern::Continuous)
	{
		return;
	}

	// For pooled actors, continuous respawn needs to be handled differently
	// This could be called when we detect the actor should respawn
	// (e.g., via a custom event or timer rather than OnDestroyed)

	// Return current actor to pool
	if (SpawnedActor && SpawnOrchestrator)
	{
		SpawnOrchestrator->ReleaseActor(SpawnedActor);
		SpawnedActor = nullptr;
	}

	// Schedule respawn
	GetWorld()->GetTimerManager().SetTimer(
		RespawnTimerHandle,
		this,
		&APACS_SpawnPoint::ExecuteSpawn,
		RespawnDelay,
		false
	);
}

void APACS_SpawnPoint::CheckOrchestratorReady()
{
	if (!HasAuthority() || !SpawnOrchestrator)
	{
		return;
	}

	// Check if orchestrator is ready
	if (SpawnOrchestrator->IsReady())
	{
		// Clear the ready check timer
		GetWorld()->GetTimerManager().ClearTimer(ReadyCheckTimerHandle);

		// Start spawning now that config is loaded
		StartSpawning();

		UE_LOG(LogTemp, Log, TEXT("PACS_SpawnPoint: Orchestrator ready, starting spawning for %s"), *GetName());
	}
}

FTransform APACS_SpawnPoint::GetSpawnTransform() const
{
	if (bUseSpawnPointTransform)
	{
		// Combine spawn point transform with offset
		FTransform BaseTransform = GetActorTransform();
		return SpawnTransformOffset * BaseTransform;
	}
	else
	{
		// Use only the offset (world space)
		return SpawnTransformOffset;
	}
}