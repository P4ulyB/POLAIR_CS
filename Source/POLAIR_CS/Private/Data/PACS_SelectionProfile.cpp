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

    // Validate brightness values are reasonable
    if (Profile.HoveredBrightness < 0.1f || Profile.HoveredBrightness > 3.0f)
    {
        ValidationErrors.Add(NSLOCTEXT("SelectionProfile", "InvalidHoveredBrightness",
            "Hovered brightness is outside valid range (0.1 - 3.0)"));
        Result = EDataValidationResult::Invalid;
    }

    if (Profile.SelectedBrightness < 0.1f || Profile.SelectedBrightness > 3.0f)
    {
        ValidationErrors.Add(NSLOCTEXT("SelectionProfile", "InvalidSelectedBrightness",
            "Selected brightness is outside valid range (0.1 - 3.0)"));
        Result = EDataValidationResult::Invalid;
    }

    if (Profile.UnavailableBrightness < 0.1f || Profile.UnavailableBrightness > 3.0f)
    {
        ValidationErrors.Add(NSLOCTEXT("SelectionProfile", "InvalidUnavailableBrightness",
            "Unavailable brightness is outside valid range (0.1 - 3.0)"));
        Result = EDataValidationResult::Invalid;
    }

    if (Profile.AvailableBrightness < 0.1f || Profile.AvailableBrightness > 3.0f)
    {
        ValidationErrors.Add(NSLOCTEXT("SelectionProfile", "InvalidAvailableBrightness",
            "Available brightness is outside valid range (0.1 - 3.0)"));
        Result = EDataValidationResult::Invalid;
    }

    // Validate scale values
    if (Profile.ProjectionScale < 0.5f || Profile.ProjectionScale > 3.0f)
    {
        ValidationErrors.Add(NSLOCTEXT("SelectionProfile", "InvalidScale",
            "Projection scale is outside valid range (0.5 - 3.0)"));
        Result = EDataValidationResult::Invalid;
    }

    return Result;
}
#endif