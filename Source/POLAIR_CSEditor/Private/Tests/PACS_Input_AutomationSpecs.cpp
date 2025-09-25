#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "InputAction.h"
#include "InputMappingContext.h"

#include "Data/Configs/PACS_InputMappingConfig.h"
#include "Data/PACS_InputTypes.h" // EPACS_* types, FPACS_InputReceiverEntry, PACS_InputLimits, etc.

// ------- Spec 1: Config validity & identifier lookup -------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPACS_InputConfigValiditySpec,
    "PACS.Input.InputConfig.ValidityAndLookup",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPACS_InputConfigValiditySpec::RunTest(const FString& Parameters)
{
    UPACS_InputMappingConfig* Config = NewObject<UPACS_InputMappingConfig>(GetTransientPackage());
    TestNotNull(TEXT("Config allocated"), Config);

    UInputMappingContext* IMC_Gameplay = NewObject<UInputMappingContext>(GetTransientPackage());
    UInputMappingContext* IMC_Menu     = NewObject<UInputMappingContext>(GetTransientPackage());
    UInputMappingContext* IMC_UI       = NewObject<UInputMappingContext>(GetTransientPackage());
    TestNotNull(TEXT("Gameplay IMC"), IMC_Gameplay);
    TestNotNull(TEXT("Menu IMC"), IMC_Menu);
    TestNotNull(TEXT("UI IMC"), IMC_UI);

    Config->GameplayContext = IMC_Gameplay;
    Config->MenuContext     = IMC_Menu;
    Config->UIContext       = IMC_UI;

    UInputAction* IA_Move = NewObject<UInputAction>(GetTransientPackage());
    IA_Move->Rename(TEXT("IA_Move"));
    FPACS_InputActionMapping MapMove;
    MapMove.InputAction      = IA_Move;
    MapMove.ActionIdentifier = TEXT("Move");

    UInputAction* IA_Fire = NewObject<UInputAction>(GetTransientPackage());
    IA_Fire->Rename(TEXT("IA_Fire"));
    FPACS_InputActionMapping MapFire;
    MapFire.InputAction      = IA_Fire;
    MapFire.ActionIdentifier = TEXT("Fire");

    Config->ActionMappings = { MapMove, MapFire };

    TestTrue(TEXT("Config reports valid"), Config->IsValid());
    TestEqual(TEXT("Lookup Move by action ptr"), Config->GetActionIdentifier(IA_Move), FName(TEXT("Move")));
    TestEqual(TEXT("Lookup Fire by action ptr"), Config->GetActionIdentifier(IA_Fire), FName(TEXT("Fire")));

    // Guardrail: exceed mapping cap
    TArray<FPACS_InputActionMapping> Big;
    Big.SetNum(PACS_InputLimits::MaxActionsPerConfig + 1);
    Config->ActionMappings = Big;
    TestFalse(TEXT("Too many mappings -> invalid"), Config->IsValid());

    return true;
}

// ------- Spec 2: Receiver ordering (priority desc, FIFO for equals) -------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPACS_ReceiverOrderingSpec,
    "PACS.Input.Receivers.Ordering",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPACS_ReceiverOrderingSpec::RunTest(const FString& Parameters)
{
    FPACS_InputReceiverEntry A; A.Priority = 400;  A.RegistrationOrder = 1;
    FPACS_InputReceiverEntry B; B.Priority = 1000; B.RegistrationOrder = 2;
    FPACS_InputReceiverEntry C; C.Priority = 400;  C.RegistrationOrder = 3;

    TArray<FPACS_InputReceiverEntry> Arr = { A, B, C };
    Arr.Sort();

    TestTrue(TEXT("First is highest priority (B)"), Arr[0].Priority == 1000);
    TestTrue(TEXT("Second is A (FIFO within 400)"), Arr[1].Priority == 400 && Arr[1].RegistrationOrder == 1);
    TestTrue(TEXT("Third is C (FIFO within 400)"),  Arr[2].Priority == 400 && Arr[2].RegistrationOrder == 3);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS