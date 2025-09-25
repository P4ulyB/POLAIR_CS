#include "Tests/PACS_Heli_DataSpec.h"
#include "Data/Configs/PACS_CandidateHelicopterData.h"
#include "UObject/SoftObjectPtr.h"

bool FPACS_Heli_DataSpec::RunTest(const FString& Parameters)
{
    // Designers must create a data asset named /Game/PACS/Data/DA_CandidateHelicopter
    const TSoftObjectPtr<UPACS_CandidateHelicopterData> SoftDA(FSoftObjectPath(TEXT("/Game/PACS/Data/DA_CandidateHelicopter.DA_CandidateHelicopter")));
    UPACS_CandidateHelicopterData* DA = SoftDA.LoadSynchronous();
    TestNotNull(TEXT("Data asset exists"), DA);
    if (!DA) return false;

    TestTrue(TEXT("Default Altitude > 0"), DA->DefaultAltitudeCm > 0.f);
    TestTrue(TEXT("Default Radius > 0"),   DA->DefaultRadiusCm   > 0.f);
    TestTrue(TEXT("Default Speed >= 0"),   DA->DefaultSpeedCms   >= 0.f);
    TestTrue(TEXT("MaxSpeed >= DefaultSpeed"), DA->MaxSpeedCms >= DA->DefaultSpeedCms);
    TestTrue(TEXT("MaxBankDeg in [0,10]"), DA->MaxBankDeg >= 0.f && DA->MaxBankDeg <= 10.f);
    return true;
}