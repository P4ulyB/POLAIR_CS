// Copyright 2025 BitProtectStudio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorldDirectorNpcPRO.h"
#include "GameFramework/SaveGame.h"
#include "SaveGameWdPRO.generated.h"

/**
 * 
 */
UCLASS()
class WORLDDIRECTORPRO_API USaveGameWdPRO : public USaveGame
{
	GENERATED_BODY()

public:
	
	UPROPERTY()
	TArray<FNpcStructPro> allBackgroundNPCArrTHSaved;

	UPROPERTY()
	TArray<FNpcStructPro> canRestoreNPCArrTHSaved;

	UPROPERTY();
	TArray<FString> allNpcInBackgroundArr_ForBP;
};
