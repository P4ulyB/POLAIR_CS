#include "UI/PACS_SpawnListWidget.h"
#include "UI/PACS_SpawnButtonWidget.h"
#include "Data/PACS_SpawnConfig.h"
#include "Components/VerticalBox.h"
#include "Components/Button.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"

void UPACS_SpawnListWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Don't create UI on dedicated servers
	if (!ShouldCreateWidget())
	{
		return;
	}

	// Cache widget references
	CacheButtonContainer();

	// Populate buttons with a slight delay to ensure systems are ready
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick([this]()
		{
			RefreshSpawnButtons();
		});
	}
}

void UPACS_SpawnListWidget::NativeDestruct()
{
	// Clean up any async loads
	if (IconLoadHandle.IsValid())
	{
		IconLoadHandle->CancelHandle();
		IconLoadHandle.Reset();
	}

	ClearSpawnButtons();

	Super::NativeDestruct();
}

void UPACS_SpawnListWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Editor preview support
	if (IsDesignTime())
	{
		CacheButtonContainer();
		// Could add preview buttons here if desired
	}
}

void UPACS_SpawnListWidget::RefreshSpawnButtons()
{
	// Clear existing buttons
	ClearSpawnButtons();

	// Use the SpawnConfigAsset set in Blueprint
	if (!SpawnConfigAsset)
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnListWidget: SpawnConfigAsset not set. Set it in the widget Blueprint."));
		return;
	}

	// Populate buttons from config
	PopulateSpawnButtons(SpawnConfigAsset);
}

void UPACS_SpawnListWidget::ClearSpawnButtons()
{
	if (!ButtonContainer)
	{
		return;
	}

	// Remove all button widgets
	for (UPACS_SpawnButtonWidget* Button : SpawnButtons)
	{
		if (Button)
		{
			Button->RemoveFromParent();
		}
	}

	SpawnButtons.Empty();
	ButtonContainer->ClearChildren();
	bButtonsPopulated = false;
}

void UPACS_SpawnListWidget::UpdateButtonAvailability()
{
	// Update each button's availability based on current spawn limits
	for (UPACS_SpawnButtonWidget* Button : SpawnButtons)
	{
		if (Button && Button->SpawnTag.IsValid())
		{
			bool bAvailable = IsSpawnAvailable(Button->SpawnTag);
			Button->UpdateAvailability(bAvailable);
		}
	}
}

void UPACS_SpawnListWidget::DisableAllButtons()
{
	UE_LOG(LogTemp, Log, TEXT("PACS_SpawnListWidget: Disabling all %d spawn buttons"), SpawnButtons.Num());

	// Temporarily disable all buttons for placement mode
	// This preserves their original availability state
	for (UPACS_SpawnButtonWidget* Button : SpawnButtons)
	{
		if (Button)
		{
			// Set temporarily disabled - this will trigger OnAvailabilityChanged
			Button->SetTemporarilyDisabled(true);
		}
	}
}

void UPACS_SpawnListWidget::EnableAllButtons()
{
	UE_LOG(LogTemp, Log, TEXT("PACS_SpawnListWidget: Enabling all %d spawn buttons"), SpawnButtons.Num());

	// Remove temporary disable, restoring buttons to their availability state
	for (UPACS_SpawnButtonWidget* Button : SpawnButtons)
	{
		if (Button)
		{
			// Clear temporary disable - buttons return to their bIsAvailable state
			Button->SetTemporarilyDisabled(false);
		}
	}
}

void UPACS_SpawnListWidget::PopulateSpawnButtons(UPACS_SpawnConfig* Config)
{
	if (!Config || !ButtonContainer)
	{
		return;
	}

	const TArray<FSpawnClassConfig>& SpawnConfigs = Config->GetSpawnConfigs();
	int32 ButtonsCreated = 0;

	UE_LOG(LogTemp, Log, TEXT("PACS_SpawnListWidget: Populating buttons from %d spawn configs"),
		SpawnConfigs.Num());

	// Create button for each spawn config
	for (const FSpawnClassConfig& SpawnConfig : SpawnConfigs)
	{
		// Check if we should display this config
		if (bFilterByUIVisibility && !SpawnConfig.bVisibleInUI)
		{
			continue;
		}

		// Check max button limit
		if (MaxButtonsToDisplay > 0 && ButtonsCreated >= MaxButtonsToDisplay)
		{
			break;
		}

		// Create button widget
		UPACS_SpawnButtonWidget* NewButton = CreateSpawnButton(SpawnConfig);
		if (NewButton)
		{
			AddButtonToContainer(NewButton);
			SpawnButtons.Add(NewButton);
			ButtonsCreated++;

			// Notify Blueprint
			OnButtonCreated(NewButton);
		}
	}

	bButtonsPopulated = true;

	UE_LOG(LogTemp, Log, TEXT("PACS_SpawnListWidget: Created %d spawn buttons"), ButtonsCreated);

	// Notify Blueprint of completion
	OnButtonsPopulated(ButtonsCreated);

	// Start async loading of icons
	LoadButtonIcons();
}

