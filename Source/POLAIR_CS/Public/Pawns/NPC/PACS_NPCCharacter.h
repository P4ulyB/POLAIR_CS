#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Data/PACS_NPCVisualConfig.h"
#include "PACS_NPCCharacter.generated.h"

class UPACS_NPCConfig;
struct FStreamableHandle;

UCLASS(BlueprintType, Blueprintable)
class POLAIR_CS_API APACS_NPCCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APACS_NPCCharacter();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC", meta = (DisplayName = "NPC Configuration"))
	TObjectPtr<UPACS_NPCConfig> NPCConfigAsset;

	UPROPERTY(ReplicatedUsing = OnRep_VisualConfig, meta = (RepNotifyCondition = "REPNOTIFY_OnChanged"))
	FPACS_NPCVisualConfig VisualConfig;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;

protected:
	UFUNCTION()
	void OnRep_VisualConfig();

	void ApplyVisuals_Client();
	void BuildVisualConfigFromAsset_Server();

private:
	bool bVisualsApplied = false;
	TSharedPtr<FStreamableHandle> AssetLoadHandle;
};