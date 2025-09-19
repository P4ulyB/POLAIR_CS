// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/PACS_NPCBase.h"
#include "Actors/PACS_SelectionCueProxy.h"
#include "Data/PACS_NPCArchetypeData.h"
#include "Data/PACS_SelectionGlobalConfig.h"
#include "Data/PACS_SelectionLocalConfig.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

APACS_NPCBase::APACS_NPCBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void APACS_NPCBase::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		if (!SelectionProxy.IsValid())
		{
			if (APACS_SelectionCueProxy* P = GetWorld()->SpawnActor<APACS_SelectionCueProxy>())
			{
				P->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
				P->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);

				P->GlobalCfg = SelectionGlobalConfig;
				P->LocalCfg  = SelectionLocalOverride;

				SelectionProxy = P;
			}
		}
	}

	ApplyArchetype();
}

void APACS_NPCBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clean up selection proxy
	if (SelectionProxy.IsValid())
	{
		SelectionProxy->Destroy();
		SelectionProxy = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void APACS_NPCBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// No selection state on NPC
}

void APACS_NPCBase::ApplyArchetype()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (Archetype && !Archetype.IsValid()) Archetype.LoadSynchronous();
#endif
	UPACS_NPCArchetypeData* Data = Archetype.Get();
	if (!Data) return;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (Data->SkeletalMesh && !Data->SkeletalMesh.IsValid()) Data->SkeletalMesh.LoadSynchronous();
	if (Data->AnimBPClass  && !Data->AnimBPClass.IsValid())  Data->AnimBPClass.LoadSynchronous();
	for (auto& KVP : Data->MaterialOverrides)
	{
		if (KVP.Value && !KVP.Value.IsValid()) KVP.Value.LoadSynchronous();
	}
#endif

	if (USkeletalMesh* SkeletalMeshAsset = Data->SkeletalMesh.Get())
	{
		GetMesh()->SetSkeletalMesh(SkeletalMeshAsset);
	}
	if (UClass* AnimClass = Data->AnimBPClass.Get())
	{
		GetMesh()->SetAnimInstanceClass(AnimClass);
	}
	for (const auto& KVP : Data->MaterialOverrides)
	{
		if (UMaterialInterface* MI = KVP.Value.Get())
		{
			GetMesh()->SetMaterial(KVP.Key, MI);
		}
	}
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = Data->WalkSpeed;
	}
}