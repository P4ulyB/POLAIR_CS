// Copyright 2025 BitProtectStudio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "NpcDataProComponent.h"
#include "GameFramework/Actor.h"
#include "Runtime/Core/Public/HAL/Runnable.h"
#include "Runtime/Core/Public/HAL/ThreadSafeBool.h"

#include "WorldDirectorNpcPRO.generated.h"

UENUM(BlueprintType)
enum class EWalkingType : uint8
{
	EDITOR	  UMETA(DisplayName = "Editor"),
	SHIPPING  UMETA(DisplayName = "Shipping"),
	NONE	  UMETA(DisplayName = "Not Use")
};

// Not delete. Used in BP.
USTRUCT(BlueprintType)
struct FNpcStructProInBP
{
	GENERATED_USTRUCT_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Director Parameters")
	int indexNpc = 0;

	FNpcStructProInBP()
	{
	};
};

USTRUCT(BlueprintType)
struct FNpcStructPro
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "World Director Struct")
	FVector npcLocation = FVector::ZeroVector;
	UPROPERTY()
	FRotator npcRotation = FRotator::ZeroRotator;
	UPROPERTY()
	FVector npcScale = FVector::ZeroVector;

	UPROPERTY()
	FVector npcSpawnLocation = FVector::ZeroVector;
	UPROPERTY()
	FVector npcTargetLocation = FVector::ZeroVector;
	UPROPERTY()
	FVector randomLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "World Director Struct")
	TArray<FVector> pathPoints;

	UPROPERTY()
	float nowTime = 0.f;
	
	UPROPERTY()
	bool bIsNoPoints = false;
	UPROPERTY()
	bool bIsNeedFindWaypoint = true;

	UPROPERTY(BlueprintReadOnly, Category = "World Director Struct")
	bool bIsCanMove = false;

	UPROPERTY(BlueprintReadOnly, Category = "World Director Struct")
	bool bIsNearNPC = false;
	
	UPROPERTY()
	UClass* classNpc_ = nullptr;
	UPROPERTY()
	FNpcDataPro npcData;

	UPROPERTY()
	FString npcUniqName;

	bool operator == (const FNpcStructPro &npcStructPro) const
	{
		if (npcLocation == npcStructPro.npcLocation && npcRotation == npcStructPro.npcRotation && npcUniqName == npcStructPro.npcUniqName)
		{
			return true;
		}
		
		return false;
	}

	FNpcStructPro()
	{
	};
};

USTRUCT(BlueprintType)
struct FDirectorProStruct
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "All Players Classes")
	TArray<TSubclassOf<AActor>> playersClassesArr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Director Parameters")
	EWalkingType walkingType = EWalkingType::EDITOR;

	// Global layer control.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Director Parameters")
	float firstLayerRadius = 3000.f;
	UPROPERTY()
	float firstLayerRadiusSquare = 0.f;
	UPROPERTY()
	float mainLayerRadiusSquare = 0.f;
	// Global layer control.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Director Parameters")
	float secondLayerRadius = 15000.f;
	UPROPERTY()
	float secondLayerRadiusSquare = 0.f;
	// Global layer control.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Director Parameters")
	float thirdLayerRadius = 30000.f;
	UPROPERTY()
	float thirdLayerRadiusSquare = 0.f;
	// Global layer control.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director Actor Parameters")
	float layerOffset = 500.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Director Actor Parameters")
	bool bInstanceCastShadows = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Director Parameters")
	bool bUseInstanceSimulate = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Director Parameters", meta = (ClampMin = 0), meta = (EditCondition="bUseInstanceSimulate", EditConditionHides))
	float maxDistanceShowSimulation = 15000.f;
	UPROPERTY()
	float maxDistanceShowSimulationSquare = 0.f;

	FDirectorProStruct()
	{
	};
};

UCLASS()
class WORLDDIRECTORPRO_API AWorldDirectorNpcPRO : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AWorldDirectorNpcPRO();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	//************************************************************************
	// Component                                                                  
	//************************************************************************

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Component")
	class UInstancedStaticMeshComponent* StaticMeshInstanceComponent;

	//************************************************************************

	bool RegisterNPC(AActor *actorRef);

	UFUNCTION(BlueprintPure, Category = "World Director Struct")
    int GetBackgroundNpcCount() const;

	UFUNCTION(BlueprintCallable, Category = "World Director PRO")
	bool SaveState(FString slotNameString = "WorldDirectorPRO", int playerIndex = 0);
	UFUNCTION(BlueprintCallable, Category = "World Director PRO")
	bool LoadState(FString slotNameString = "WorldDirectorPRO", int playerIndex = 0);

	UFUNCTION(BlueprintImplementableEvent, Category = "World Director PRO")
	void SaveStateBlueprintEvent();
	UFUNCTION(BlueprintImplementableEvent, Category = "World Director PRO")
	void LoadStateBlueprintEvent();

	UFUNCTION(BlueprintImplementableEvent, Category = "World Director Parameters")
	void InsertNPCInBackground_BP(AActor* actorRef);
	UFUNCTION(BlueprintImplementableEvent, Category = "World Director Parameters")
	void RestoreNPC_BP(int indexNpc, AActor* actorRef);

	void RemoveActorFormSystem(AActor* setActor);

	// Set Enable World Director.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Director Parameters")
	bool bIsActivate = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Director Parameters")
	bool bIsDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Director Parameters", meta = (ClampMin = 0.025))
	float updateRate = 0.2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Director Parameters")
	FDirectorProStruct directorParameters;
	

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void BeginDestroy() override;

	void InsertNPCinBackground(AActor* setNPC);

	void ExchangeInformationTimer();

	void RegisterInstanceStaticMeshComponent(FNpcStructPro &npcStruct);

	void UpdateInstanceSimulate(const FVector &plrDistance);

	void RemoveInstance(UNpcDataProComponent* actorComp_);

