// Performance Validation Helper
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "Subsystems/PACS_CharacterPool.h"
#include "Subsystems/PACS_NPCSpawnManager.h"
#include "Actors/NPC/PACS_NPCCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/ThreadManager.h"
#include "Async/TaskGraphInterfaces.h"
#include "RHI.h"
#include "RenderingThread.h"
#include "HAL/PlatformTLS.h"
#include "Misc/App.h"

namespace PACS_Performance
{
    static void ValidateCharacterPooling(UWorld* World)
    {
        if (!World) return;

        auto* GameInstance = World->GetGameInstance();
        if (!GameInstance) return;

        auto* CharacterPool = GameInstance->GetSubsystem<UPACS_CharacterPool>();
        if (!CharacterPool)
        {
            UE_LOG(LogTemp, Error, TEXT("[PERF] CharacterPool subsystem not found"));
            return;
        }

        int32 TotalPooled, InUse, Available;
        CharacterPool->GetPoolStatistics(TotalPooled, InUse, Available);

        UE_LOG(LogTemp, Display, TEXT("[PERF] Pool Stats - Total: %d, InUse: %d, Available: %d"),
            TotalPooled, InUse, Available);

        // Verify characters are properly configured
        for (TActorIterator<APACS_NPCCharacter> It(World); It; ++It)
        {
            APACS_NPCCharacter* NPC = *It;
            if (!NPC) continue;

            bool bIsPooled = NPC->IsPooledCharacter();
            auto* Movement = NPC->GetCharacterMovement();
            auto* Mesh = NPC->GetMesh();

            float TickInterval = NPC->GetActorTickInterval();
            bool bMovementTicking = Movement ? Movement->IsComponentTickEnabled() : false;

            UE_LOG(LogTemp, Display, TEXT("[PERF] NPC %s - Pooled:%d, TickInterval:%.2f, MovementTick:%d"),
                *NPC->GetName(),
                bIsPooled,
                TickInterval,
                bMovementTicking);
        }
    }

    static void MeasurePerformanceMetrics(UWorld* World)
    {
        if (!World) return;

        int32 TotalNPCs = 0;
        int32 NPCsWithMovementTick = 0;
        int32 NPCsWithHighTickRate = 0;
        int32 NPCsWithAnimationCulling = 0;

        for (TActorIterator<APACS_NPCCharacter> It(World); It; ++It)
        {
            APACS_NPCCharacter* NPC = *It;
            if (!NPC) continue;

            TotalNPCs++;

            auto* Movement = NPC->GetCharacterMovement();
            if (Movement && Movement->IsComponentTickEnabled())
            {
                NPCsWithMovementTick++;
            }

            if (NPC->GetActorTickInterval() < 0.1f)
            {
                NPCsWithHighTickRate++;
            }

            auto* Mesh = NPC->GetMesh();
            if (Mesh && Mesh->VisibilityBasedAnimTickOption != EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones)
            {
                NPCsWithAnimationCulling++;
            }
        }

        UE_LOG(LogTemp, Display, TEXT("[PERF] === Performance Metrics ==="));
        UE_LOG(LogTemp, Display, TEXT("[PERF] Total NPCs: %d"), TotalNPCs);
        UE_LOG(LogTemp, Display, TEXT("[PERF] NPCs with Movement Tick: %d (%.1f%%)"),
            NPCsWithMovementTick,
            TotalNPCs > 0 ? (100.0f * NPCsWithMovementTick / TotalNPCs) : 0.0f);
        UE_LOG(LogTemp, Display, TEXT("[PERF] NPCs with High Tick Rate: %d (%.1f%%)"),
            NPCsWithHighTickRate,
            TotalNPCs > 0 ? (100.0f * NPCsWithHighTickRate / TotalNPCs) : 0.0f);
        UE_LOG(LogTemp, Display, TEXT("[PERF] NPCs with Animation Culling: %d (%.1f%%)"),
            NPCsWithAnimationCulling,
            TotalNPCs > 0 ? (100.0f * NPCsWithAnimationCulling / TotalNPCs) : 0.0f);
    }

