#include "Data/PACS_SelectionProfile.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

UPACS_SelectionProfileAsset::UPACS_SelectionProfileAsset()
{
    // Default constructor
}

#if WITH_EDITOR
EDataValidationResult UPACS_SelectionProfileAsset::IsDataValid(TArray<FText>& ValidationErrors)
{
    EDataValidationResult Result = Super::IsDataValid(ValidationErrors);

    // Validate brightness values
    if (HoveredBrightness < 0.1f || HoveredBrightness > 3.0f)
    {
        ValidationErrors.Add(NSLOCTEXT("SelectionProfile", "InvalidHoveredBrightness",
            "Hovered brightness must be between 0.1 and 3.0"));
        Result = EDataValidationResult::Invalid;
    }

    if (SelectedBrightness < 0.1f || SelectedBrightness > 3.0f)
    {
        ValidationErrors.Add(NSLOCTEXT("SelectionProfile", "InvalidSelectedBrightness",
            "Selected brightness must be between 0.1 and 3.0"));
        Result = EDataValidationResult::Invalid;
    }

    if (UnavailableBrightness < 0.1f || UnavailableBrightness > 3.0f)
    {
        ValidationErrors.Add(NSLOCTEXT("SelectionProfile", "InvalidUnavailableBrightness",
            "Unavailable brightness must be between 0.1 and 3.0"));
        Result = EDataValidationResult::Invalid;
    }

    if (AvailableBrightness < 0.1f || AvailableBrightness > 3.0f)
    {
        ValidationErrors.Add(NSLOCTEXT("SelectionProfile", "InvalidAvailableBrightness",
            "Available brightness must be between 0.1 and 3.0"));
        Result = EDataValidationResult::Invalid;
    }

    // Validate render distance
    if (RenderDistance < 100.0f || RenderDistance > 10000.0f)
    {
        ValidationErrors.Add(NSLOCTEXT("SelectionProfile", "InvalidRenderDistance",
            "Render distance must be between 100.0 and 10000.0"));
        Result = EDataValidationResult::Invalid;
    }

    return Result;
}
#endif