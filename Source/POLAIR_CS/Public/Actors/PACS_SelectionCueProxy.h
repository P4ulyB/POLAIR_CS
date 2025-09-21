#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PACS_SelectionCueProxy.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Selection cue proxy actor for hover and selection visual feedback
 * Epic pattern: Lightweight actor for visual state management
 * Handles local hover state without network traffic
 */
UCLASS(BlueprintType, Blueprintable)
class POLAIR_CS_API APACS_SelectionCueProxy : public AActor
{
	GENERATED_BODY()

public:
	APACS_SelectionCueProxy();

	// Local-only hover state management (no RPC)
	UFUNCTION(BlueprintCallable, Category="Selection")
	void SetLocalHovered(bool bHovered);

	UFUNCTION(BlueprintCallable, Category="Selection")
	bool IsCurrentlyHovered() const { return bIsCurrentlyHovered; }

	// Selection state (could be networked if needed later)
	UFUNCTION(BlueprintCallable, Category="Selection")
	void SetSelected(bool bSelected);

	UFUNCTION(BlueprintCallable, Category="Selection")
	bool IsCurrentlySelected() const { return bIsCurrentlySelected; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Visual feedback components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> HoverMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> SelectionMesh;

	// Material configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Materials")
	TObjectPtr<UMaterialInterface> HoverMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Materials")
	TObjectPtr<UMaterialInterface> SelectionMaterial;

	// Visual state colors
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual")
	FLinearColor HoverColor = FLinearColor::Yellow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual")
	FLinearColor SelectionColor = FLinearColor::Green;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual")
	float HoverOpacity = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual")
	float SelectionOpacity = 0.8f;

private:
	bool bIsCurrentlyHovered = false;
	bool bIsCurrentlySelected = false;

	// Dynamic material instances for runtime parameter control
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> HoverMaterialInstance;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SelectionMaterialInstance;

	// Visual state management
	void UpdateHoverVisuals();
	void UpdateSelectionVisuals();
	void InitializeMaterialInstances();
};