    static void DiagnoseTickSettings(UWorld* World)
    {
        if (!World) return;

        UE_LOG(LogTemp, Display, TEXT("========================================"));
        UE_LOG(LogTemp, Display, TEXT("=== TICK RATE DIAGNOSIS ==="));
        UE_LOG(LogTemp, Display, TEXT("========================================"));

        // World settings
        AWorldSettings* WorldSettings = World->GetWorldSettings();
        if (WorldSettings)
        {
            UE_LOG(LogTemp, Display, TEXT("[TICK] World TimeDilation: %.2f"),
                WorldSettings->GetEffectiveTimeDilation());
            UE_LOG(LogTemp, Display, TEXT("[TICK] World Settings Found: YES"));
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("[TICK] World Settings Found: NO"));
        }

        // Engine settings
        UEngine* Engine = GEngine;
        if (Engine)
        {
            // Calculate current tick rate from World DeltaTime (UE5.5 compatible)
            float CurrentTickRate = 0.0f;
            if (World)
            {
                float DeltaTime = World->GetDeltaSeconds();
                CurrentTickRate = (DeltaTime > 0.0f) ? (1.0f / DeltaTime) : 0.0f;
            }
            UE_LOG(LogTemp, Display, TEXT("[TICK] Current Tick Rate: %.2f"),
                CurrentTickRate);
            UE_LOG(LogTemp, Display, TEXT("[TICK] Engine MaxFPS: %.2f"),
                Engine->GetMaxFPS());
            UE_LOG(LogTemp, Display, TEXT("[TICK] Engine SmoothFrameRate: %s"),
                Engine->bSmoothFrameRate ? TEXT("ENABLED") : TEXT("DISABLED"));
            UE_LOG(LogTemp, Display, TEXT("[TICK] Engine FixedFrameRate: %s"),
                Engine->bUseFixedFrameRate ? TEXT("ENABLED") : TEXT("DISABLED"));

            if (Engine->bUseFixedFrameRate)
            {
                UE_LOG(LogTemp, Display, TEXT("[TICK] Fixed Frame Rate: %.2f"),
                    Engine->FixedFrameRate);
            }
        }

