#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Engine/StreamableManager.h"
#include "PACS_SpawnListWidget.generated.h"

class UVerticalBox;
class UPACS_SpawnButtonWidget;
class UPACS_SpawnConfig;
struct FSpawnClassConfig;

/**
 * Widget that manages the list of spawn buttons
 * Dynamically creates buttons based on SpawnConfig data asset
 *
 * Blueprint Usage:
 * 1. Create BP widget derived from this class
 * 2. Add VerticalBox named "ButtonContainer" or "VerticalBox"
 * 3. Set ButtonWidgetClass to your BP_SpawnButton class
 * 4. Buttons are auto-populated from SpawnOrchestrator config
 */
UCLASS()
class POLAIR_CS_API UPACS_SpawnListWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ========================================
	// Configuration
	// ========================================

	/** The widget class to use for spawn buttons (set in Blueprint) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn List")
	TSubclassOf<UPACS_SpawnButtonWidget> ButtonWidgetClass;

	/** The spawn configuration data asset (set in Blueprint) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn List")
	TObjectPtr<UPACS_SpawnConfig> SpawnConfigAsset;

	/** Whether to filter out non-UI-visible configs */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn List")
	bool bFilterByUIVisibility = true;

	/** Maximum buttons to display (0 = unlimited) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn List",
		meta = (ClampMin = "0", ClampMax = "50"))
	int32 MaxButtonsToDisplay = 20;

	// ========================================
	// Public Functions
	// ========================================

	/** Refresh the spawn button list from current config */
	UFUNCTION(BlueprintCallable, Category = "Spawn List")
	void RefreshSpawnButtons();

	/** Clear all spawn buttons */
	UFUNCTION(BlueprintCallable, Category = "Spawn List")
	void ClearSpawnButtons();

	/** Update button availability based on spawn limits */
	UFUNCTION(BlueprintCallable, Category = "Spawn List")
	void UpdateButtonAvailability();

	/** Disable all spawn buttons (used when entering placement mode) */
	UFUNCTION(BlueprintCallable, Category = "Spawn List")
	void DisableAllButtons();

	/** Enable all spawn buttons (used when exiting placement mode) */
	UFUNCTION(BlueprintCallable, Category = "Spawn List")
	void EnableAllButtons();

	/** Get all spawn button widgets */
	UFUNCTION(BlueprintPure, Category = "Spawn List")
	TArray<UPACS_SpawnButtonWidget*> GetSpawnButtons() const { return SpawnButtons; }

protected:
	// ========================================
	// UUserWidget Interface
	// ========================================

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativePreConstruct() override;

	// ========================================
	// Button Management
	// ========================================

	/** Create buttons from spawn config */
	void PopulateSpawnButtons(UPACS_SpawnConfig* Config);

	/** Create a single button for a spawn config */
	UPACS_SpawnButtonWidget* CreateSpawnButton(const FSpawnClassConfig& SpawnConfig);

	/** Add button to the container */
	void AddButtonToContainer(UPACS_SpawnButtonWidget* Button);

	/** Blueprint event when buttons are populated */
	UFUNCTION(BlueprintImplementableEvent, Category = "Spawn List")
	void OnButtonsPopulated(int32 ButtonCount);

	/** Blueprint event when a button is created */
	UFUNCTION(BlueprintImplementableEvent, Category = "Spawn List")
	void OnButtonCreated(UPACS_SpawnButtonWidget* Button);

private:
	// ========================================
	// Widget References
	// ========================================

	/** Container for spawn buttons */
	UPROPERTY()
	UVerticalBox* ButtonContainer;

	/** Array of created spawn buttons */
	UPROPERTY()
	TArray<UPACS_SpawnButtonWidget*> SpawnButtons;

	// ========================================
	// State
	// ========================================

	/** Whether we've already populated buttons */
	bool bButtonsPopulated = false;

	/** Handle for async icon loading */
	TSharedPtr<FStreamableHandle> IconLoadHandle;

	// ========================================
	// Helper Functions
	// ========================================

	/** Find and cache the button container widget */
	void CacheButtonContainer();

	/** Check if we should create widgets (not on server) */
	bool ShouldCreateWidget() const;

	/** Async load button icons */
	void LoadButtonIcons();

	/** Check spawn limits for a given tag */
	bool IsSpawnAvailable(FGameplayTag SpawnTag) const;
};