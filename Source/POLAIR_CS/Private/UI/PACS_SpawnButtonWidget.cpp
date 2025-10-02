#include "UI/PACS_SpawnButtonWidget.h"
#include "UI/PACS_SpawnListWidget.h"
#include "Core/PACS_PlayerController.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Engine/Engine.h"

void UPACS_SpawnButtonWidget::InitializeButton(FGameplayTag InSpawnTag, const FText& InDisplayName,
	UTexture2D* InIcon, const FText& InTooltip)
{
	SpawnTag = InSpawnTag;
	DisplayName = InDisplayName;
	ButtonIcon = InIcon;
	TooltipDescription = InTooltip;

	ApplyDataToWidgets();
	OnDataUpdated();
}

void UPACS_SpawnButtonWidget::UpdateAvailability(bool bAvailable)
{
	bIsAvailable = bAvailable;

	// Update button interactability
	if (CachedButton)
	{
		CachedButton->SetIsEnabled(bAvailable);
	}

	// Notify Blueprint for visual updates
	OnAvailabilityChanged(bAvailable);
}

void UPACS_SpawnButtonWidget::ResetButtonState()
{
	// Re-enable the button after placement mode ends
	if (CachedButton)
	{
		// Only enable if the button was available before
		CachedButton->SetIsEnabled(bIsAvailable);

		UE_LOG(LogTemp, Log, TEXT("PACS_SpawnButtonWidget: Button state reset - Enabled=%s"),
			bIsAvailable ? TEXT("true") : TEXT("false"));
	}
}

void UPACS_SpawnButtonWidget::SetTemporarilyDisabled(bool bDisabled)
{
	bTemporarilyDisabled = bDisabled;

	// Update the button state - disabled if temporarily disabled OR not available
	bool bShouldBeEnabled = !bTemporarilyDisabled && bIsAvailable;

	if (CachedButton)
	{
		CachedButton->SetIsEnabled(bShouldBeEnabled);
	}

	// Trigger Blueprint event with the combined state
	OnAvailabilityChanged(bShouldBeEnabled);

	UE_LOG(LogTemp, Log, TEXT("PACS_SpawnButtonWidget: SetTemporarilyDisabled=%s, Final enabled=%s"),
		bDisabled ? TEXT("true") : TEXT("false"),
		bShouldBeEnabled ? TEXT("true") : TEXT("false"));
}

void UPACS_SpawnButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Don't create UI on dedicated servers
	if (!ShouldCreateWidget())
	{
		return;
	}

	// Cache widget references
	CacheWidgetReferences();

	// Bind button click event
	if (CachedButton)
	{
		CachedButton->OnClicked.AddDynamic(this, &UPACS_SpawnButtonWidget::HandleButtonClicked);
	}

	// Apply initial data
	ApplyDataToWidgets();
}

void UPACS_SpawnButtonWidget::NativeDestruct()
{
	// Clean up button binding
	if (CachedButton)
	{
		CachedButton->OnClicked.RemoveDynamic(this, &UPACS_SpawnButtonWidget::HandleButtonClicked);
	}

	Super::NativeDestruct();
}

void UPACS_SpawnButtonWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Editor preview support
	if (IsDesignTime())
	{
		CacheWidgetReferences();
		ApplyDataToWidgets();
	}
}

void UPACS_SpawnButtonWidget::HandleButtonClicked()
{
	// Don't process if disabled
	if (!bIsAvailable)
	{
		return;
	}

	// Validate spawn tag
	if (!SpawnTag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnButtonWidget: Invalid spawn tag on button click"));
		return;
	}

	OnSpawnButtonClicked();
}

void UPACS_SpawnButtonWidget::OnSpawnButtonClicked()
{
	// Get owning player controller
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnButtonWidget: No owning player controller"));
		return;
	}

	// Find the parent SpawnListWidget and disable all buttons
	UWidget* ParentWidget = GetParent();
	while (ParentWidget)
	{
		if (UPACS_SpawnListWidget* SpawnList = Cast<UPACS_SpawnListWidget>(ParentWidget))
		{
			// Disable all buttons in the list
			SpawnList->DisableAllButtons();
			UE_LOG(LogTemp, Log, TEXT("PACS_SpawnButtonWidget: Disabled all buttons in spawn list"));
			break;
		}
		ParentWidget = ParentWidget->GetParent();
	}

	// Cast to our custom player controller
	APACS_PlayerController* PACSPC = Cast<APACS_PlayerController>(PC);
	if (!PACSPC)
	{
		UE_LOG(LogTemp, Warning, TEXT("PACS_SpawnButtonWidget: Player controller is not APACS_PlayerController"));
		return;
	}

	// Enter spawn placement mode with our spawn tag
	UE_LOG(LogTemp, Log, TEXT("PACS_SpawnButtonWidget: Entering spawn placement mode for tag: %s"),
		*SpawnTag.ToString());
	PACSPC->EnterSpawnPlacementMode(SpawnTag);
}

void UPACS_SpawnButtonWidget::CacheWidgetReferences()
{
	// Try to find button widget by name
	if (!CachedButton)
	{
		CachedButton = Cast<UButton>(GetWidgetFromName(TEXT("SpawnButton")));
		if (!CachedButton)
		{
			// Try alternative name
			CachedButton = Cast<UButton>(GetWidgetFromName(TEXT("Button")));
		}
	}

	// Try to find text widget by name
	if (!CachedTextBlock)
	{
		CachedTextBlock = Cast<UTextBlock>(GetWidgetFromName(TEXT("ButtonText")));
		if (!CachedTextBlock)
		{
			// Try alternative names
			CachedTextBlock = Cast<UTextBlock>(GetWidgetFromName(TEXT("DisplayName")));
			if (!CachedTextBlock)
			{
				CachedTextBlock = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text")));
			}
		}
	}

	// Try to find image widget by name
	if (!CachedIconImage)
	{
		CachedIconImage = Cast<UImage>(GetWidgetFromName(TEXT("ButtonIcon")));
		if (!CachedIconImage)
		{
			// Try alternative names
			CachedIconImage = Cast<UImage>(GetWidgetFromName(TEXT("Icon")));
			if (!CachedIconImage)
			{
				CachedIconImage = Cast<UImage>(GetWidgetFromName(TEXT("Image")));
			}
		}
	}
}

void UPACS_SpawnButtonWidget::ApplyDataToWidgets()
{
	// Update text if we have a text widget
	if (CachedTextBlock && !DisplayName.IsEmpty())
	{
		CachedTextBlock->SetText(DisplayName);
	}

	// Update icon if we have an image widget and icon texture
	if (CachedIconImage && ButtonIcon)
	{
		CachedIconImage->SetBrushFromTexture(ButtonIcon);
		CachedIconImage->SetVisibility(ESlateVisibility::Visible);
	}
	else if (CachedIconImage)
	{
		// Hide icon if no texture
		CachedIconImage->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Set tooltip if available
	if (CachedButton && !TooltipDescription.IsEmpty())
	{
		CachedButton->SetToolTipText(TooltipDescription);
	}
}

bool UPACS_SpawnButtonWidget::ShouldCreateWidget() const
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