        // Network settings
        if (World->GetNetDriver())
        {
            UNetDriver* NetDriver = World->GetNetDriver();
            UE_LOG(LogTemp, Display, TEXT("[NET] NetDriver MaxTickRate: %d"),
                NetDriver->GetNetServerMaxTickRate());
            UE_LOG(LogTemp, Display, TEXT("[NET] Is Server: %s"),
                NetDriver->IsServer() ? TEXT("YES") : TEXT("NO"));
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("[NET] No NetDriver (Single Player)"));
        }

        // Runtime environment
        UE_LOG(LogTemp, Display, TEXT("[ENV] Is Dedicated Server: %s"),
            IsRunningDedicatedServer() ? TEXT("YES") : TEXT("NO"));
        UE_LOG(LogTemp, Display, TEXT("[ENV] Is Game: %s"),
            FApp::IsGame() ? TEXT("YES") : TEXT("NO"));
        UE_LOG(LogTemp, Display, TEXT("[ENV] With Editor: %s"),
            WITH_EDITOR ? TEXT("YES") : TEXT("NO"));

        // Frame timing
        float CurrentDeltaTime = World->GetDeltaSeconds();
        float CurrentFPS = CurrentDeltaTime > 0.0f ? 1.0f / CurrentDeltaTime : 0.0f;
        UE_LOG(LogTemp, Display, TEXT("[TIMING] Current DeltaTime: %.4f ms (%.1f FPS)"),
            CurrentDeltaTime * 1000.0f, CurrentFPS);
        UE_LOG(LogTemp, Display, TEXT("[TIMING] Real Time Seconds: %.3f"),
            World->GetRealTimeSeconds());
        UE_LOG(LogTemp, Display, TEXT("[TIMING] Time Seconds: %.3f"),
            World->GetTimeSeconds());
    }

    static void DiagnoseTaskSystem(UWorld* World)
    {
        if (!World) return;

        UE_LOG(LogTemp, Display, TEXT("========================================"));
        UE_LOG(LogTemp, Display, TEXT("=== TASK SYSTEM DIAGNOSIS ==="));
        UE_LOG(LogTemp, Display, TEXT("========================================"));

        // Task graph info
        UE_LOG(LogTemp, Display, TEXT("[TASK] Task Graph Worker Threads: %d"),
            FTaskGraphInterface::Get().GetNumWorkerThreads());
        UE_LOG(LogTemp, Display, TEXT("[TASK] Game Thread ID: %d"),
            FPlatformTLS::GetCurrentThreadId());

        // Threading info
        UE_LOG(LogTemp, Display, TEXT("[THREAD] Is Threaded Rendering: %s"),
            GIsThreadedRendering ? TEXT("YES") : TEXT("NO"));
        UE_LOG(LogTemp, Display, TEXT("[THREAD] Use Render Thread: %s"),
            GUseThreadedRendering ? TEXT("YES") : TEXT("NO"));

        // Current thread context (UE5.5 compatible)
        if (IsInRenderingThread())
        {
            UE_LOG(LogTemp, Display, TEXT("[THREAD] Currently on Rendering Thread"));
        }
        else if (IsInGameThread())
        {
            UE_LOG(LogTemp, Display, TEXT("[THREAD] Currently on Game Thread"));
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("[THREAD] Currently on Worker Thread"));
        }

        // Dedicated server specific checks
        if (IsRunningDedicatedServer())
        {
            UE_LOG(LogTemp, Warning, TEXT("[SERVER] Dedicated server running with threaded rendering: %s"),
                GIsThreadedRendering ? TEXT("YES - POTENTIAL ISSUE") : TEXT("NO - GOOD"));
        }

        // CPU info
        const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
        UE_LOG(LogTemp, Display, TEXT("[CPU] Logical Cores: %d"),
            FPlatformMisc::NumberOfCores());
        UE_LOG(LogTemp, Display, TEXT("[CPU] Physical Cores: %d"),
            FPlatformMisc::NumberOfCoresIncludingHyperthreads());
    }

    static void DiagnoseWaitForTasks(UWorld* World)
    {
        if (!World) return;

        UE_LOG(LogTemp, Display, TEXT("========================================"));
        UE_LOG(LogTemp, Display, TEXT("=== WAIT FOR TASKS ANALYSIS ==="));
        UE_LOG(LogTemp, Display, TEXT("========================================"));

        // Start Unreal Insights trace for detailed analysis
#if WITH_EDITOR
        static bool bTraceStarted = false;
        if (!bTraceStarted)
        {
            UE_LOG(LogTemp, Display, TEXT("[TRACE] Starting Unreal Insights trace..."));
            FString TraceCommand = TEXT("trace.start default,task,loadtime,cpu");
            GEngine->Exec(World, *TraceCommand);
            bTraceStarted = true;

            // Set timer to stop trace after 30 seconds
            FTimerHandle TraceTimer;
            World->GetTimerManager().SetTimer(TraceTimer, []()
            {
                GEngine->Exec(GWorld, TEXT("trace.stop"));
                UE_LOG(LogTemp, Display, TEXT("[TRACE] Trace stopped. Check UnrealInsights for detailed analysis."));
            }, 30.0f, false);
        }
#endif

        // Manual frame timing measurement
        static double LastFrameTime = 0.0;
        static double WorstFrameTime = 0.0;
        static int32 FrameCount = 0;
        static double TotalFrameTime = 0.0;

        double CurrentTime = FPlatformTime::Seconds();
        if (LastFrameTime > 0.0)
        {
            double FrameDuration = CurrentTime - LastFrameTime;
            FrameCount++;
            TotalFrameTime += FrameDuration;

            if (FrameDuration > WorstFrameTime)
            {
                WorstFrameTime = FrameDuration;
            }

            if (FrameCount % 300 == 0) // Every ~5 seconds at 60fps
            {
                double AvgFrameTime = TotalFrameTime / FrameCount;
                UE_LOG(LogTemp, Display, TEXT("[FRAME] Avg: %.2fms, Worst: %.2fms, Frames: %d"),
                    AvgFrameTime * 1000.0, WorstFrameTime * 1000.0, FrameCount);

                if (AvgFrameTime > 0.015) // >15ms average
                {
                    UE_LOG(LogTemp, Warning, TEXT("[FRAME] Frame time above 15ms - investigating..."));

                    // Check common culprits
                    UE_LOG(LogTemp, Display, TEXT("[DEBUG] Actors in world: %d"),
                        World->GetActorCount());
                    UE_LOG(LogTemp, Display, TEXT("[DEBUG] Performance investigation needed - check actor count and tick intervals"));
                }

                // Reset counters
                FrameCount = 0;
                TotalFrameTime = 0.0;
                WorstFrameTime = 0.0;
            }
        }
        LastFrameTime = CurrentTime;

        // Suggest console commands for further analysis
        static bool bSuggestedCommands = false;
        if (!bSuggestedCommands)
        {
            UE_LOG(LogTemp, Display, TEXT("[HELP] Run these console commands for detailed analysis:"));
            UE_LOG(LogTemp, Display, TEXT("[HELP] stat unit - Frame time breakdown"));
            UE_LOG(LogTemp, Display, TEXT("[HELP] stat threading - Thread synchronization"));
            UE_LOG(LogTemp, Display, TEXT("[HELP] stat taskgraph - Task system details"));
            UE_LOG(LogTemp, Display, TEXT("[HELP] t.MaxFPS 0 - Remove frame cap"));
            UE_LOG(LogTemp, Display, TEXT("[HELP] r.VSync 0 - Disable VSync"));
            bSuggestedCommands = true;
        }
    }

    static void RunCompleteSystemDiagnosis(UWorld* World)
    {
        if (!World) return;

        UE_LOG(LogTemp, Display, TEXT(""));
        UE_LOG(LogTemp, Display, TEXT("########################################"));
        UE_LOG(LogTemp, Display, TEXT("# COMPLETE PERFORMANCE SYSTEM DIAGNOSIS"));
        UE_LOG(LogTemp, Display, TEXT("########################################"));
        UE_LOG(LogTemp, Display, TEXT(""));

        DiagnoseTickSettings(World);
        UE_LOG(LogTemp, Display, TEXT(""));

        DiagnoseTaskSystem(World);
        UE_LOG(LogTemp, Display, TEXT(""));

        DiagnoseWaitForTasks(World);
        UE_LOG(LogTemp, Display, TEXT(""));

        // Character pool analysis if available
        auto* GameInstance = World->GetGameInstance();
        if (GameInstance)
        {
            auto* CharacterPool = GameInstance->GetSubsystem<UPACS_CharacterPool>();
            if (CharacterPool)
            {
                ValidateCharacterPooling(World);
                UE_LOG(LogTemp, Display, TEXT(""));
                MeasurePerformanceMetrics(World);
            }
        }

        UE_LOG(LogTemp, Display, TEXT("########################################"));
        UE_LOG(LogTemp, Display, TEXT("# DIAGNOSIS COMPLETE"));
        UE_LOG(LogTemp, Display, TEXT("########################################"));
    }
}

