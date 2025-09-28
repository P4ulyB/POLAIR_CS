// Copyright 2025 BitProtectStudio. All Rights Reserved.


#include "WorldDirectorNpcPRO.h"

#include "InterestingPlacePro.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "NpcPathPro.h"
#include "Kismet/GameplayStatics.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "TimerManager.h"
#include "SaveGameWdPRO.h"
#include "Runtime/Core/Public/HAL/RunnableThread.h"
#include "Engine.h"
#include "GameFramework/Pawn.h"


// Sets default values
AWorldDirectorNpcPRO::AWorldDirectorNpcPRO()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	StaticMeshInstanceComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("StaticMeshInstanceComponent"));
	StaticMeshInstanceComponent->SetupAttachment(RootComponent);
	StaticMeshInstanceComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StaticMeshInstanceComponent->SetGenerateOverlapEvents(false);
	StaticMeshInstanceComponent->SetCastShadow(false);
}

// Called when the game starts or when spawned
void AWorldDirectorNpcPRO::BeginPlay()
{
	Super::BeginPlay();

	// Run thread if has Authority.
	if (HasAuthority())
	{
		directorParameters.maxDistanceShowSimulationSquare = FMath::Square(directorParameters.maxDistanceShowSimulation);
		directorParameters.mainLayerRadiusSquare = FMath::Square(directorParameters.firstLayerRadius + directorParameters.layerOffset);
		directorParameters.firstLayerRadiusSquare = FMath::Square(directorParameters.firstLayerRadius);
		directorParameters.secondLayerRadiusSquare = FMath::Square(directorParameters.secondLayerRadius);
		directorParameters.thirdLayerRadiusSquare = FMath::Square(directorParameters.thirdLayerRadius);

		// Run thread.
		directorThreadRef = nullptr;
		directorThreadRef = new DirectorProThread(this, directorParameters);

		bIsActivate = true;

		// Worker timers.
		GetWorldTimerManager().SetTimer(exchangeInformation_Timer, this, &AWorldDirectorNpcPRO::ExchangeInformationTimer, updateRate, true, updateRate);

		if (directorParameters.walkingType == EWalkingType::EDITOR)
		{
			navSystem = UNavigationSystemV1::GetNavigationSystem(this);
		}


		// Search all NPC Path Points.
		TArray<AActor*> foundActorsArr;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ANpcPathPro::StaticClass(), foundActorsArr);
		for (int ph_ = 0; ph_ < foundActorsArr.Num(); ++ph_)
		{
			ANpcPathPro* newPath_ = Cast<ANpcPathPro>(foundActorsArr[ph_]);
			if (newPath_)
			{
				npcPathArr.Add(newPath_);
			}
		}
		foundActorsArr.Empty();


		// Search all NPC Path Points.
		TArray<AActor*> foundPlacesArr;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AInterestingPlacePro::StaticClass(), foundPlacesArr);
		for (int ph_ = 0; ph_ < foundPlacesArr.Num(); ++ph_)
		{
			AInterestingPlacePro* newPath_ = Cast<AInterestingPlacePro>(foundPlacesArr[ph_]);
			if (newPath_)
			{
				interestingPlacesArr.Add(newPath_);
			}
		}
		foundPlacesArr.Empty();
	}
	else
	{
		bIsActivate = false;
	}
}

