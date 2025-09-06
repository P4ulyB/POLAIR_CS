#include "PACS_InputMappingConfig.h"

FName UPACS_InputMappingConfig::GetActionIdentifier(const UInputAction* InputAction) const
{
    for (const FPACS_InputActionMapping& Mapping : ActionMappings)
    {
        if (Mapping.InputAction == InputAction)
        {
            return Mapping.ActionIdentifier;
        }
    }
    return NAME_None;
}