#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"
#include "PACS_InputTypes.generated.h"

// Logging category
DECLARE_LOG_CATEGORY_EXTERN(LogPACSInput, Log, All);

// Production logging macros
#define PACS_INPUT_ERROR(Format, ...) UE_LOG(LogPACSInput, Error, TEXT(Format), ##__VA_ARGS__)
#define PACS_INPUT_WARNING(Format, ...) UE_LOG(LogPACSInput, Warning, TEXT(Format), ##__VA_ARGS__)
#define PACS_INPUT_LOG(Format, ...) UE_LOG(LogPACSInput, Log, TEXT(Format), ##__VA_ARGS__)
#define PACS_INPUT_VERBOSE(Format, ...) UE_LOG(LogPACSInput, VeryVerbose, TEXT(Format), ##__VA_ARGS__)

// Safety constants
namespace PACS_InputLimits
{
    static constexpr int32 MaxOverlayContexts = 10;
    static constexpr int32 MaxReceivers = 100;
    static constexpr int32 MaxActionsPerConfig = 100;
    static constexpr int32 InvalidReceiverCleanupThreshold = 10;
}

namespace PACS_InputPriority
{
    static constexpr int32 Critical     = 10000;
    static constexpr int32 UI          = 1000;
    static constexpr int32 Menu        = 800;
    static constexpr int32 Modal       = 600;
    static constexpr int32 Gameplay    = 400;
    static constexpr int32 Background  = 200;
}

UENUM()
enum class EPACS_InputHandleResult : uint8
{
    NotHandled,
    HandledPassThrough,
    HandledConsume
};

UENUM()
enum class EPACS_InputContextMode : uint8
{
    Gameplay,
    Menu,
    UI
};

UENUM()
enum class EPACS_OverlayType : uint8
{
    None,
    Blocking,
    NonBlocking,
    Partial
};

UINTERFACE(MinimalAPI)
class UPACS_InputReceiver : public UInterface
{
    GENERATED_BODY()
};

class IPACS_InputReceiver
{
    GENERATED_BODY()
public:
    virtual EPACS_InputHandleResult HandleInputAction(FName ActionName, const FInputActionValue& Value) = 0;
    virtual int32 GetInputPriority() const { return PACS_InputPriority::Gameplay; }
};

USTRUCT()
struct FPACS_InputReceiverEntry
{
    GENERATED_BODY()

    UPROPERTY() 
    TWeakObjectPtr<UObject> ReceiverObject;
    
    UPROPERTY() 
    int32 Priority = 0;
    
    UPROPERTY() 
    uint32 RegistrationOrder = 0;

    IPACS_InputReceiver* GetInterface() const
    {
        if (UObject* Obj = ReceiverObject.Get())
        {
            return Cast<IPACS_InputReceiver>(Obj);
        }
        return nullptr;
    }

    bool IsValid() const 
    { 
        return ReceiverObject.IsValid() && GetInterface() != nullptr; 
    }

    bool operator<(const FPACS_InputReceiverEntry& Other) const
    {
        if (Priority != Other.Priority) return Priority > Other.Priority;
        return RegistrationOrder < Other.RegistrationOrder;
    }
};

USTRUCT()
struct FPACS_OverlayEntry
{
    GENERATED_BODY()

    UPROPERTY()
    TObjectPtr<UInputMappingContext> Context;
    
    UPROPERTY()
    EPACS_OverlayType Type = EPACS_OverlayType::None;
    
    UPROPERTY()
    int32 Priority = 0;
};