void AWorldDirectorNpcPRO::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (directorThreadRef)
	{
		directorThreadRef->EnsureCompletion();
		delete directorThreadRef;
		directorThreadRef = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AWorldDirectorNpcPRO::BeginDestroy()
{
	if (directorThreadRef)
	{
		directorThreadRef->EnsureCompletion();
		delete directorThreadRef;
		directorThreadRef = nullptr;
	}
	Super::BeginDestroy();
}

// Called every frame
void AWorldDirectorNpcPRO::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void DirectorProThread::WanderingNPCinBackground(FNpcStructPro& npcStruct)
{
	FVector npcLoc_ = npcStruct.npcLocation;
	FVector targetLoc_ = npcStruct.npcTargetLocation;

	if (npcStruct.pathPoints.Num() == 0 && npcStruct.bIsNeedFindWaypoint)
	{
		if (UKismetMathLibrary::EqualEqual_VectorVector(FVector(npcLoc_.X, npcLoc_.Y, 0.f), FVector(targetLoc_.X, targetLoc_.Y, 0.f), 10.f))
		{
			if (npcStruct.npcData.bIsWander && npcStruct.npcData.bIsPawnClass)
			{
				if (!IsGarbageCollecting())
				{
					if (IsValid(navigationSystemTH))
					{
						FNavLocation resultLocation_;
						if (navigationSystemTH->GetRandomPointInNavigableRadius(npcStruct.npcSpawnLocation, npcStruct.npcData.wanderRadius, resultLocation_))
						{
							if (directorParametersTH.walkingType == EWalkingType::EDITOR)
							{
								if (resultLocation_.Location != FVector::ZeroVector && !lookNavPathNpcArrTH.Contains(npcStruct))
								{
									npcStruct.randomLocation = resultLocation_;
									lookNavPathNpcArrTH.Add(npcStruct);
								}
							}
							else if (directorParametersTH.walkingType == EWalkingType::SHIPPING)
							{
								if (npcStruct.bIsNeedFindWaypoint)
								{
									bool bSelectRoad_(false);
									TArray<FVector> allRoadPathArr_;
									TArray<int> nearPathIndexArr_;
									bool bSelectInterestingPlace_(false);

									// Look interesting places.
									if (npcStruct.npcData.bUseInterestingPlaces)
									{
										for (int plc_ = 0; plc_ < interestingPlacesArrTH.Num(); ++plc_)
										{
											if (FMath::RandRange(0.f, 100.f) <= interestingPlacesArrTH[plc_]->chanceAttractAttention)
											{
												if ((interestingPlacesArrTH[plc_]->GetActorLocation() - npcStruct.npcLocation).SizeSquared() <= interestingPlacesArrTH[plc_]->findRadiusSquare)
												{
													FNavLocation randomPoint_;
													navigationSystemTH->GetRandomPointInNavigableRadius(interestingPlacesArrTH[plc_]->GetActorLocation(), interestingPlacesArrTH[plc_]->walkingRadius, randomPoint_);
													if (randomPoint_.NodeRef)
													{
														UNavigationPath* resultPath_ = navigationSystemTH->FindPathToLocationSynchronously(ownerActor->GetWorld(), npcStruct.npcLocation, randomPoint_.Location);
														if (resultPath_)
														{
															if (resultPath_->IsValid())
															{
																npcStruct.pathPoints = resultPath_->PathPoints;
																npcStruct.bIsNeedFindWaypoint = false;
																bSelectInterestingPlace_ = true;
															}
														}
													}
												}
											}
										}
									}

									if (npcStruct.npcData.bUseRoads && !bSelectInterestingPlace_)
									{
										for (int pp_ = 0; pp_ < npcPathArrTH.Num(); ++pp_)
										{
											if (FMath::RandRange(0.f, 100.f) <= npcPathArrTH[pp_]->chanceAttractAttention)
											{
												// Search road by tag.
												if (npcStruct.npcData.accessibleRoadsArr.Num() > 0)
												{
													bool hasTag_(false);
													for (int tagID_ = 0; tagID_ < npcStruct.npcData.accessibleRoadsArr.Num(); ++tagID_)
													{
														if (npcPathArrTH[pp_]->ActorHasTag(npcStruct.npcData.accessibleRoadsArr[tagID_]))
														{
															hasTag_ = true;
															break;
														}
													}
													if (!hasTag_)
													{
														continue;
													}
												}

												for (int pID_ = 0; pID_ < npcPathArrTH[pp_]->pathPointsArr.Num(); ++pID_)
												{
													if ((npcStruct.npcLocation - npcPathArrTH[pp_]->pathPointsArr[pID_]).SizeSquared() < npcStruct.npcData.maxDistanceSearchRoadSquare)
													{
														nearPathIndexArr_.Add(pp_);
														break;
													}
												}
											}
										}

										if (nearPathIndexArr_.Num() > 0)
										{
											int randWayIndex_(nearPathIndexArr_[FMath::RandRange(0, nearPathIndexArr_.Num() - 1)]);
											float minDistance_(9999999999.f);
											int minDistID_(-1);

											for (int pointID_ = 0; pointID_ < npcPathArrTH[randWayIndex_]->pathPointsArr.Num(); ++pointID_)
											{
												float distance_ = (npcStruct.npcLocation - npcPathArrTH[randWayIndex_]->pathPointsArr[pointID_]).SizeSquared();
												if (distance_ < minDistance_)
												{
													minDistance_ = distance_;
													minDistID_ = pointID_;
												}
											}

											bool bAllDirection_(false);
											bool bForwardDirection_(false);
											bool bRearDirection_(false);
											if (npcPathArrTH[randWayIndex_]->pathPointsArr.IsValidIndex(minDistID_ + 3))
											{
												bForwardDirection_ = true;
											}
											if (npcPathArrTH[randWayIndex_]->pathPointsArr.IsValidIndex(minDistID_ - 3))
											{
												bRearDirection_ = true;
											}

											if (bForwardDirection_ && bRearDirection_)
											{
												bAllDirection_ = true;
											}

											if (bAllDirection_)
											{
												bForwardDirection_ = FMath::RandBool();
											}

											int randWalkPoints_ = FMath::RandRange(3, npcPathArrTH[randWayIndex_]->maxWalkingPoints);

											if (bForwardDirection_)
											{
												for (int point_ = minDistID_; point_ < minDistID_ + randWalkPoints_; ++point_)
												{
													if (npcPathArrTH[randWayIndex_]->pathPointsArr.IsValidIndex(point_))
													{
														FNavLocation randomPoint_;
														navigationSystemTH->GetRandomPointInNavigableRadius(npcPathArrTH[randWayIndex_]->pathPointsArr[point_], npcPathArrTH[randWayIndex_]->radiusRandomPointNearPoint, randomPoint_);
														if (randomPoint_.NodeRef)
														{
															allRoadPathArr_.Add(randomPoint_.Location);
														}
													}
													else
													{
														break;
													}
												}


												bSelectRoad_ = true;
											}
											else if (bRearDirection_)
											{
												for (int point_ = minDistID_ - 3; point_ >= 0; --point_)
												{
													if (npcPathArrTH[randWayIndex_]->pathPointsArr.IsValidIndex(point_))
													{
														FNavLocation randomPoint_;
														navigationSystemTH->GetRandomPointInNavigableRadius(npcPathArrTH[randWayIndex_]->pathPointsArr[point_], npcPathArrTH[randWayIndex_]->radiusRandomPointNearPoint, randomPoint_);
														if (randomPoint_.NodeRef)
														{
															allRoadPathArr_.Add(randomPoint_.Location);
														}
													}
													else
													{
														break;
													}
												}
												bSelectRoad_ = true;
											}

											if (bSelectRoad_)
											{
												if (allRoadPathArr_.Num() > 0)
												{
													UNavigationPath* resultPath_ = navigationSystemTH->FindPathToLocationSynchronously(ownerActor->GetWorld(), npcStruct.npcLocation, allRoadPathArr_[0]);
													if (resultPath_)
													{
														npcStruct.pathPoints = resultPath_->PathPoints;
														allRoadPathArr_.RemoveAt(0);
														npcStruct.pathPoints.Append(allRoadPathArr_);
														npcStruct.bIsNeedFindWaypoint = false;
													}
												}
											}
										}
										else if (!bSelectRoad_)
										{
											UNavigationPath* resultPath_ = navigationSystemTH->FindPathToLocationSynchronously(ownerActor->GetWorld(), npcStruct.npcLocation, npcStruct.randomLocation);
											if (resultPath_)
											{
												if (resultPath_->IsValid())
												{
													npcStruct.pathPoints = resultPath_->PathPoints;
													npcStruct.bIsNeedFindWaypoint = false;
												}
											}
										}
									}
									else if (!bSelectInterestingPlace_)
									{
										UNavigationPath* resultPath_ = navigationSystemTH->FindPathToLocationSynchronously(ownerActor->GetWorld(), npcStruct.npcLocation, npcStruct.randomLocation);
										if (resultPath_)
										{
											if (resultPath_->IsValid())
											{
												npcStruct.pathPoints = resultPath_->PathPoints;
												npcStruct.bIsNeedFindWaypoint = false;
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

void DirectorProThread::WanderNPCinBackground(FNpcStructPro& npcStruct, const float& inDeltaTime)
{
	FVector npcLoc_ = npcStruct.npcLocation;
	FVector targetLoc_ = npcStruct.npcTargetLocation;
	float errorTolerance_ = 10.f;

	if (npcStruct.pathPoints.Num() > 0)
	{
		targetLoc_ = npcStruct.pathPoints[0];
		targetLoc_.Z += 80.f; //npcStruct.npcData.correctSpawnAxisZ;
		npcStruct.npcTargetLocation = targetLoc_;


		if (UKismetMathLibrary::EqualEqual_VectorVector(FVector(npcLoc_.X, npcLoc_.Y, 0.f), FVector(targetLoc_.X, targetLoc_.Y, 0.f), errorTolerance_))
		{
			npcStruct.pathPoints.RemoveAt(0);
		}
	}

	// Move Ghost NPC.
	npcStruct.npcLocation = FMath::VInterpConstantTo(npcStruct.npcLocation, targetLoc_, inDeltaTime, npcStruct.npcData.npcSpeed);

	if (!npcStruct.npcLocation.Equals(targetLoc_, 0.0001f))
	{
		npcStruct.npcRotation = UKismetMathLibrary::FindLookAtRotation(npcStruct.npcLocation, targetLoc_);
	}
	
	if (UKismetMathLibrary::EqualEqual_VectorVector(FVector(npcLoc_.X, npcLoc_.Y, 0.f),
	                                                FVector(targetLoc_.X, targetLoc_.Y, 0.f), errorTolerance_) && npcStruct.pathPoints.Num() == 0)
	{
		npcStruct.bIsNeedFindWaypoint = true;
	}
}

void AWorldDirectorNpcPRO::RegisterInstanceStaticMeshComponent(FNpcStructPro& npcStruct)
{
	// Register Instanced Static Mesh Component.
	if (npcStruct.npcData.staticMesh)
	{
		bool bCanRegisterInstSM_(true);
		int instanceID_(0);
		for (; instanceID_ < simulateStaticMeshComponentsArr.Num(); ++instanceID_)
		{
			if (simulateStaticMeshComponentsArr[instanceID_]->GetStaticMesh() == npcStruct.npcData.staticMesh)
			{
				bCanRegisterInstSM_ = false;
				break;
			}
		}

		if (bCanRegisterInstSM_)
		{
			UInstancedStaticMeshComponent* newInstSMComp_ = NewObject<UInstancedStaticMeshComponent>(this, UInstancedStaticMeshComponent::StaticClass());
			if (IsValid(newInstSMComp_))
			{
				newInstSMComp_->RegisterComponent();
				newInstSMComp_->SetStaticMesh(npcStruct.npcData.staticMesh);
				newInstSMComp_->SetCastShadow(directorParameters.bInstanceCastShadows);

				FTransform instanceTransform_;
				instanceTransform_.SetScale3D(npcStruct.npcScale);
				instanceTransform_.SetLocation(npcStruct.npcLocation);
				instanceTransform_.SetRotation((npcStruct.npcRotation + npcStruct.npcData.pivotOffsetRotation).Quaternion());
				newInstSMComp_->AddInstance(instanceTransform_, true);
				
				simulateStaticMeshComponentsArr.Add(newInstSMComp_);
			}
		}
		else
		{
			FTransform instanceTransform_;
			instanceTransform_.SetScale3D(npcStruct.npcScale);
			instanceTransform_.SetLocation(npcStruct.npcLocation);
			instanceTransform_.SetRotation((npcStruct.npcRotation + npcStruct.npcData.pivotOffsetRotation).Quaternion());
			
			simulateStaticMeshComponentsArr[instanceID_]->AddInstance(instanceTransform_, true);
		}
	}
}

void AWorldDirectorNpcPRO::UpdateInstanceSimulate(const FVector& plrDistance)
{
	if (directorParameters.maxDistanceShowSimulation > 0.f)
	{
		for (int compID_ = 0; compID_ < simulateStaticMeshComponentsArr.Num(); ++compID_)
		{
			int instance_(0);
			for (int instID_ = simulateStaticMeshComponentsArr[compID_]->GetInstanceCount() - 1; instID_ >= 0; --instID_)
			{
				FTransform instTransform_;
				simulateStaticMeshComponentsArr[compID_]->GetInstanceTransform(instID_, instTransform_, true);
				float playerDistance_ = (instTransform_.GetLocation() - plrDistance).SizeSquared();

				if (playerDistance_ > directorParameters.maxDistanceShowSimulationSquare)
				{
					simulateStaticMeshComponentsArr[compID_]->RemoveInstance(instID_);
					continue;
				}
			}

			int npcCount_(0);
			for (int npcID_ = 0; npcID_ < allThreadNPCArr_Debug.Num(); ++npcID_)
			{
				if (allThreadNPCArr_Debug[npcID_].npcData.staticMesh == simulateStaticMeshComponentsArr[compID_]->GetStaticMesh())
				{
					float playerDistance_ = (allThreadNPCArr_Debug[npcID_].npcLocation - plrDistance).SizeSquared();
					if (playerDistance_ <= directorParameters.maxDistanceShowSimulationSquare)
					{
						++npcCount_;
					}
				}
			}

			if (npcCount_ > simulateStaticMeshComponentsArr[compID_]->GetInstanceCount())
			{
				int differentNPC_ = npcCount_ - simulateStaticMeshComponentsArr[compID_]->GetInstanceCount();
				for (int i = 0; i < differentNPC_; ++i)
				{
					FTransform newInstance_(FTransform::Identity);
					simulateStaticMeshComponentsArr[compID_]->AddInstance(newInstance_, false);
				}
			}
			
			for (int npcID_ = 0; npcID_ < allThreadNPCArr_Debug.Num(); ++npcID_)
			{
				if (allThreadNPCArr_Debug[npcID_].npcData.staticMesh == simulateStaticMeshComponentsArr[compID_]->GetStaticMesh())
				{
					float playerDistance_ = (allThreadNPCArr_Debug[npcID_].npcLocation - plrDistance).SizeSquared();
					if (playerDistance_ <= directorParameters.maxDistanceShowSimulationSquare)
					{
						FTransform updateTransform_;
						updateTransform_.SetScale3D(allThreadNPCArr_Debug[npcID_].npcScale);
						updateTransform_.SetLocation(allThreadNPCArr_Debug[npcID_].npcLocation + allThreadNPCArr_Debug[npcID_].npcData.pivotOffsetLocation);

						FRotator npcRot(FRotator::ZeroRotator);
						if (!allThreadNPCArr_Debug[npcID_].npcLocation.Equals(allThreadNPCArr_Debug[npcID_].npcTargetLocation, 0.f))
						{
							npcRot = UKismetMathLibrary::FindLookAtRotation(allThreadNPCArr_Debug[npcID_].npcLocation, allThreadNPCArr_Debug[npcID_].npcTargetLocation);
						}
						else
						{
							npcRot = allThreadNPCArr_Debug[npcID_].npcRotation;
						}
						npcRot += allThreadNPCArr_Debug[npcID_].npcData.pivotOffsetRotation;
						updateTransform_.SetRotation(npcRot.Quaternion());
						simulateStaticMeshComponentsArr[compID_]->UpdateInstanceTransform(instance_, updateTransform_, true, false);
						instance_++;
					}
				}
			}

			simulateStaticMeshComponentsArr[compID_]->MarkRenderStateDirty();
		}
	}
	else
	{
		for (int instID_ = 0; instID_ < simulateStaticMeshComponentsArr.Num(); ++instID_)
		{
			int instance_(0);
			for (int npcID_ = 0; npcID_ < allThreadNPCArr_Debug.Num(); ++npcID_)
			{
				if (allThreadNPCArr_Debug[npcID_].npcData.staticMesh == simulateStaticMeshComponentsArr[instID_]->GetStaticMesh())
				{
					FTransform updateTransform_;
					updateTransform_.SetScale3D(allThreadNPCArr_Debug[npcID_].npcScale);
					updateTransform_.SetLocation(allThreadNPCArr_Debug[npcID_].npcLocation + allThreadNPCArr_Debug[npcID_].npcData.pivotOffsetLocation);
		
					FRotator npcRot(FRotator::ZeroRotator);
					if (!allThreadNPCArr_Debug[npcID_].npcLocation.Equals(allThreadNPCArr_Debug[npcID_].npcTargetLocation, 0.f))
					{
						npcRot = UKismetMathLibrary::FindLookAtRotation(allThreadNPCArr_Debug[npcID_].npcLocation, allThreadNPCArr_Debug[npcID_].npcTargetLocation);
					}
					else
					{
						npcRot = allThreadNPCArr_Debug[npcID_].npcRotation;
					}
					npcRot += allThreadNPCArr_Debug[npcID_].npcData.pivotOffsetRotation;
					
					updateTransform_.SetRotation(npcRot.Quaternion());
		
					simulateStaticMeshComponentsArr[instID_]->UpdateInstanceTransform(instance_, updateTransform_, true, false);
					instance_++;
				}
			}
			simulateStaticMeshComponentsArr[instID_]->MarkRenderStateDirty();
		}
	}
}

void AWorldDirectorNpcPRO::RemoveInstance(UNpcDataProComponent* actorComp_)
{
	for (int i = simulateStaticMeshComponentsArr.Num() - 1; i >= 0; --i)
	{
		if (simulateStaticMeshComponentsArr[i]->GetStaticMesh() == actorComp_->npcData.staticMesh)
		{
			if (simulateStaticMeshComponentsArr[i]->GetInstanceCount() > 0)
			{
				simulateStaticMeshComponentsArr[i]->RemoveInstance(simulateStaticMeshComponentsArr[i]->GetInstanceCount() - 1);
			}

			if (simulateStaticMeshComponentsArr[i]->GetInstanceCount() == 0)
			{
				simulateStaticMeshComponentsArr[i]->DestroyComponent();
				simulateStaticMeshComponentsArr.RemoveAt(i);
			}

			break;
		}
	}
}

void AWorldDirectorNpcPRO::InsertNPCinBackground(AActor* setNPC)
{
	if (IsValid(setNPC))
	{
		bool bPopulationControlSupport_(false);
		FNpcStructPro newNPC_;
		newNPC_.npcLocation = setNPC->GetActorLocation();
		newNPC_.npcRotation = setNPC->GetActorRotation();
		newNPC_.classNpc_ = setNPC->GetClass();
		newNPC_.npcScale = setNPC->GetActorScale3D();

		UNpcDataProComponent* actorComp_ = Cast<UNpcDataProComponent>(setNPC->FindComponentByClass(UNpcDataProComponent::StaticClass()));
		if (actorComp_)
		{
			newNPC_.npcData = actorComp_->npcData;
			newNPC_.npcSpawnLocation = actorComp_->npcSpawnLocation;

			actorComp_->BroadcastOnPrepareForOptimization();

			// Save uniq name npc.
			if (actorComp_->GetNpcUniqName() == "")
			{
				newNPC_.npcUniqName = setNPC->GetName();
			}
			else
			{
				newNPC_.npcUniqName = actorComp_->GetNpcUniqName();
			}

			bPopulationControlSupport_ = actorComp_->bPopulationControlSupport;
		}

		newNPC_.npcTargetLocation = newNPC_.npcLocation;

		if (directorParameters.bUseInstanceSimulate)
		{
			RegisterInstanceStaticMeshComponent(newNPC_);
		}

		// Add to background.
		backgroundNPCArr.Add(newNPC_);

		// Add to array for find npc struct in BP.
		allNpcInBackgroundArr_ForBP.Add(newNPC_.npcUniqName);
		InsertNPCInBackground_BP(setNPC);

		if (!bPopulationControlSupport_)
		{
			// Destroy actor.
			setNPC->Destroy();
		}
	}
}

void AWorldDirectorNpcPRO::ExchangeInformationTimer()
{
	if (!bIsActivate)
	{
		return;
	}

	TArray<AActor*> allRegPawnsArrTemp_;
	TArray<APawn*> allPlayersFoundArr_;
	TArray<FVector> allPlayersLocationsArr_;
	FVector firstPlayerLoc_(FVector::ZeroVector);

	// Find all players.
	for (int i = 0; i < directorParameters.playersClassesArr.Num(); ++i)
	{
		TArray<AActor*> allActorsOfClassArr_;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), directorParameters.playersClassesArr[i], allActorsOfClassArr_);

		for (int pawnID_ = 0; pawnID_ < allActorsOfClassArr_.Num(); ++pawnID_)
		{
			APawn* newPlayer_ = Cast<APawn>(allActorsOfClassArr_[pawnID_]);
			if (newPlayer_)
			{
				if (pawnID_ == 0)
				{
					firstPlayerLoc_ = newPlayer_->GetActorLocation();
				}

				if (!allPlayersFoundArr_.Contains(newPlayer_))
				{
					allPlayersFoundArr_.AddUnique(newPlayer_);
					allPlayersLocationsArr_.Add(newPlayer_->GetActorLocation());
				}
			}
		}
	}

	if (allPlayersFoundArr_.Num() == 0)
	{
		return;
	}

	// Move far NPC to Background.
	for (int registerNpcID_ = allRegisteredNpcArr.Num() - 1; registerNpcID_ >= 0; --registerNpcID_)
	{
		if (!IsValid(allRegisteredNpcArr[registerNpcID_]))
		{
			allRegisteredNpcArr.RemoveAt(registerNpcID_);			
			continue;
		}
		
		bool bIsCanSetToBackground_(true);

		for (int playerID_ = 0; playerID_ < allPlayersLocationsArr_.Num(); ++playerID_)
		{
			FVector playerLocation_ = allPlayersLocationsArr_[playerID_];

			UNpcDataProComponent* actorComp_ = Cast<UNpcDataProComponent>(allRegisteredNpcArr[registerNpcID_]->FindComponentByClass(UNpcDataProComponent::StaticClass()));
			if (actorComp_)
			{
				// Check override layers in component.
				if (actorComp_->npcData.bIsOverrideLayers)
				{
					if ((allRegisteredNpcArr[registerNpcID_]->GetActorLocation() - playerLocation_).SizeSquared() < actorComp_->npcData.mainLayerRadiusSquare)
					{
						bIsCanSetToBackground_ = false;
					}
				}
				else
				{
					if ((allRegisteredNpcArr[registerNpcID_]->GetActorLocation() - playerLocation_).SizeSquared() < directorParameters.mainLayerRadiusSquare)
					{
						bIsCanSetToBackground_ = false;
					}
				}
			}
		}

		if (bIsCanSetToBackground_ && IsValid(allRegisteredNpcArr[registerNpcID_]))
		{
			InsertNPCinBackground(allRegisteredNpcArr[registerNpcID_]);
		}
		else if (IsValid(allRegisteredNpcArr[registerNpcID_]))
		{
			allRegPawnsArrTemp_.Add(allRegisteredNpcArr[registerNpcID_]);
		}
	}

	allRegisteredNpcArr = allRegPawnsArrTemp_;

	// Get NPC Data.
	TArray<FNpcStructPro> restoreNPCArr_ = directorThreadRef->UpdateData(backgroundNPCArr, allPlayersLocationsArr_, allThreadNPCArr_Debug, npcPathArr, interestingPlacesArr);

	if (directorParameters.walkingType == EWalkingType::EDITOR && IsValid(navSystem))
	{
		TArray<FNpcStructPro> lookNavPathNpcArr_;

		directorThreadRef->GetData(lookNavPathNpcArr_);

		for (int npc_ = 0; npc_ < lookNavPathNpcArr_.Num(); ++npc_)
		{
			if (lookNavPathNpcArr_[npc_].bIsNeedFindWaypoint)
			{
				bool bSelectRoad_(false);
				TArray<FVector> allRoadPathArr_;
				TArray<int> nearPathIndexArr_;
				bool bSelectInterestingPlace_(false);

				// Look interesting places.
				if (lookNavPathNpcArr_[npc_].npcData.bUseInterestingPlaces)
				{
					for (int plc_ = interestingPlacesArr.Num() - 1; plc_ >= 0; --plc_)
					{
						if (!IsValid(interestingPlacesArr[plc_]))
						{
							interestingPlacesArr.RemoveAt(plc_);
							continue;
						}

						if (FMath::RandRange(0.f, 100.f) <= interestingPlacesArr[plc_]->chanceAttractAttention)
						{
							if ((interestingPlacesArr[plc_]->GetActorLocation() - lookNavPathNpcArr_[npc_].npcLocation).SizeSquared() <= interestingPlacesArr[plc_]->findRadiusSquare)
							{
								FNavLocation randomPoint_;
								navSystem->GetRandomPointInNavigableRadius(interestingPlacesArr[plc_]->GetActorLocation(), interestingPlacesArr[plc_]->walkingRadius, randomPoint_);
								if (randomPoint_.NodeRef)
								{
									UNavigationPath* resultPath_ = navSystem->FindPathToLocationSynchronously(GetWorld(), lookNavPathNpcArr_[npc_].npcLocation, randomPoint_.Location);
									if (resultPath_)
									{
										if (resultPath_->IsValid())
										{
											lookNavPathNpcArr_[npc_].pathPoints = resultPath_->PathPoints;
											lookNavPathNpcArr_[npc_].bIsNeedFindWaypoint = false;
											bSelectInterestingPlace_ = true;
										}
									}
								}
							}
						}
					}
				}

				if (lookNavPathNpcArr_[npc_].npcData.bUseRoads && !bSelectInterestingPlace_)
				{
					for (int pp_ = npcPathArr.Num() - 1; pp_ >= 0; --pp_)
					{
						if (!IsValid(npcPathArr[pp_]))
						{
							npcPathArr.RemoveAt(pp_);
							continue;
						}

						if (FMath::RandRange(0.f, 100.f) <= npcPathArr[pp_]->chanceAttractAttention)
						{
							// Search road by tag.
							if (lookNavPathNpcArr_[npc_].npcData.accessibleRoadsArr.Num() > 0)
							{
								bool hasTag_(false);
								for (int tagID_ = 0; tagID_ < lookNavPathNpcArr_[npc_].npcData.accessibleRoadsArr.Num(); ++tagID_)
								{
									if (npcPathArr[pp_]->ActorHasTag(lookNavPathNpcArr_[npc_].npcData.accessibleRoadsArr[tagID_]))
									{
										hasTag_ = true;
										break;
									}
								}
								if (!hasTag_)
								{
									continue;
								}
							}

							for (int pID_ = 0; pID_ < npcPathArr[pp_]->pathPointsArr.Num(); ++pID_)
							{
								if ((lookNavPathNpcArr_[npc_].npcLocation - npcPathArr[pp_]->pathPointsArr[pID_]).SizeSquared() < lookNavPathNpcArr_[npc_].npcData.maxDistanceSearchRoadSquare)
								{
									nearPathIndexArr_.Add(pp_);
									break;
								}
							}
						}
					}

					if (nearPathIndexArr_.Num() > 0)
					{
						int randWayIndex_(nearPathIndexArr_[FMath::RandRange(0, nearPathIndexArr_.Num() - 1)]);
						float minDistance_(9999999999.f);
						int minDistID_(-1);

						for (int pointID_ = 0; pointID_ < npcPathArr[randWayIndex_]->pathPointsArr.Num(); ++pointID_)
						{
							float distance_ = (lookNavPathNpcArr_[npc_].npcLocation - npcPathArr[randWayIndex_]->pathPointsArr[pointID_]).SizeSquared();
							if (distance_ < minDistance_)
							{
								minDistance_ = distance_;
								minDistID_ = pointID_;
							}
						}

						bool bAllDirection_(false);
						bool bForwardDirection_(false);
						bool bRearDirection_(false);
						if (npcPathArr[randWayIndex_]->pathPointsArr.IsValidIndex(minDistID_ + 3))
						{
							bForwardDirection_ = true;
						}
						if (npcPathArr[randWayIndex_]->pathPointsArr.IsValidIndex(minDistID_ - 3))
						{
							bRearDirection_ = true;
						}

						if (bForwardDirection_ && bRearDirection_)
						{
							bAllDirection_ = true;
						}

						if (bAllDirection_)
						{
							bForwardDirection_ = FMath::RandBool();
						}

						int randWalkPoints_ = FMath::RandRange(3, npcPathArr[randWayIndex_]->maxWalkingPoints);

						if (bForwardDirection_)
						{
							for (int point_ = minDistID_; point_ < minDistID_ + randWalkPoints_; ++point_)
							{
								if (npcPathArr[randWayIndex_]->pathPointsArr.IsValidIndex(point_))
								{
									FNavLocation randomPoint_;
									navSystem->GetRandomPointInNavigableRadius(npcPathArr[randWayIndex_]->pathPointsArr[point_], npcPathArr[randWayIndex_]->radiusRandomPointNearPoint, randomPoint_);
									if (randomPoint_.NodeRef)
									{
										allRoadPathArr_.Add(randomPoint_.Location);
									}
								}
								else
								{
									break;
								}
							}


							bSelectRoad_ = true;
						}
						else if (bRearDirection_)
						{
							for (int point_ = minDistID_ - 3; point_ >= 0; --point_)
							{
								if (npcPathArr[randWayIndex_]->pathPointsArr.IsValidIndex(point_))
								{
									FNavLocation randomPoint_;
									navSystem->GetRandomPointInNavigableRadius(npcPathArr[randWayIndex_]->pathPointsArr[point_], npcPathArr[randWayIndex_]->radiusRandomPointNearPoint, randomPoint_);
									if (randomPoint_.NodeRef)
									{
										allRoadPathArr_.Add(randomPoint_.Location);
									}
								}
								else
								{
									break;
								}
							}
							bSelectRoad_ = true;
						}

						if (bSelectRoad_)
						{
							if (allRoadPathArr_.Num() > 0)
							{
								UNavigationPath* resultPath_ = navSystem->FindPathToLocationSynchronously(GetWorld(), lookNavPathNpcArr_[npc_].npcLocation, allRoadPathArr_[0]);
								if (resultPath_)
								{
									lookNavPathNpcArr_[npc_].pathPoints = resultPath_->PathPoints;
									allRoadPathArr_.RemoveAt(0);
									lookNavPathNpcArr_[npc_].pathPoints.Append(allRoadPathArr_);
									lookNavPathNpcArr_[npc_].bIsNeedFindWaypoint = false;
								}
							}
						}
					}
					else if (!bSelectRoad_)
					{
						UNavigationPath* resultPath_ = navSystem->FindPathToLocationSynchronously(GetWorld(), lookNavPathNpcArr_[npc_].npcLocation, lookNavPathNpcArr_[npc_].randomLocation);
						if (resultPath_)
						{
							if (resultPath_->IsValid())
							{
								lookNavPathNpcArr_[npc_].pathPoints = resultPath_->PathPoints;
								lookNavPathNpcArr_[npc_].bIsNeedFindWaypoint = false;
							}
						}
					}
				}
				else if (!bSelectInterestingPlace_)
				{
					UNavigationPath* resultPath_ = navSystem->FindPathToLocationSynchronously(GetWorld(), lookNavPathNpcArr_[npc_].npcLocation, lookNavPathNpcArr_[npc_].randomLocation);
					if (resultPath_)
					{
						if (resultPath_->IsValid())
						{
							lookNavPathNpcArr_[npc_].pathPoints = resultPath_->PathPoints;
							lookNavPathNpcArr_[npc_].bIsNeedFindWaypoint = false;
						}
					}
				}
			}
		}

		if (lookNavPathNpcArr_.Num() > 0)
		{
			directorThreadRef->UpdatePathForNpc(lookNavPathNpcArr_);
		}
		else
		{
			directorThreadRef->UnlockThread();
		}
	}
	else
	{
		directorThreadRef->UnlockThread();
	}


	npcInBackgroundDebug = allThreadNPCArr_Debug.Num();

	if (bIsDebug)
	{
		if (StaticMeshInstanceComponent->GetInstanceCount() != allThreadNPCArr_Debug.Num())
		{
			StaticMeshInstanceComponent->ClearInstances();
			for (int i = 0; i < allThreadNPCArr_Debug.Num(); ++i)
			{
				FTransform newInstanceTransform_;
				newInstanceTransform_.SetLocation(allThreadNPCArr_Debug[i].npcLocation);
				StaticMeshInstanceComponent->AddInstance(newInstanceTransform_, true);
			}
		}
		else
		{
			for (int i = 0; i < allThreadNPCArr_Debug.Num(); ++i)
			{
				FTransform newInstanceTransform_;
				newInstanceTransform_.SetLocation(allThreadNPCArr_Debug[i].npcLocation);

				StaticMeshInstanceComponent->UpdateInstanceTransform(i, newInstanceTransform_, true, false);
			}
		}
	}
	else
	{
		if (StaticMeshInstanceComponent->GetInstanceCount() > 0)
		{
			StaticMeshInstanceComponent->ClearInstances();
		}
	}

	if (bIsDebug)
	{
		StaticMeshInstanceComponent->MarkRenderStateDirty();
	}

	if (directorParameters.bUseInstanceSimulate)
	{
		UpdateInstanceSimulate(firstPlayerLoc_);
	}

	backgroundNPCArr.Empty();

	for (int restoreID_ = 0; restoreID_ < restoreNPCArr_.Num(); ++restoreID_)
	{
		FActorSpawnParameters spawnInfo_;
		spawnInfo_.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		FRotator spawnRotation_(FRotator::ZeroRotator);

		// Set random rotation.
		if (restoreNPCArr_[restoreID_].npcData.bRandomPitchRotationOnSpawn || restoreNPCArr_[restoreID_].npcData.bRandomRollRotationOnSpawn || restoreNPCArr_[restoreID_].npcData.bRandomYawRotationOnSpawn)
		{
			if (restoreNPCArr_[restoreID_].npcData.bRandomYawRotationOnSpawn)
			{
				spawnRotation_.Yaw = FMath::RandRange(0.f, 360.f);
			}
			if (restoreNPCArr_[restoreID_].npcData.bRandomPitchRotationOnSpawn)
			{
				spawnRotation_.Pitch = FMath::RandRange(0.f, 360.f);
			}
			if (restoreNPCArr_[restoreID_].npcData.bRandomRollRotationOnSpawn)
			{
				spawnRotation_.Roll = FMath::RandRange(0.f, 360.f);
			}
		}
		else // Restore origin rotation.
		{
			spawnRotation_ = restoreNPCArr_[restoreID_].npcRotation;
		}

		// Set transform.
		FTransform spawnTransform_;

		spawnTransform_.SetLocation(restoreNPCArr_[restoreID_].npcLocation);
		spawnTransform_.SetRotation(spawnRotation_.Quaternion());
		spawnTransform_.SetScale3D(restoreNPCArr_[restoreID_].npcScale);
		
		AActor* restoredNPC_ = GetWorld()->SpawnActor<AActor>(restoreNPCArr_[restoreID_].classNpc_, spawnTransform_, spawnInfo_);
		if (restoredNPC_)
		{
			// Fix actor scale.
			restoredNPC_->SetActorScale3D(restoreNPCArr_[restoreID_].npcScale);

			// Correct spawn Z location.
			if (Cast<APawn>(restoredNPC_))
			{
				restoreNPCArr_[restoreID_].npcLocation.Z += restoreNPCArr_[restoreID_].npcData.correctSpawnAxisZ;
				restoredNPC_->SetActorLocation(restoreNPCArr_[restoreID_].npcLocation);
			}

			UNpcDataProComponent* actorComp_ = Cast<UNpcDataProComponent>(restoredNPC_->FindComponentByClass(UNpcDataProComponent::StaticClass()));

			if (actorComp_)
			{
				if (directorParameters.bUseInstanceSimulate)
				{
					RemoveInstance(actorComp_);
				}

				actorComp_->npcData = restoreNPCArr_[restoreID_].npcData;
				actorComp_->npcSpawnLocation = restoreNPCArr_[restoreID_].npcSpawnLocation;
				// Restore uniq name of npc.
				actorComp_->SetNpcUniqName(restoreNPCArr_[restoreID_].npcUniqName);

				actorComp_->BroadcastOnRecoveryFromOptimization();

				for (int findID_ = 0; findID_ < allNpcInBackgroundArr_ForBP.Num(); ++findID_)
				{
					if (allNpcInBackgroundArr_ForBP[findID_] == restoreNPCArr_[restoreID_].npcUniqName)
					{
						RestoreNPC_BP(findID_, restoredNPC_);
						allNpcInBackgroundArr_ForBP.RemoveAt(findID_);
					}
				}
			}
		}
	}
}

uint32 DirectorProThread::Run()
{
	//Initial wait before starting
	FPlatformProcess::Sleep(0.5f);

	while (!m_Kill)
	{
		if (m_Pause)
		{
			//FEvent->Wait(); will "sleep" the thread until it will get a signal "Trigger()"
			m_semaphore->Wait();

			if (m_Kill)
			{
				return 0;
			}
		}
		else
		{
			threadDeltaTime = FTimespan(FPlatformTime::Cycles64() - lastFrameTime).GetTotalSeconds();
			lastFrameTime = FPlatformTime::Cycles64();

			m_mutex.Lock();

			TArray<FNpcStructPro> allBackgroundNPCArrTemp_ = allBackgroundNPCArrTH;
			TArray<FVector> playersLocationsArrTemp_ = playersLocationsArr;
			TArray<FNpcStructPro> instanceSimulateArrTemp_;

			allBackgroundNPCArrTH.Empty();

			if (lookNavPathNpcArrTH.Num() > 0 && updatePath)
			{
				for (int i = 0; i < allBackgroundNPCArrTemp_.Num(); ++i)
				{
					for (int g = 0; g < lookNavPathNpcArrTH.Num(); ++g)
					{
						if (allBackgroundNPCArrTemp_[i] == lookNavPathNpcArrTH[g])
						{
							allBackgroundNPCArrTemp_[i] = lookNavPathNpcArrTH[g];
						}
					}
				}

				lookNavPathNpcArrTH.Empty();
				updatePath = false;
			}


			TArray<FNpcStructPro> stayInBackgroundNPCArrMem_;
			TArray<FNpcStructPro> restoreNPCArr_;
			TArray<FNpcStructPro> nearCanMovableNPC_;
			TArray<FNpcStructPro> farCanMovableNPC_;

			m_mutex.Unlock();

			// Check distance between players and background NPC. And sort by layers.
			for (int checkNpcID_ = 0; checkNpcID_ < allBackgroundNPCArrTemp_.Num(); ++checkNpcID_)
			{
				bool sorted_(false);
				bool bIsSelectSecondLayer_(false);
				bool bIsSelectThirdLayer_(false);

				for (int playerLocID_ = 0; playerLocID_ < playersLocationsArrTemp_.Num(); ++playerLocID_)
				{
					float distanceToNpc_ = (allBackgroundNPCArrTemp_[checkNpcID_].npcLocation - playersLocationsArrTemp_[playerLocID_]).SizeSquared();

					if (directorParametersTH.bUseInstanceSimulate)
					{
						if (distanceToNpc_ <= directorParametersTH.maxDistanceShowSimulationSquare)
						{
							instanceSimulateArrTemp_.Add(allBackgroundNPCArrTemp_[checkNpcID_]);
						}
					}


					// If override control layers in component.
					if (allBackgroundNPCArrTemp_[checkNpcID_].npcData.bIsOverrideLayers)
					{
						// Check distance and sort by layers.
						if (distanceToNpc_ < allBackgroundNPCArrTemp_[checkNpcID_].npcData.firstLayerRadiusSquare)
						{
							restoreNPCArr_.Add(allBackgroundNPCArrTemp_[checkNpcID_]);
							sorted_ = true;
							break;
						}
						else if (distanceToNpc_ < allBackgroundNPCArrTemp_[checkNpcID_].npcData.secondLayerRadiusSquare)
						{
							bIsSelectSecondLayer_ = true;
						}
						else if (distanceToNpc_ < allBackgroundNPCArrTemp_[checkNpcID_].npcData.thirdLayerRadiusSquare)
						{
							bIsSelectThirdLayer_ = true;
						}
					}
					else
					{
						// Check distance and sort by layers.
						if (distanceToNpc_ < directorParametersTH.firstLayerRadiusSquare)
						{
							restoreNPCArr_.Add(allBackgroundNPCArrTemp_[checkNpcID_]);
							sorted_ = true;
							break;
						}
						else if (distanceToNpc_ < directorParametersTH.secondLayerRadiusSquare)
						{
							bIsSelectSecondLayer_ = true;
						}
						else if (distanceToNpc_ < directorParametersTH.thirdLayerRadiusSquare)
						{
							bIsSelectThirdLayer_ = true;
						}
					}
				}

				if (bIsSelectSecondLayer_ && !sorted_)
				{
					allBackgroundNPCArrTemp_[checkNpcID_].bIsCanMove = true;
					allBackgroundNPCArrTemp_[checkNpcID_].bIsNearNPC = true;
					nearCanMovableNPC_.Add(allBackgroundNPCArrTemp_[checkNpcID_]);
				}
				else if (bIsSelectThirdLayer_ && !sorted_)
				{
					allBackgroundNPCArrTemp_[checkNpcID_].bIsCanMove = true;
					allBackgroundNPCArrTemp_[checkNpcID_].bIsNearNPC = false;
					farCanMovableNPC_.Add(allBackgroundNPCArrTemp_[checkNpcID_]);
				}
				else if (!sorted_) // If not sorted by layers.
				{
					allBackgroundNPCArrTemp_[checkNpcID_].bIsCanMove = false;
					allBackgroundNPCArrTemp_[checkNpcID_].bIsNearNPC = false;
					stayInBackgroundNPCArrMem_.Add(allBackgroundNPCArrTemp_[checkNpcID_]);
				}
			}

			if (nearCanMovableNPC_.Num() > 0 || farCanMovableNPC_.Num() > 0)
			{
				if (nowFrame > skipFrameValue)
				{
					////////////////// NEAR LAYER /////////////////////
					if (minNearGetPathNpcID <= nearCanMovableNPC_.Num() - 1)
					{
						if (maxNearGetPathNpcID <= nearCanMovableNPC_.Num() - 1)
						{
							for (int nearMoveID_ = minNearGetPathNpcID; nearMoveID_ < maxNearGetPathNpcID; ++nearMoveID_)
							{
								WanderingNPCinBackground(nearCanMovableNPC_[nearMoveID_]);
							}

							minNearGetPathNpcID = maxNearGetPathNpcID;
							maxNearGetPathNpcID += incrementNearGetPathNpc;
						}
						else
						{
							for (int moveID = minNearGetPathNpcID; moveID < nearCanMovableNPC_.Num(); ++moveID)
							{
								WanderingNPCinBackground(nearCanMovableNPC_[moveID]);
							}

							minNearGetPathNpcID = 0;
							maxNearGetPathNpcID = incrementNearGetPathNpc;
						}
					}
					else
					{
						minNearGetPathNpcID = 0;
						maxNearGetPathNpcID = incrementNearGetPathNpc;
					}

					////////////////// FAR LAYER /////////////////////
					if (minFarGetPathNpcID <= farCanMovableNPC_.Num() - 1)
					{
						if (maxFarGetPathNpcID <= farCanMovableNPC_.Num() - 1)
						{
							for (int farMoveID_ = minFarGetPathNpcID; farMoveID_ < maxFarGetPathNpcID; ++farMoveID_)
							{
								WanderingNPCinBackground(farCanMovableNPC_[farMoveID_]);
							}

							minFarGetPathNpcID = maxFarGetPathNpcID;
							maxFarGetPathNpcID += incrementFarGetPathNpc;
						}
						else
						{
							for (int moveID = minFarGetPathNpcID; moveID < farCanMovableNPC_.Num(); ++moveID)
							{
								WanderingNPCinBackground(farCanMovableNPC_[moveID]);
							}

							minFarGetPathNpcID = 0;
							maxFarGetPathNpcID = incrementFarGetPathNpc;
						}
					}
					else
					{
						minFarGetPathNpcID = 0;
						maxFarGetPathNpcID = incrementFarGetPathNpc;
					}

					// Reset skip frame.
					nowFrame = 0;
				}
				else
				{
					++nowFrame;
				}
			}


			// Append all movable array for return data npc.
			stayInBackgroundNPCArrMem_.Append(nearCanMovableNPC_);
			stayInBackgroundNPCArrMem_.Append(farCanMovableNPC_);

			for (int moveID = 0; moveID < stayInBackgroundNPCArrMem_.Num(); ++moveID)
			{
				if (stayInBackgroundNPCArrMem_[moveID].bIsCanMove)
				{
					WanderNPCinBackground(stayInBackgroundNPCArrMem_[moveID], threadDeltaTime);
				}
			}

			m_mutex.Lock();

			// Save data.
			allBackgroundNPCArrTH.Append(stayInBackgroundNPCArrMem_);

			canRestoreNPCArrTH.Append(restoreNPCArr_);

			m_mutex.Unlock();

			threadDeltaTime += threadSleepTime;


			//A little sleep between the chunks (So CPU will rest a bit -- (may be omitted))
			FPlatformProcess::Sleep(threadSleepTime);
		}
	}
	return 0;
}

bool AWorldDirectorNpcPRO::RegisterNPC(AActor* actorRef)
{
	if (bIsActivate)
	{
		bool bIsReg_ = false;
		if (actorRef)
		{
			allRegisteredNpcArr.Add(actorRef);
			bIsReg_ = true;
		}
		return bIsReg_;
	}
	else
	{
		return false;
	}
}

int AWorldDirectorNpcPRO::GetBackgroundNpcCount() const
{
	return npcInBackgroundDebug;
}

bool AWorldDirectorNpcPRO::SaveState(FString slotNameString, int playerIndex)
{
	bool bCanSave_(false);

	if (wdSaveGame == nullptr)
	{
		wdSaveGame = Cast<USaveGameWdPRO>(UGameplayStatics::CreateSaveGameObject(USaveGameWdPRO::StaticClass()));
	}

	if (wdSaveGame)
	{
		bCanSave_ = true;
	}

	if (bCanSave_)
	{
		GetWorld()->GetTimerManager().PauseTimer(exchangeInformation_Timer);

		directorThreadRef->PauseThread();

		for (int i = 0; i < allRegisteredNpcArr.Num(); ++i)
		{
			if (IsValid(allRegisteredNpcArr[i]))
			{
				InsertNPCinBackground(allRegisteredNpcArr[i]);
			}
		}

		SaveStateBlueprintEvent();

		wdSaveGame->allNpcInBackgroundArr_ForBP = allNpcInBackgroundArr_ForBP;

		allRegisteredNpcArr.Empty();

		directorThreadRef->SaveThreadData(backgroundNPCArr, wdSaveGame->allBackgroundNPCArrTHSaved, wdSaveGame->canRestoreNPCArrTHSaved);

		backgroundNPCArr.Empty();

		directorThreadRef->ContinueThread();

		GetWorld()->GetTimerManager().UnPauseTimer(exchangeInformation_Timer);

		if (bIsDebug)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Saved State"));
		}

		UGameplayStatics::SaveGameToSlot(wdSaveGame, slotNameString, playerIndex);
	}
	else
	{
		if (bIsDebug)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Saved State"));
		}
	}

	return bCanSave_;
}

bool AWorldDirectorNpcPRO::LoadState(FString slotNameString, int playerIndex)
{
	bool bCanLoad_(false);

	if (wdSaveGame)
	{
		if (UGameplayStatics::DoesSaveGameExist(slotNameString, playerIndex))
		{
			bCanLoad_ = true;
		}
	}
	else
	{
		wdSaveGame = Cast<USaveGameWdPRO>(UGameplayStatics::LoadGameFromSlot(slotNameString, playerIndex));

		if (wdSaveGame)
		{
			if (UGameplayStatics::DoesSaveGameExist(slotNameString, playerIndex))
			{
				bCanLoad_ = true;
			}
		}
	}

	if (bCanLoad_)
	{
		allNpcInBackgroundArr_ForBP = wdSaveGame->allNpcInBackgroundArr_ForBP;

		LoadStateBlueprintEvent();

		GetWorld()->GetTimerManager().PauseTimer(exchangeInformation_Timer);

		directorThreadRef->PauseThread();

		for (int i = 0; i < allRegisteredNpcArr.Num(); ++i)
		{
			if (IsValid(allRegisteredNpcArr[i]))
			{
				allRegisteredNpcArr[i]->Destroy();
			}
		}
		allRegisteredNpcArr.Empty();

		directorThreadRef->LoadThreadData(wdSaveGame->allBackgroundNPCArrTHSaved, wdSaveGame->canRestoreNPCArrTHSaved);

		directorThreadRef->ContinueThread();

		GetWorld()->GetTimerManager().UnPauseTimer(exchangeInformation_Timer);

		if (bIsDebug)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Loaded State"));
		}
	}
	else
	{
		if (bIsDebug)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("ERROR - Save slot is not valid."));
		}
	}

	return bCanLoad_;
}

void AWorldDirectorNpcPRO::RemoveActorFormSystem(AActor* setActor)
{
	if (IsValid(setActor))
	{
		for (int i = allRegisteredNpcArr.Num() - 1; i >= 0; --i)
		{
			if (setActor == allRegisteredNpcArr[i])
			{
				allRegisteredNpcArr.RemoveAt(i);
				return;
			}
		}
	}
}

DirectorProThread::DirectorProThread(AActor* newActor, FDirectorProStruct setDirectorParameters)
{
	m_Kill = false;
	m_Pause = false;

	//Initialize FEvent (as a cross platform (Confirmed Mac/Windows))
	m_semaphore = FGenericPlatformProcess::GetSynchEventFromPool(false);

	ownerActor = newActor;
	directorParametersTH = setDirectorParameters;

	UNavigationSystemV1* navSystem_ = UNavigationSystemV1::GetNavigationSystem(ownerActor);
	if (IsValid(navSystem_))
	{
		navigationSystemTH = navSystem_;
	}


	Thread = FRunnableThread::Create(this, (TEXT("%s_FSomeRunnable"), *newActor->GetName()), 0, TPri_Lowest);
}

DirectorProThread::~DirectorProThread()
{
	if (m_semaphore)
	{
		//Cleanup the FEvent
		FGenericPlatformProcess::ReturnSynchEventToPool(m_semaphore);
		m_semaphore = nullptr;
	}

	if (Thread)
	{
		//Cleanup the worker thread
		delete Thread;
		Thread = nullptr;
	}
}

void DirectorProThread::EnsureCompletion()
{
	Stop();

	navigationSystemTH = nullptr;

	if (Thread)
	{
		Thread->WaitForCompletion();
	}
}

void DirectorProThread::PauseThread()
{
	m_Pause = true;
}

void DirectorProThread::ContinueThread()
{
	m_Pause = false;

	if (m_semaphore)
	{
		//Here is a FEvent signal "Trigger()" -> it will wake up the thread.
		m_semaphore->Trigger();
	}
}

bool DirectorProThread::Init()
{
	return true;
}

void DirectorProThread::Stop()
{
	m_Kill = true; //Thread kill condition "while (!m_Kill){...}"
	m_Pause = false;

	if (m_semaphore)
	{
		//We shall signal "Trigger" the FEvent (in case the Thread is sleeping it shall wake up!!)
		m_semaphore->Trigger();
	}
}

bool DirectorProThread::IsThreadPaused() const
{
	return (bool)m_Pause;
}

TArray<FNpcStructPro> DirectorProThread::UpdateData(const TArray<FNpcStructPro>& setNewBackgroundNPCArr, const TArray<FVector>& setPlayersLocationsArr, TArray<FNpcStructPro>& getAllThreadNPCArr, TArray<class ANpcPathPro*> npcPathArr,
                                                    TArray<class AInterestingPlacePro*> interestingPlacesArr)
{
	m_mutex.Lock();

	allBackgroundNPCArrTH.Append(setNewBackgroundNPCArr);

	getAllThreadNPCArr = allBackgroundNPCArrTH;

	playersLocationsArr = setPlayersLocationsArr;

	npcPathArrTH = npcPathArr;

	interestingPlacesArrTH = interestingPlacesArr;

	TArray<FNpcStructPro> canRestoreNPCTemp_ = canRestoreNPCArrTH;
	canRestoreNPCArrTH.Empty();

	//	m_mutex.Unlock();

	return canRestoreNPCTemp_;
}

void DirectorProThread::GetData(TArray<FNpcStructPro>& getLookNavPathNpc) const
{
	getLookNavPathNpc = lookNavPathNpcArrTH;
}

void DirectorProThread::UpdatePathForNpc(const TArray<FNpcStructPro>& navPathNpc)
{
	lookNavPathNpcArrTH = navPathNpc;

	updatePath = true;

	m_mutex.Unlock();
}

void DirectorProThread::UnlockThread()
{
	m_mutex.Unlock();
}

void DirectorProThread::SaveThreadData(const TArray<FNpcStructPro>& setNewBackgroundNPCArr, TArray<FNpcStructPro>& setBackgroundArr, TArray<FNpcStructPro>& setRestoreArr)
{
	allBackgroundNPCArrTH.Append(setNewBackgroundNPCArr);

	setBackgroundArr = allBackgroundNPCArrTH;
	setRestoreArr = canRestoreNPCArrTH;
}

void DirectorProThread::LoadThreadData(const TArray<FNpcStructPro>& setBackgroundArr, const TArray<FNpcStructPro>& setRestoreArr)
{
	allBackgroundNPCArrTH.Empty();
	canRestoreNPCArrTH.Empty();

	allBackgroundNPCArrTH = setBackgroundArr;
	canRestoreNPCArrTH = setRestoreArr;
}