// Console commands for testing
static FAutoConsoleCommand ValidatePoolingCmd(
    TEXT("pacs.ValidatePooling"),
    TEXT("Validate character pooling system"),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        PACS_Performance::ValidateCharacterPooling(World);
    })
);

static FAutoConsoleCommand MeasurePerformanceCmd(
    TEXT("pacs.MeasurePerformance"),
    TEXT("Measure NPC performance metrics"),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        PACS_Performance::MeasurePerformanceMetrics(World);
    })
);

static FAutoConsoleCommand DiagnoseTickCmd(
    TEXT("pacs.DiagnoseTick"),
    TEXT("Diagnose tick rate settings and timing"),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        PACS_Performance::DiagnoseTickSettings(World);
    })
);

static FAutoConsoleCommand DiagnoseTasksCmd(
    TEXT("pacs.DiagnoseTasks"),
    TEXT("Diagnose task system and threading"),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        PACS_Performance::DiagnoseTaskSystem(World);
    })
);

static FAutoConsoleCommand DiagnoseWaitCmd(
    TEXT("pacs.DiagnoseWait"),
    TEXT("Diagnose WaitForTasks issues with frame timing analysis"),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        PACS_Performance::DiagnoseWaitForTasks(World);
    })
);

static FAutoConsoleCommand FullDiagnosisCmd(
    TEXT("pacs.FullDiagnosis"),
    TEXT("Run complete system diagnosis for WaitForTasks investigation"),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        PACS_Performance::RunCompleteSystemDiagnosis(World);
    })
);