#pragma once

#include "CoreMinimal.h"
#include "ReplicationGraph.h"
#include "PACS_ReplicationGraph.generated.h"

class UReplicationGraphNode_GridSpatialization2D;
class UReplicationGraphNode_AlwaysRelevant_ForConnection;

/**
 * PACS Replication Graph
 * Optimized replication system for dedicated server with spatial grid
 * Replaces per-actor relevancy checks with efficient graph-based system
 */
UCLASS(Transient, config=Engine)
class POLAIR_CS_API UPACS_ReplicationGraph : public UReplicationGraph
{
	GENERATED_BODY()

public:
	UPACS_ReplicationGraph();

	// UReplicationGraph interface
	virtual void ResetGameWorldState() override;
	virtual void InitGlobalActorClassSettings() override;
	virtual void InitGlobalGraphNodes() override;
	virtual void InitConnectionGraphNodes(UNetReplicationGraphConnection* RepGraphConnection) override;
	virtual void RouteAddNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo, FGlobalActorReplicationInfo& GlobalInfo) override;
	virtual void RouteRemoveNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo) override;
	virtual int32 ServerReplicateActors(float DeltaSeconds) override;

	// Epic pattern: Provide way to force an actor to be always relevant
	UPROPERTY()
	TArray<TObjectPtr<AActor>> AlwaysRelevantActors;

protected:
	// Add actor to the always relevant list
	void AddAlwaysRelevantActor(AActor* Actor);

	// Remove actor from always relevant list
	void RemoveAlwaysRelevantActor(AActor* Actor);

	// Spatial node for NPC actors (main optimization)
	UPROPERTY()
	TObjectPtr<UReplicationGraphNode_GridSpatialization2D> GridNode;

	// Always relevant for all connections
	UPROPERTY()
	TObjectPtr<UReplicationGraphNode_AlwaysRelevant> AlwaysRelevantNode;

	// Per-player always relevant actors
	UPROPERTY()
	TMap<UNetConnection*, TObjectPtr<UReplicationGraphNode_AlwaysRelevant_ForConnection>> AlwaysRelevantForConnectionNodes;

	// Epic pattern: Track actor classes for efficient routing
	TClassMap<FClassReplicationInfo> GlobalActorReplicationInfoMap;

	// Grid configuration
	UPROPERTY(Config)
	float GridCellSize = 10000.0f; // 100 meters

	UPROPERTY(Config)
	float SpatialBiasX = 0.0f;

	UPROPERTY(Config)
	float SpatialBiasY = 0.0f;

	// Net cull distances per class
	UPROPERTY(Config)
	float NPCNetCullDistance = 15000.0f; // 150 meters

	// Helper to determine if an actor should be spatially relevant
	bool IsActorSpatiallyRelevant(const AActor* Actor) const;


private:
	// Track classes that should use spatial relevancy
	TSet<UClass*> SpatializedClasses;

	// Track classes that are always relevant
	TSet<UClass*> AlwaysRelevantClasses;

	// Initialize class settings
	void InitClassReplicationInfo();
};