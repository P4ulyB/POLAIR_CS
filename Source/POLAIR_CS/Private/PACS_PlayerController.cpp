#include "PACS_PlayerController.h"

#if !UE_SERVER
#include "EnhancedInputComponent.h"
#include "PACS_InputTypes.h"
#endif

APACS_PlayerController::APACS_PlayerController()
{
    InputHandler = CreateDefaultSubobject<UPACS_InputHandlerComponent>(TEXT("InputHandler"));
}

void APACS_PlayerController::BeginPlay()
{
    Super::BeginPlay();
    ValidateInputSystem();
}

void APACS_PlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
#if !UE_SERVER
    BindInputActions();
#endif
}

void APACS_PlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    
#if !UE_SERVER
    if (InputHandler)
    {
        InputHandler->OnSubsystemAvailable();
    }
#endif
}

void APACS_PlayerController::OnUnPossess()
{
#if !UE_SERVER
    if (InputHandler)
    {
        InputHandler->OnSubsystemUnavailable();
    }
#endif
    
    Super::OnUnPossess();
}

void APACS_PlayerController::ValidateInputSystem()
{
#if !UE_SERVER
    if (!InputHandler)
    {
        UE_LOG(LogPACSInput, Error, 
            TEXT("InputHandler component missing! Input will not work."));
        return;
    }
    
    if (!InputHandler->IsHealthy())
    {
        UE_LOG(LogPACSInput, Warning, 
            TEXT("InputHandler not healthy - check configuration"));
    }
#endif
}

void APACS_PlayerController::BindInputActions()
{
#if !UE_SERVER
    if (!InputHandler || !InputHandler->InputConfig)
    {
        UE_LOG(LogPACSInput, Warning, 
            TEXT("Cannot bind input actions - invalid configuration"));
        return;
    }

    UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
    if (!EIC)
    {
        UE_LOG(LogPACSInput, Error, TEXT("Enhanced Input Component not found!"));
        return;
    }

    for (const FPACS_InputActionMapping& Mapping : InputHandler->InputConfig->ActionMappings)
    {
        if (!Mapping.InputAction)
        {
            UE_LOG(LogPACSInput, Warning, TEXT("Null InputAction for %s"), 
                *Mapping.ActionIdentifier.ToString());
            continue;
        }

        if (Mapping.bBindStarted)
        {
            EIC->BindAction(Mapping.InputAction.Get(), ETriggerEvent::Started, 
                InputHandler.Get(), &UPACS_InputHandlerComponent::HandleAction);
        }
        
        if (Mapping.bBindTriggered)
        {
            EIC->BindAction(Mapping.InputAction.Get(), ETriggerEvent::Triggered, 
                InputHandler.Get(), &UPACS_InputHandlerComponent::HandleAction);
        }
        
        if (Mapping.bBindCompleted)
        {
            EIC->BindAction(Mapping.InputAction.Get(), ETriggerEvent::Completed, 
                InputHandler.Get(), &UPACS_InputHandlerComponent::HandleAction);
        }
        
        if (Mapping.bBindOngoing)
        {
            EIC->BindAction(Mapping.InputAction.Get(), ETriggerEvent::Ongoing, 
                InputHandler.Get(), &UPACS_InputHandlerComponent::HandleAction);
        }
        
        if (Mapping.bBindCanceled)
        {
            EIC->BindAction(Mapping.InputAction.Get(), ETriggerEvent::Canceled, 
                InputHandler.Get(), &UPACS_InputHandlerComponent::HandleAction);
        }
    }
    
    UE_LOG(LogPACSInput, Log, TEXT("Bound %d input actions (permanent bindings)"), 
        InputHandler->InputConfig->ActionMappings.Num());
#endif
}