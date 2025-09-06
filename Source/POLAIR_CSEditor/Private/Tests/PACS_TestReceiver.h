#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PACS_InputTypes.h"
#include "PACS_TestReceiver.generated.h"

UCLASS()
class UPACS_TestReceiver : public UObject, public IPACS_InputReceiver
{
    GENERATED_BODY()

public:
    UPROPERTY()
    EPACS_InputHandleResult Response = EPACS_InputHandleResult::NotHandled;
    
    UPROPERTY()
    int32 PriorityOverride = 400; // Gameplay priority

    UPROPERTY()
    FName LastAction = NAME_None;
    
    UPROPERTY()
    FInputActionValue LastValue;

    virtual EPACS_InputHandleResult HandleInputAction(FName ActionName, const FInputActionValue& Value) override
    {
        LastAction = ActionName;
        LastValue = Value;
        return Response;
    }
    
    virtual int32 GetInputPriority() const override 
    { 
        return PriorityOverride; 
    }
};