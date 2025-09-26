#include "Core/PACS_ReplicationGraph.h"
#include "Engine/LevelScriptActor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Info.h"
#include "Engine/LevelScriptActor.h"
#include "Net/UnrealNetwork.h"


UPACS_ReplicationGraph::UPACS_ReplicationGraph()
{
	// Configure default net cull distances
	NPCNetCullDistance = 15000.0f; // 150 meters
	GridCellSize = 10000.0f; // 100 meters

	// Epic pattern: Configure default settings
}

void UPACS_ReplicationGraph::ResetGameWorldState()
{
	Super::ResetGameWorldState();

	AlwaysRelevantActors.Reset();
	AlwaysRelevantForConnectionNodes.Reset();

	for (UNetReplicationGraphConnection* ConnManager : Connections)
	{
		if (ConnManager)
		{
			ConnManager->NotifyResetAllNetworkActors();
		}
	}
}

void UPACS_ReplicationGraph::InitGlobalActorClassSettings()
{
	Super::InitGlobalActorClassSettings();

	// Epic pattern: Configure class replication settings
	InitClassReplicationInfo();
}

void UPACS_ReplicationGraph::InitClassReplicationInfo()
{
	// Epic pattern: Register NPC class with replication info
	FClassReplicationInfo NPCInfo;
	NPCInfo.DistancePriorityScale = 1.0f;
	NPCInfo.StarvationPriorityScale = 1.0f;
	NPCInfo.ActorChannelFrameTimeout = 4;
	NPCInfo.SetCullDistanceSquared(NPCNetCullDistance * NPCNetCullDistance);
	NPCInfo.ReplicationPeriodFrame = 2;

	// NPC class registration removed - will be reimplemented later
	// Track spatialized classes
	SpatializedClasses.Add(APawn::StaticClass());

	// Track always relevant classes
	AlwaysRelevantClasses.Add(AGameStateBase::StaticClass());
	AlwaysRelevantClasses.Add(AGameModeBase::StaticClass());
	AlwaysRelevantClasses.Add(AInfo::StaticClass());
	AlwaysRelevantClasses.Add(APlayerState::StaticClass());
}

void UPACS_ReplicationGraph::InitGlobalGraphNodes()
{
	// Epic pattern: Create spatial grid node for NPCs
	GridNode = CreateNewNode<UReplicationGraphNode_GridSpatialization2D>();
	GridNode->CellSize = GridCellSize;
	GridNode->SpatialBias = FVector2D(SpatialBiasX, SpatialBiasY);
	// GridNode->SetProcessOnSpatialConnectionOnly(); // API may vary by version
	AddGlobalGraphNode(GridNode);

	// Create always relevant node
	AlwaysRelevantNode = CreateNewNode<UReplicationGraphNode_AlwaysRelevant>();
	AddGlobalGraphNode(AlwaysRelevantNode);
}

void UPACS_ReplicationGraph::InitConnectionGraphNodes(UNetReplicationGraphConnection* RepGraphConnection)
{
	Super::InitConnectionGraphNodes(RepGraphConnection);

	// Create per-connection always relevant node
	UReplicationGraphNode_AlwaysRelevant_ForConnection* AlwaysRelevantNodeForConnection = CreateNewNode<UReplicationGraphNode_AlwaysRelevant_ForConnection>();
	AddConnectionGraphNode(AlwaysRelevantNodeForConnection, RepGraphConnection);
	AlwaysRelevantForConnectionNodes.Add(RepGraphConnection->NetConnection, AlwaysRelevantNodeForConnection);

	// Epic pattern: Per-connection node setup complete
}

void UPACS_ReplicationGraph::RouteAddNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo, FGlobalActorReplicationInfo& GlobalInfo)
{
	AActor* Actor = ActorInfo.Actor;
	if (!Actor)
	{
		return;
	}

	// Route to spatial node if applicable
	if (IsActorSpatiallyRelevant(Actor))
	{
		if (GridNode)
		{
			GridNode->NotifyAddNetworkActor(ActorInfo);
		}
	}
	// Otherwise route to always relevant
	else if (Actor->bAlwaysRelevant || AlwaysRelevantClasses.Contains(Actor->GetClass()))
	{
		if (AlwaysRelevantNode)
		{
			AlwaysRelevantNode->NotifyAddNetworkActor(ActorInfo);
		}
	}

	// Handle per-connection relevancy (owned actors)
	if (Actor->bOnlyRelevantToOwner)
	{
		if (UNetConnection* OwnerConnection = Actor->GetNetConnection())
		{
			if (auto NodePtr = AlwaysRelevantForConnectionNodes.Find(OwnerConnection))
			{
				if (*NodePtr)
				{
					(*NodePtr)->NotifyAddNetworkActor(ActorInfo);
				}
			}
		}
	}
}

void UPACS_ReplicationGraph::RouteRemoveNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo)
{
	AActor* Actor = ActorInfo.Actor;
	if (!Actor)
	{
		return;
	}

	// Remove from spatial node
	if (IsActorSpatiallyRelevant(Actor) && GridNode)
	{
		GridNode->NotifyRemoveNetworkActor(ActorInfo);
	}

	// Remove from always relevant
	if (AlwaysRelevantNode)
	{
		AlwaysRelevantNode->NotifyRemoveNetworkActor(ActorInfo);
	}

	// Remove from per-connection nodes
	for (auto& Pair : AlwaysRelevantForConnectionNodes)
	{
		if (Pair.Value)
		{
			Pair.Value->NotifyRemoveNetworkActor(ActorInfo);
		}
	}

	// Remove from always relevant list
	AlwaysRelevantActors.RemoveSingleSwap(Actor);
}

int32 UPACS_ReplicationGraph::ServerReplicateActors(float DeltaSeconds)
{
	// Epic pattern: Call parent implementation
	return Super::ServerReplicateActors(DeltaSeconds);
}

void UPACS_ReplicationGraph::AddAlwaysRelevantActor(AActor* Actor)
{
	if (Actor && !AlwaysRelevantActors.Contains(Actor))
	{
		AlwaysRelevantActors.Add(Actor);

		// Route to always relevant node
		if (AlwaysRelevantNode)
		{
			FNewReplicatedActorInfo ActorInfo(Actor);
			AlwaysRelevantNode->NotifyAddNetworkActor(ActorInfo);
		}
	}
}

void UPACS_ReplicationGraph::RemoveAlwaysRelevantActor(AActor* Actor)
{
	AlwaysRelevantActors.RemoveSingleSwap(Actor);

	if (AlwaysRelevantNode)
	{
		FNewReplicatedActorInfo ActorInfo(Actor);
		AlwaysRelevantNode->NotifyRemoveNetworkActor(ActorInfo);
	}
}

bool UPACS_ReplicationGraph::IsActorSpatiallyRelevant(const AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}

	// Check if actor class is in spatialized classes
	for (UClass* SpatialClass : SpatializedClasses)
	{
		if (Actor->IsA(SpatialClass))
		{
			// Don't spatialize always relevant actors
			if (Actor->bAlwaysRelevant || AlwaysRelevantClasses.Contains(Actor->GetClass()))
			{
				return false;
			}

			// Don't spatialize actors that are only relevant to owner
			if (Actor->bOnlyRelevantToOwner)
			{
				return false;
			}

			return true;
		}
	}

	return false;
}

