#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "PACS_SpawnButtonWidget.generated.h"

class UTexture2D;
class UButton;
class UTextBlock;
class UImage;

/**
 * Base widget class for NPC spawn buttons
 * Handles spawn data storage and click events for UI-driven spawning
 *
 * Blueprint Usage:
 * 1. Create BP widget derived from this class
 * 2. Add a Button widget named "SpawnButton"
 * 3. Add TextBlock named "ButtonText" for display name
 * 4. Add Image named "ButtonIcon" for icon display
 * 5. Button click is automatically bound to spawn placement
 */
UCLASS()
class POLAIR_CS_API UPACS_SpawnButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ========================================
	// Configuration Data
	// ========================================

	/** The spawn tag that identifies what NPC type to spawn */
	UPROPERTY(BlueprintReadOnly, Category = "Spawn Data", meta = (ExposeOnSpawn = "true"))
	FGameplayTag SpawnTag;

	/** Display name shown on the button */
	UPROPERTY(BlueprintReadOnly, Category = "Spawn Data")
	FText DisplayName;

	/** Icon texture for the button (can be null) */
	UPROPERTY(BlueprintReadOnly, Category = "Spawn Data")
	UTexture2D* ButtonIcon;

	/** Tooltip description for hover */
	UPROPERTY(BlueprintReadOnly, Category = "Spawn Data")
	FText TooltipDescription;

	/** Whether this button is currently available based on spawn limits */
	UPROPERTY(BlueprintReadOnly, Category = "Spawn Data")
	bool bIsAvailable = true;

	/** Whether buttons are temporarily disabled for placement mode */
	bool bTemporarilyDisabled = false;

	// ========================================
	// Blueprint Events
	// ========================================

	/** Called when button data is updated - implement in BP to update visuals */
	UFUNCTION(BlueprintImplementableEvent, Category = "Spawn Button")
	void OnDataUpdated();

	/** Called when button availability changes - implement in BP for visual state */
	UFUNCTION(BlueprintImplementableEvent, Category = "Spawn Button")
	void OnAvailabilityChanged(bool bAvailable);

	// ========================================
	// Public Functions
	// ========================================

	/** Initialize button with spawn configuration data */
	UFUNCTION(BlueprintCallable, Category = "Spawn Button")
	void InitializeButton(FGameplayTag InSpawnTag, const FText& InDisplayName,
		UTexture2D* InIcon, const FText& InTooltip);

	/** Update button availability based on spawn limits */
	UFUNCTION(BlueprintCallable, Category = "Spawn Button")
	void UpdateAvailability(bool bAvailable);

	/** Reset button to enabled state after placement mode ends */
	UFUNCTION(BlueprintCallable, Category = "Spawn Button")
	void ResetButtonState();

	/** Temporarily disable button for placement mode */
	UFUNCTION(BlueprintCallable, Category = "Spawn Button")
	void SetTemporarilyDisabled(bool bDisabled);

	/** Get the button widget if bound */
	UFUNCTION(BlueprintPure, Category = "Spawn Button")
	UButton* GetButtonWidget() const { return CachedButton; }

protected:
	// ========================================
	// UUserWidget Interface
	// ========================================

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativePreConstruct() override;

	// ========================================
	// Click Handling
	// ========================================

	/** Handle button click - triggers spawn placement mode */
	UFUNCTION()
	void HandleButtonClicked();

	/** Blueprint-callable version for custom handling */
	UFUNCTION(BlueprintCallable, Category = "Spawn Button")
	void OnSpawnButtonClicked();

private:
	// ========================================
	// Cached Widget References
	// ========================================

	/** Cached reference to the button widget */
	UPROPERTY()
	UButton* CachedButton;

	/** Cached reference to the text widget */
	UPROPERTY()
	UTextBlock* CachedTextBlock;

	/** Cached reference to the icon widget */
	UPROPERTY()
	UImage* CachedIconImage;

	// ========================================
	// Helper Functions
	// ========================================

	/** Find and cache widget references */
	void CacheWidgetReferences();

	/** Apply data to cached widgets */
	void ApplyDataToWidgets();

	/** Check if we should create widgets (not on dedicated server) */
	bool ShouldCreateWidget() const;
};