#include "Settings/PACS_NetPerfSettings.h"

#if WITH_EDITOR
#include "Misc/MessageDialog.h"
#endif

UPACS_NetPerfSettings::UPACS_NetPerfSettings()
{
    // Constructor - default values are set in the header
}

#if WITH_EDITOR
void UPACS_NetPerfSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    if (!PropertyChangedEvent.Property)
    {
        return;
    }

    const FName PropertyName = PropertyChangedEvent.Property->GetFName();

    // Validate memory thresholds
    if (PropertyName == GET_MEMBER_NAME_CHECKED(UPACS_NetPerfSettings, MemoryWarningThresholdMB) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(UPACS_NetPerfSettings, MemoryCriticalThresholdMB))
    {
        if (MemoryCriticalThresholdMB <= MemoryWarningThresholdMB)
        {
            MemoryCriticalThresholdMB = MemoryWarningThresholdMB + 20;

            FMessageDialog::Open(EAppMsgType::Ok,
                FText::FromString(TEXT("Critical threshold must be higher than warning threshold. Adjusted automatically.")));
        }
    }

    // Validate pool sizes
    if (PropertyName == GET_MEMBER_NAME_CHECKED(UPACS_NetPerfSettings, SelectionPoolInitialSize) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(UPACS_NetPerfSettings, SelectionPoolMaxSize))
    {
        if (SelectionPoolInitialSize > SelectionPoolMaxSize)
        {
            SelectionPoolInitialSize = SelectionPoolMaxSize;

            FMessageDialog::Open(EAppMsgType::Ok,
                FText::FromString(TEXT("Initial pool size cannot exceed maximum pool size. Adjusted automatically.")));
        }
    }

    // Validate update rates
    if (PropertyName == GET_MEMBER_NAME_CHECKED(UPACS_NetPerfSettings, FarSelectionUpdateRate) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(UPACS_NetPerfSettings, NearSelectionUpdateRate))
    {
        if (FarSelectionUpdateRate > NearSelectionUpdateRate)
        {
            FarSelectionUpdateRate = NearSelectionUpdateRate;

            FMessageDialog::Open(EAppMsgType::Ok,
                FText::FromString(TEXT("Far update rate cannot exceed near update rate. Adjusted automatically.")));
        }
    }

    // Validate distances
    if (PropertyName == GET_MEMBER_NAME_CHECKED(UPACS_NetPerfSettings, SelectionPlaneMaxDistance))
    {
        if (SelectionPlaneMaxDistance < NearDistanceThreshold)
        {
            SelectionPlaneMaxDistance = NearDistanceThreshold * 2.0f;

            FMessageDialog::Open(EAppMsgType::Ok,
                FText::FromString(TEXT("Selection plane max distance must be greater than near distance threshold. Adjusted automatically.")));
        }
    }
}
#endif