private:

	UPROPERTY();
	TArray<FNpcStructPro> backgroundNPCArr;

	UPROPERTY()
	TArray<FNpcStructPro> allThreadNPCArr_Debug;

	UPROPERTY();
	TArray<FString> allNpcInBackgroundArr_ForBP;

	UPROPERTY()
	TArray<AActor*> allRegisteredNpcArr;

	class DirectorProThread* directorThreadRef;

	FTimerHandle exchangeInformation_Timer;

	int npcInBackgroundDebug = 0;

	UPROPERTY()
	class USaveGameWdPRO* wdSaveGame = nullptr;

	UPROPERTY()
	class UNavigationSystemV1* navSystem = nullptr;

	UPROPERTY()
	TArray<class ANpcPathPro*> npcPathArr;

	UPROPERTY()
	TArray<class AInterestingPlacePro*> interestingPlacesArr;

	UPROPERTY()
	TArray<class UInstancedStaticMeshComponent*> simulateStaticMeshComponentsArr;

	UPROPERTY()
	TArray<FNpcStructPro> instanceSimulateNpcArr;

};

// Thread
class DirectorProThread : public FRunnable
{
public:

	//================================= THREAD =====================================
    
	//Constructor
	DirectorProThread(AActor *newActor, FDirectorProStruct setDirectorParameters);
	//Destructor
	virtual ~DirectorProThread() override;

	//Use this method to kill the thread!!
	void EnsureCompletion();
	//Pause the thread 
	void PauseThread();
	//Continue/UnPause the thread
	void ContinueThread();
	//FRunnable interface.
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	bool IsThreadPaused() const;

	//================================= THREAD =====================================

	TArray<FNpcStructPro> UpdateData(const TArray<FNpcStructPro> &setNewBackgroundNPCArr, const TArray<FVector> &setPlayersLocationsArr,TArray<FNpcStructPro> &getAllThreadNPCArr, TArray<class ANpcPathPro*> npcPathArr, TArray<class AInterestingPlacePro*> interestingPlacesArr);

	void GetData(TArray<FNpcStructPro> &getLookNavPathNpc) const;

	void UpdatePathForNpc(const TArray<FNpcStructPro> &navPathNpc);

	void UnlockThread();

	void SaveThreadData(const TArray<FNpcStructPro> &setNewBackgroundNPCArr, TArray<FNpcStructPro> &setBackgroundArr, TArray<FNpcStructPro> &setRestoreArr);
	void LoadThreadData(const TArray<FNpcStructPro> &setBackgroundArr, const TArray<FNpcStructPro> &setRestoreArr);

	void WanderingNPCinBackground(FNpcStructPro &npcStruct);

	static void WanderNPCinBackground(FNpcStructPro &npcStruct, const float &inDeltaTime);
	
	AActor *ownerActor;

private:

	//================================= THREAD =====================================
	
	//Thread to run the worker FRunnable on
	FRunnableThread* Thread;

	FCriticalSection m_mutex;
	FEvent* m_semaphore;

	int m_chunkCount;
	int m_amount;
	int m_MinInt;
	int m_MaxInt;

	float threadSleepTime = 0.01f;

	//As the name states those members are Thread safe
	FThreadSafeBool m_Kill;
	FThreadSafeBool m_Pause;


	//================================= THREAD =====================================

	// Save
	TArray<FNpcStructPro> allBackgroundNPCArrTH;

	// Save
	TArray<FNpcStructPro> canRestoreNPCArrTH;
	
	TArray<FNpcStructPro> lookNavPathNpcArrTH;
	
	class UNavigationSystemV1* navigationSystemTH = nullptr;

	// All found players locations. 
	TArray<FVector> playersLocationsArr;

	FDirectorProStruct directorParametersTH;

	float threadDeltaTime = 0.f;
	float lastFrameTime = 0.f;

	int incrementNearGetPathNpc = 15;
	int minNearGetPathNpcID = 0;
	int maxNearGetPathNpcID = 0;

	int incrementFarGetPathNpc = 5;
	int minFarGetPathNpcID = 0;
	int maxFarGetPathNpcID = 0;

	// Skip frame for performance.
	int skipFrameValue = 0;
	int nowFrame = 0;

	bool updatePath = false;

	TArray<class ANpcPathPro*> npcPathArrTH;
	TArray<class AInterestingPlacePro*> interestingPlacesArrTH;
};