UPACS_SpawnButtonWidget* UPACS_SpawnListWidget::CreateSpawnButton(const FSpawnClassConfig& SpawnConfig)
{
	if (!ButtonWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnListWidget: ButtonWidgetClass not set"));
		return nullptr;
	}

	// Create the button widget
	UPACS_SpawnButtonWidget* NewButton = CreateWidget<UPACS_SpawnButtonWidget>(this, ButtonWidgetClass);
	if (!NewButton)
	{
		UE_LOG(LogTemp, Error, TEXT("PACS_SpawnListWidget: Failed to create button widget"));
		return nullptr;
	}

	// Initialize button with spawn data
	// Note: Icon might be null initially if async loading
	NewButton->InitializeButton(
		SpawnConfig.SpawnTag,
		SpawnConfig.DisplayName,
		nullptr,  // Icon will be loaded async
		SpawnConfig.TooltipDescription
	);

	// Check initial availability
	bool bAvailable = IsSpawnAvailable(SpawnConfig.SpawnTag);
	NewButton->UpdateAvailability(bAvailable);

	return NewButton;
}

void UPACS_SpawnListWidget::AddButtonToContainer(UPACS_SpawnButtonWidget* Button)
{
	if (!ButtonContainer || !Button)
	{
		return;
	}

	// Add to vertical box
	ButtonContainer->AddChildToVerticalBox(Button);

	// Could add spacing or slot configuration here if needed
	// For example:
	// if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(Button->Slot))
	// {
	//     Slot->SetPadding(FMargin(0, 2));
	// }
}

void UPACS_SpawnListWidget::CacheButtonContainer()
{
	// Try to find vertical box by common names
	if (!ButtonContainer)
	{
		ButtonContainer = Cast<UVerticalBox>(GetWidgetFromName(TEXT("ButtonContainer")));
		if (!ButtonContainer)
		{
			ButtonContainer = Cast<UVerticalBox>(GetWidgetFromName(TEXT("VerticalBox")));
			if (!ButtonContainer)
			{
				ButtonContainer = Cast<UVerticalBox>(GetWidgetFromName(TEXT("Vertical_Box")));
			}
		}

		if (!ButtonContainer)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("PACS_SpawnListWidget: Could not find VerticalBox widget. Expected names: ButtonContainer, VerticalBox, or Vertical_Box"));
		}
	}
}

bool UPACS_SpawnListWidget::ShouldCreateWidget() const
{
	// Never create widgets on dedicated servers
	if (IsRunningDedicatedServer())
	{
		return false;
	}

	// Only create for local players
	APlayerController* PC = GetOwningPlayer();
	return PC && PC->IsLocalController();
}

void UPACS_SpawnListWidget::LoadButtonIcons()
{
	// Use the SpawnConfigAsset set in Blueprint
	if (!SpawnConfigAsset)
	{
		return;
	}

	// Collect all icon assets to load
	TArray<FSoftObjectPath> IconsToLoad;
	TMap<FSoftObjectPath, UPACS_SpawnButtonWidget*> PathToButtonMap;

	for (UPACS_SpawnButtonWidget* Button : SpawnButtons)
	{
		if (!Button || !Button->SpawnTag.IsValid())
		{
			continue;
		}

		// Find the config for this button
		FSpawnClassConfig SpawnConfig;
		if (SpawnConfigAsset->GetConfigForTag(Button->SpawnTag, SpawnConfig))
		{
			if (!SpawnConfig.ButtonIcon.IsNull())
			{
				FSoftObjectPath IconPath = SpawnConfig.ButtonIcon.ToSoftObjectPath();
				IconsToLoad.Add(IconPath);
				PathToButtonMap.Add(IconPath, Button);
			}
		}
	}

	// If no icons to load, we're done
	if (IconsToLoad.Num() == 0)
	{
		return;
	}

	// Start async loading
	FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
	IconLoadHandle = StreamableManager.RequestAsyncLoad(
		IconsToLoad,
		FStreamableDelegate::CreateLambda([this, PathToButtonMap]()
		{
			// Icons loaded, apply them to buttons
			for (const auto& Pair : PathToButtonMap)
			{
				FSoftObjectPath Path = Pair.Key;
				UPACS_SpawnButtonWidget* Button = Pair.Value;

				if (Button && IsValid(Button))
				{
					UTexture2D* LoadedIcon = Cast<UTexture2D>(Path.ResolveObject());
					if (LoadedIcon)
					{
						Button->ButtonIcon = LoadedIcon;
						Button->OnDataUpdated();
					}
				}
			}

			UE_LOG(LogTemp, Log, TEXT("PACS_SpawnListWidget: Loaded %d button icons"), PathToButtonMap.Num());
		})
	);
}

bool UPACS_SpawnListWidget::IsSpawnAvailable(FGameplayTag SpawnTag) const
{
	// TODO: Check spawn limits here
	// For now, always return true
	// In a full implementation, would check:
	// - Player spawn limit
	// - Global spawn limit
	// - Pool availability

	return true;
}

// Note: SpawnOrchestrator is server-only, so this method is no longer used.
// The widget gets its config from the SpawnConfigAsset property set in Blueprint.