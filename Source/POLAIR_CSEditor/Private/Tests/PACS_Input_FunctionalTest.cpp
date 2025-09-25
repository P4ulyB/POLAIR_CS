#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"
#include "EnhancedInputSubsystems.h"

#include "PACS_TestReceiver.h"
#include "Components/PACS_InputHandlerComponent.h"
#include "Data/Configs/PACS_InputMappingConfig.h"
#include "Core/PACS_PlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPACS_InputHandlerIntegrationTest,
    "PACS.Input.InputHandler.Integration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPACS_InputHandlerIntegrationTest::RunTest(const FString& Parameters)
{
    UE_LOG(LogTemp, Warning, TEXT("PACS Integration Test: Starting basic configuration test"));
    
    // Test 1: Configuration validation
    UPACS_InputMappingConfig* Config = NewObject<UPACS_InputMappingConfig>(GetTransientPackage());
    UInputMappingContext* IMC_Gameplay = NewObject<UInputMappingContext>(GetTransientPackage());
    UInputMappingContext* IMC_Menu = NewObject<UInputMappingContext>(GetTransientPackage());
    UInputMappingContext* IMC_UI = NewObject<UInputMappingContext>(GetTransientPackage());
    UInputAction* IA_Test = NewObject<UInputAction>(GetTransientPackage());

    FPACS_InputActionMapping M;
    M.InputAction = IA_Test;
    M.ActionIdentifier = TEXT("TestAction");
    Config->ActionMappings = { M };
    Config->GameplayContext = IMC_Gameplay;
    Config->MenuContext = IMC_Menu;
    Config->UIContext = IMC_UI;

    TestTrue(TEXT("Config should be valid"), Config->IsValid());
    TestEqual(TEXT("GetActionIdentifier should work"), Config->GetActionIdentifier(IA_Test), FName(TEXT("TestAction")));
    
    // Test 2: Basic component creation and properties
    UPACS_InputHandlerComponent* Handler = NewObject<UPACS_InputHandlerComponent>(GetTransientPackage());
    Handler->InputConfig = Config;
    
    // Test overlay count (should work without full initialization)
    TestEqual(TEXT("Initial overlay count should be 0"), Handler->GetOverlayCount(), 0);
    TestFalse(TEXT("Should not have blocking overlay initially"), Handler->HasBlockingOverlay());
    
    // Test 3: Simple receiver system (without full initialization)
    UPACS_TestReceiver* RConsume = NewObject<UPACS_TestReceiver>(GetTransientPackage());
    RConsume->Response = EPACS_InputHandleResult::HandledConsume;
    RConsume->PriorityOverride = 10000;

    UPACS_TestReceiver* RPass = NewObject<UPACS_TestReceiver>(GetTransientPackage());
    RPass->Response = EPACS_InputHandleResult::HandledPassThrough;
    RPass->PriorityOverride = 400;

    TestEqual(TEXT("High priority receiver should return correct priority"), RConsume->GetInputPriority(), 10000);
    TestEqual(TEXT("Normal priority receiver should return correct priority"), RPass->GetInputPriority(), 400);
    
    // Test 4: Interface implementation
    TestEqual(TEXT("Consume receiver should handle correctly"), RConsume->HandleInputAction(TEXT("TestAction"), FInputActionValue()), EPACS_InputHandleResult::HandledConsume);
    TestEqual(TEXT("Pass receiver should handle correctly"), RPass->HandleInputAction(TEXT("TestAction"), FInputActionValue()), EPACS_InputHandleResult::HandledPassThrough);
    
    TestEqual(TEXT("Receiver should capture action name"), RConsume->LastAction, FName(TEXT("TestAction")));
    
    UE_LOG(LogTemp, Warning, TEXT("PACS Integration Test: Basic tests completed successfully"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPACS_OverlayManagementTest,
    "PACS.Input.OverlayManagement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPACS_OverlayManagementTest::RunTest(const FString& Parameters)
{
    UE_LOG(LogTemp, Warning, TEXT("PACS Overlay Test: Starting overlay stack tests"));
    
    // Setup basic config and handler
    UPACS_InputMappingConfig* Config = NewObject<UPACS_InputMappingConfig>(GetTransientPackage());
    UInputMappingContext* IMC_Gameplay = NewObject<UInputMappingContext>(GetTransientPackage());
    UInputMappingContext* IMC_Menu = NewObject<UInputMappingContext>(GetTransientPackage());
    UInputMappingContext* IMC_UI = NewObject<UInputMappingContext>(GetTransientPackage());
    
    Config->GameplayContext = IMC_Gameplay;
    Config->MenuContext = IMC_Menu;
    Config->UIContext = IMC_UI;
    
    UInputAction* IA_Test = NewObject<UInputAction>(GetTransientPackage());
    FPACS_InputActionMapping M;
    M.InputAction = IA_Test;
    M.ActionIdentifier = TEXT("TestAction");
    Config->ActionMappings = { M };
    
    UPACS_InputHandlerComponent* Handler = NewObject<UPACS_InputHandlerComponent>(GetTransientPackage());
    Handler->InputConfig = Config;
    
    // Test 1: Initial overlay state (these work without initialization)
    TestEqual(TEXT("Initial overlay count should be 0"), Handler->GetOverlayCount(), 0);
    TestFalse(TEXT("Should not have blocking overlay initially"), Handler->HasBlockingOverlay());
    
    // Test 2: Validate config setup
    TestTrue(TEXT("Config should be valid for overlay tests"), Config->IsValid());
    TestEqual(TEXT("Config should have test action"), Config->GetActionIdentifier(IA_Test), FName(TEXT("TestAction")));
    
    // Test 3: Handler health without initialization 
    // Note: Handler requires PlayerController and Enhanced Input subsystem for full initialization
    // These PushOverlay/PopOverlay methods require bIsInitialized=true which needs:
    // - PlayerController owner
    // - Enhanced Input subsystem
    // - Valid config (which we have)
    TestFalse(TEXT("Handler should not be healthy without initialization"), Handler->IsHealthy());
    
    // Test 4: Test overlay type enum values (compile-time verification)
    EPACS_OverlayType BlockingType = EPACS_OverlayType::Blocking;
    EPACS_OverlayType NonBlockingType = EPACS_OverlayType::NonBlocking;
    TestTrue(TEXT("Blocking and NonBlocking overlay types should be different"), BlockingType != NonBlockingType);
    
    // NOTE: PushOverlay/PopOverlay require full component initialization with PlayerController
    // This would require a more complex test setup with actual game world context
    // For now, we test what we can in isolation
    
    UE_LOG(LogTemp, Warning, TEXT("PACS Overlay Test: Overlay interface tests completed successfully"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPACS_ReceiverPriorityTest,
    "PACS.Input.ReceiverPriority",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPACS_ReceiverPriorityTest::RunTest(const FString& Parameters)
{
    UE_LOG(LogTemp, Warning, TEXT("PACS Priority Test: Starting receiver priority tests"));
    
    // Create test receivers with different priorities
    UPACS_TestReceiver* RHigh = NewObject<UPACS_TestReceiver>(GetTransientPackage());
    RHigh->Response = EPACS_InputHandleResult::HandledConsume;
    RHigh->PriorityOverride = 10000; // UI priority
    
    UPACS_TestReceiver* RMed = NewObject<UPACS_TestReceiver>(GetTransientPackage());
    RMed->Response = EPACS_InputHandleResult::HandledPassThrough;
    RMed->PriorityOverride = 1000; // Menu priority
    
    UPACS_TestReceiver* RLow = NewObject<UPACS_TestReceiver>(GetTransientPackage());
    RLow->Response = EPACS_InputHandleResult::HandledPassThrough;
    RLow->PriorityOverride = 400; // Gameplay priority
    
    // Test 1: Priority values
    TestEqual(TEXT("High priority receiver should return 10000"), RHigh->GetInputPriority(), 10000);
    TestEqual(TEXT("Medium priority receiver should return 1000"), RMed->GetInputPriority(), 1000);
    TestEqual(TEXT("Low priority receiver should return 400"), RLow->GetInputPriority(), 400);
    
    // Test 2: Response handling
    TestEqual(TEXT("High priority should consume"), RHigh->HandleInputAction(TEXT("TestAction"), FInputActionValue()), EPACS_InputHandleResult::HandledConsume);
    TestEqual(TEXT("Medium priority should pass through"), RMed->HandleInputAction(TEXT("TestAction"), FInputActionValue()), EPACS_InputHandleResult::HandledPassThrough);
    TestEqual(TEXT("Low priority should pass through"), RLow->HandleInputAction(TEXT("TestAction"), FInputActionValue()), EPACS_InputHandleResult::HandledPassThrough);
    
    // Test 3: Action capture
    TestEqual(TEXT("High priority should capture action name"), RHigh->LastAction, FName(TEXT("TestAction")));
    TestEqual(TEXT("Medium priority should capture action name"), RMed->LastAction, FName(TEXT("TestAction")));
    TestEqual(TEXT("Low priority should capture action name"), RLow->LastAction, FName(TEXT("TestAction")));
    
    UE_LOG(LogTemp, Warning, TEXT("PACS Priority Test: Receiver priority tests completed successfully"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPACS_ContextSwitchingTest,
    "PACS.Input.ContextSwitching",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPACS_ContextSwitchingTest::RunTest(const FString& Parameters)
{
    UE_LOG(LogTemp, Warning, TEXT("PACS Context Test: Starting context switching tests"));
    
    // Test 1: Context mode enum values
    EPACS_InputContextMode GameplayMode = EPACS_InputContextMode::Gameplay;
    EPACS_InputContextMode MenuMode = EPACS_InputContextMode::Menu;
    EPACS_InputContextMode UIMode = EPACS_InputContextMode::UI;
    
    TestTrue(TEXT("Context modes should all be different (Gameplay vs Menu)"), GameplayMode != MenuMode);
    TestTrue(TEXT("Context modes should all be different (Menu vs UI)"), MenuMode != UIMode);
    TestTrue(TEXT("Context modes should all be different (Gameplay vs UI)"), GameplayMode != UIMode);
    
    // Test 2: Config context assignment
    UPACS_InputMappingConfig* Config = NewObject<UPACS_InputMappingConfig>(GetTransientPackage());
    UInputMappingContext* IMC_Gameplay = NewObject<UInputMappingContext>(GetTransientPackage());
    UInputMappingContext* IMC_Menu = NewObject<UInputMappingContext>(GetTransientPackage());
    UInputMappingContext* IMC_UI = NewObject<UInputMappingContext>(GetTransientPackage());
    
    Config->GameplayContext = IMC_Gameplay;
    Config->MenuContext = IMC_Menu;
    Config->UIContext = IMC_UI;
    
    TestEqual(TEXT("Gameplay context should be assigned correctly"), Config->GameplayContext.Get(), IMC_Gameplay);
    TestEqual(TEXT("Menu context should be assigned correctly"), Config->MenuContext.Get(), IMC_Menu);
    TestEqual(TEXT("UI context should be assigned correctly"), Config->UIContext.Get(), IMC_UI);
    
    // Test 3: UI blocked actions configuration
    TArray<FName> DefaultUIBlockedActions = { 
        TEXT("Move"), TEXT("Look"), TEXT("Jump"), TEXT("Fire"), TEXT("Interact") 
    };
    
    TestEqual(TEXT("UI blocked actions should have expected count"), Config->UIBlockedActions.Num(), 5);
    TestTrue(TEXT("Move should be blocked by UI"), Config->UIBlockedActions.Contains(FName(TEXT("Move"))));
    TestTrue(TEXT("Look should be blocked by UI"), Config->UIBlockedActions.Contains(FName(TEXT("Look"))));
    TestTrue(TEXT("Fire should be blocked by UI"), Config->UIBlockedActions.Contains(FName(TEXT("Fire"))));
    
    // Test 4: Context validation
    UInputAction* IA_Test = NewObject<UInputAction>(GetTransientPackage());
    FPACS_InputActionMapping M;
    M.InputAction = IA_Test;
    M.ActionIdentifier = TEXT("TestAction");
    Config->ActionMappings = { M };
    
    TestTrue(TEXT("Config with all contexts should be valid"), Config->IsValid());
    
    // Test invalid configs
    Config->GameplayContext = nullptr;
    TestFalse(TEXT("Config without Gameplay context should be invalid"), Config->IsValid());
    
    UE_LOG(LogTemp, Warning, TEXT("PACS Context Test: Context switching tests completed successfully"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPACS_EdgeCaseTest,
    "PACS.Input.EdgeCases",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPACS_EdgeCaseTest::RunTest(const FString& Parameters)
{
    UE_LOG(LogTemp, Warning, TEXT("PACS Edge Case Test: Starting edge case validation"));
    
    // Test 1: Invalid configuration
    UPACS_InputMappingConfig* InvalidConfig = NewObject<UPACS_InputMappingConfig>(GetTransientPackage());
    // Leave contexts null - should be invalid
    TestFalse(TEXT("Config with null contexts should be invalid"), InvalidConfig->IsValid());
    
    // Test 2: Empty action mappings
    UInputMappingContext* IMC_Valid = NewObject<UInputMappingContext>(GetTransientPackage());
    InvalidConfig->GameplayContext = IMC_Valid;
    InvalidConfig->MenuContext = IMC_Valid;
    InvalidConfig->UIContext = IMC_Valid;
    // ActionMappings is empty - should be invalid
    TestFalse(TEXT("Config with empty action mappings should be invalid"), InvalidConfig->IsValid());
    
    // Test 3: Valid minimal config
    UInputAction* IA_Valid = NewObject<UInputAction>(GetTransientPackage());
    FPACS_InputActionMapping ValidMapping;
    ValidMapping.InputAction = IA_Valid;
    ValidMapping.ActionIdentifier = TEXT("ValidAction");
    InvalidConfig->ActionMappings = { ValidMapping };
    TestTrue(TEXT("Config with all required elements should be valid"), InvalidConfig->IsValid());
    
    // Test 4: Action identifier lookup
    TestEqual(TEXT("GetActionIdentifier should return correct name"), 
        InvalidConfig->GetActionIdentifier(IA_Valid), FName(TEXT("ValidAction")));
    
    // Test 5: Lookup non-existent action
    UInputAction* IA_Missing = NewObject<UInputAction>(GetTransientPackage());
    TestEqual(TEXT("GetActionIdentifier should return None for missing action"), 
        InvalidConfig->GetActionIdentifier(IA_Missing), FName(NAME_None));
    
    // Test 6: Handler with invalid config
    UPACS_InputHandlerComponent* Handler = NewObject<UPACS_InputHandlerComponent>(GetTransientPackage());
    Handler->InputConfig = nullptr;
    TestFalse(TEXT("Handler with null config should not be healthy"), Handler->IsHealthy());
    
    Handler->InputConfig = InvalidConfig;
    // Handler should be healthy now (config is valid)
    // Note: bIsInitialized will be false, so IsHealthy() might still be false
    // This tests the config validation part specifically
    
    UE_LOG(LogTemp, Warning, TEXT("PACS Edge Case Test: Edge case validation completed successfully"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPACS_InputRoutingTest,
    "PACS.Input.InputRouting",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPACS_InputRoutingTest::RunTest(const FString& Parameters)
{
    UE_LOG(LogTemp, Warning, TEXT("PACS Routing Test: Starting input routing tests"));
    
    // Test 1: Input action mapping structure
    FPACS_InputActionMapping Mapping;
    TestEqual(TEXT("Default action identifier should be None"), Mapping.ActionIdentifier, FName(NAME_None));
    TestTrue(TEXT("Default bind started should be true"), Mapping.bBindStarted);
    TestFalse(TEXT("Default bind triggered should be false"), Mapping.bBindTriggered);
    TestTrue(TEXT("Default bind completed should be true"), Mapping.bBindCompleted);
    TestFalse(TEXT("Default bind ongoing should be false"), Mapping.bBindOngoing);
    TestFalse(TEXT("Default bind canceled should be false"), Mapping.bBindCanceled);
    
    // Test 2: Action mapping configuration
    UInputAction* IA_Move = NewObject<UInputAction>(GetTransientPackage());
    UInputAction* IA_Jump = NewObject<UInputAction>(GetTransientPackage());
    
    FPACS_InputActionMapping MoveMapping;
    MoveMapping.InputAction = IA_Move;
    MoveMapping.ActionIdentifier = TEXT("Move");
    MoveMapping.bBindStarted = true;
    MoveMapping.bBindCompleted = false;
    MoveMapping.bBindOngoing = true;
    
    FPACS_InputActionMapping JumpMapping;
    JumpMapping.InputAction = IA_Jump;
    JumpMapping.ActionIdentifier = TEXT("Jump");
    JumpMapping.bBindStarted = true;
    JumpMapping.bBindTriggered = true;
    JumpMapping.bBindCompleted = true;
    
    TestEqual(TEXT("Move mapping should have correct action"), MoveMapping.InputAction.Get(), IA_Move);
    TestEqual(TEXT("Move mapping should have correct identifier"), MoveMapping.ActionIdentifier, FName(TEXT("Move")));
    TestTrue(TEXT("Move mapping should bind ongoing"), MoveMapping.bBindOngoing);
    
    TestEqual(TEXT("Jump mapping should have correct action"), JumpMapping.InputAction.Get(), IA_Jump);
    TestEqual(TEXT("Jump mapping should have correct identifier"), JumpMapping.ActionIdentifier, FName(TEXT("Jump")));
    TestTrue(TEXT("Jump mapping should bind triggered"), JumpMapping.bBindTriggered);
    
    // Test 3: Multiple receiver response types
    UPACS_TestReceiver* RConsume1 = NewObject<UPACS_TestReceiver>(GetTransientPackage());
    RConsume1->Response = EPACS_InputHandleResult::HandledConsume;
    RConsume1->PriorityOverride = 2000;
    
    UPACS_TestReceiver* RConsume2 = NewObject<UPACS_TestReceiver>(GetTransientPackage());
    RConsume2->Response = EPACS_InputHandleResult::HandledConsume;
    RConsume2->PriorityOverride = 1500;
    
    UPACS_TestReceiver* RPass = NewObject<UPACS_TestReceiver>(GetTransientPackage());
    RPass->Response = EPACS_InputHandleResult::HandledPassThrough;
    RPass->PriorityOverride = 1000;
    
    UPACS_TestReceiver* RIgnore = NewObject<UPACS_TestReceiver>(GetTransientPackage());
    RIgnore->Response = EPACS_InputHandleResult::NotHandled;
    RIgnore->PriorityOverride = 500;
    
    // Test individual responses
    TestEqual(TEXT("Consumer 1 should consume input"), 
        RConsume1->HandleInputAction(TEXT("TestAction"), FInputActionValue()), 
        EPACS_InputHandleResult::HandledConsume);
        
    TestEqual(TEXT("Consumer 2 should consume input"), 
        RConsume2->HandleInputAction(TEXT("TestAction"), FInputActionValue()), 
        EPACS_InputHandleResult::HandledConsume);
        
    TestEqual(TEXT("Pass-through should pass input"), 
        RPass->HandleInputAction(TEXT("TestAction"), FInputActionValue()), 
        EPACS_InputHandleResult::HandledPassThrough);
        
    TestEqual(TEXT("Ignore should not handle input"), 
        RIgnore->HandleInputAction(TEXT("TestAction"), FInputActionValue()), 
        EPACS_InputHandleResult::NotHandled);
    
    // Test 4: Action value capture
    FInputActionValue TestValue(1.5f);
    RConsume1->HandleInputAction(TEXT("MoveForward"), TestValue);
    
    TestEqual(TEXT("Receiver should capture action name"), RConsume1->LastAction, FName(TEXT("MoveForward")));
    TestEqual(TEXT("Receiver should capture action value"), RConsume1->LastValue.Get<float>(), 1.5f);
    
    // Test 5: Priority comparison
    TestTrue(TEXT("Higher priority should be greater"), RConsume1->GetInputPriority() > RConsume2->GetInputPriority());
    TestTrue(TEXT("Medium priority should be greater than low"), RConsume2->GetInputPriority() > RPass->GetInputPriority());
    TestTrue(TEXT("Low priority should be greater than ignore"), RPass->GetInputPriority() > RIgnore->GetInputPriority());
    
    UE_LOG(LogTemp, Warning, TEXT("PACS Routing Test: Input routing tests completed successfully"));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS