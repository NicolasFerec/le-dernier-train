#if !UE_BUILD_SHIPPING
#include "../TrainPrototype.h"
#include "../TrainSettings.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SWindow.h"
#include "UnrealClient.h"
#include "../network/TrainNetwork.h"
#include "../network/TrainSession.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMemory.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "DynamicRHI.h"

namespace
{
    struct FTrainBenchmark
    {
        int32 Case=0,Stage=0,Enemies=0;
        bool ProfileRequested=false;
        double Start=FPlatformTime::Seconds(),Since=Start,Previous=Start,PeakMiB=0;
        TArray<double> Frames,GPU;
        FString Rows=TEXT("map,seed,enemies,samples,mean_fps,p50_ms,p95_ms,p99_ms,gpu_mean_ms,peak_process_mib\n");
        const TCHAR* Maps[3]={TEXT("Wagon"),TEXT("Faubourgs06"),TEXT("Faubourgs10")};
        bool Tick(float)
        {
            const double Now=FPlatformTime::Seconds(),FrameMs=(Now-Previous)*1000.;Previous=Now;
            if (Now-Start>150) { UE_LOG(LogTemp,Error,TEXT("TRAIN_BENCHMARK_TIMEOUT"));FPlatformMisc::RequestExitWithStatus(false,1);return false; }
            UWorld* W=nullptr;for (const auto& C:GEngine->GetWorldContexts()) if (C.WorldType==EWorldType::Game) W=C.World();
            auto* PC=W?W->GetFirstPlayerController():nullptr;
            auto* Pawn=PC?Cast<ATrainPawn>(PC->GetPawn()):nullptr;
            auto* GS=W?W->GetGameState<ATrainGameState>():nullptr;
            auto* PS=Pawn?Pawn->GetPlayerState<ATrainPlayerState>():nullptr;
            if (!Pawn||!PS||!GS||!GS->RoomReady||UGameplayStatics::GetCurrentLevelName(W)!=Maps[Case]) return true;
            PS->Health=PS->MaxHealth=100000; // Only this opt-in benchmark is invulnerable; enemy stats are unchanged.
            if (Stage==0)
            {
                PC->ConsoleCommand(TEXT("t.MaxFPS 0"));PC->ConsoleCommand(TEXT("r.VSync 0"));if (FParse::Param(FCommandLine::Get(),TEXT("TrainNativeRendering"))) PC->ConsoleCommand(TEXT("r.ScreenPercentage 100"));PC->SetIgnoreMoveInput(true);Pawn->DisableInput(PC);
                if (FParse::Param(FCommandLine::Get(),TEXT("TrainBenchmarkScout"))) Pawn->ServerLoadout(1,0,false,false);
                int32 Width=0,Height=0;PC->GetViewportSize(Width,Height);
                UE_LOG(LogTemp,Display,TEXT("TRAIN_BENCHMARK_VIEWPORT %dx%d screen_percentage=%.1f"),Width,Height,IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"))->GetFloat());
                Pawn->SetActorLocationAndRotation(Case?FVector(500,0,95):FVector(-1450,100,95),FRotator(0,225,0),false,nullptr,ETeleportType::TeleportPhysics);
                Pawn->GetCharacterMovement()->StopMovementImmediately();
                Enemies=0;for(TActorIterator<ATrainEnemy> It(W);It;++It) ++Enemies;
                Stage=1;Since=Now;ProfileRequested=false;
            }
            else if (Stage==1 && Now-Since>3 && !ProfileRequested && FParse::Param(FCommandLine::Get(),TEXT("TrainProfileGPU")))
            {
                ProfileRequested=true;
                PC->ConsoleCommand(TEXT("r.ProfileGPU.ShowUI 0"));PC->ConsoleCommand(TEXT("ProfileGPU"));
            }
            else if (Stage==1 && Now-Since>6)
            {
                int32 Width=0,Height=0;PC->GetViewportSize(Width,Height);
                UE_LOG(LogTemp,Display,TEXT("TRAIN_BENCHMARK_RENDER %dx%d screen_percentage=%.1f settings_mode=%d window_mode=%d"),Width,Height,IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"))->GetFloat(),int32(UTrainSettings::Get()->GetFullscreenMode()),int32(W->GetGameViewport()->GetWindow()->GetWindowMode()));
                Stage=2;Since=Now;Frames.Reset();GPU.Reset();PeakMiB=0;
            }
            else if (Stage==2)
            {
                Frames.Add(FrameMs);GPU.Add(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
                PeakMiB=FMath::Max(PeakMiB,FPlatformMemory::GetStats().UsedPhysical/(1024.*1024.));
                if (Now-Since<12) return true;
                double Sum=0,GpuSum=0;for(double V:Frames)Sum+=V;for(double V:GPU)GpuSum+=V;
                Frames.Sort();auto Percentile=[&](double P){return Frames[FMath::Clamp(FMath::CeilToInt(P*Frames.Num())-1,0,Frames.Num()-1)];};
                const FString Row=FString::Printf(TEXT("%s,%d,%d,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.1f\n"),Maps[Case],GS->RoomSeed,Enemies,Frames.Num(),1000.*Frames.Num()/Sum,Percentile(.5),Percentile(.95),Percentile(.99),GpuSum/GPU.Num(),PeakMiB);
                Rows+=Row;UE_LOG(LogTemp,Display,TEXT("TRAIN_BENCHMARK %s"),*Row);
                if (++Case==3)
                {
                    FString Label=TEXT("review");FParse::Value(FCommandLine::Get(),TEXT("TrainBenchmarkLabel="),Label);
                    const FString Dir=FPaths::ProjectSavedDir()/TEXT("Benchmarks");IFileManager::Get().MakeDirectory(*Dir,true);
                    const bool Saved=FFileHelper::SaveStringToFile(Rows,*(Dir/Label+TEXT(".csv")));
                    FPlatformMisc::RequestExitWithStatus(false,Saved?0:1);return false;
                }
                auto* Session=W->GetGameInstance<UTrainSession>();Session->ExpeditionSeed=101;Session->ExpeditionCrew=4;
                Stage=0;W->ServerTravel(FString(TEXT("/Game/Maps/"))+Maps[Case]);
            }
            return true;
        }
    };
    FAutoConsoleCommand Benchmark(TEXT("Train.Benchmark"),TEXT("Development: warm rendered solo and four-player encounter workloads, then write timing percentiles."),FConsoleCommandDelegate::CreateLambda([]()
    {
        if (!IsRunningGame()||!FParse::Param(FCommandLine::Get(),TEXT("TrainBenchmark"))) return;
        auto Test=MakeShared<FTrainBenchmark>();FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Test](float Delta){return Test->Tick(Delta);}));
    }));
}
#endif
