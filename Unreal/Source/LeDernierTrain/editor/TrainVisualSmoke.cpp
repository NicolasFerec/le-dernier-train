#if !UE_BUILD_SHIPPING
#include "../TrainPrototype.h"
#include "../TrainSettings.h"
#include "../network/TrainNetwork.h"
#include "../network/TrainMelee.h"
#include "../network/TrainDash.h"
#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "InputKeyEventArgs.h"
#include "Containers/Ticker.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/IConsoleManager.h"
#include "UnrealClient.h"

namespace
{
    struct FVisualSmoke
    {
        double Began=FPlatformTime::Seconds(),Since=Began;
        int32 Step=0,Frame=0;
        bool bMotionStarted=false;
        TWeakObjectPtr<ATrainEnemy> Enemy;
        float Health=100;
        double DodgeStartZ=0,DodgePeakZ=0;
        void Next() { ++Step;Frame=0;bMotionStarted=false;Since=FPlatformTime::Seconds(); }
        void Pose(ATrainPawn* Pawn,FVector Location,float Yaw=0)
        {
            Pawn->SetActorLocationAndRotation(Location,FRotator(0,Yaw,0),false,nullptr,ETeleportType::TeleportPhysics);
            Pawn->GetCharacterMovement()->StopMovementImmediately();
        }
        void Move(APlayerController* PC,bool Pressed)
        {
            for (FName Action:{FName(TEXT("MoveUp")),FName(TEXT("MoveRight"))})
                PC->InputKey(FInputKeyEventArgs::CreateSimulated(UTrainSettings::Get()->Key(Action,false),Pressed?IE_Pressed:IE_Released,Pressed?1:0));
        }
        void Capture(const TCHAR* Name)
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("PrototypeCaptures")/Name,true,false);
            UE_LOG(LogTemp,Display,TEXT("Train visual capture: %s"),Name);
        }
        void CaptureMotion(const TCHAR* Name,double Time,double Start,double Interval,int32 Count)
        {
            if (Frame<Count && Time>=Start+Frame*Interval)
                Capture(*FString::Printf(TEXT("%s-%02d.png"),Name,++Frame));
        }
        bool Tick(float Delta)
        {
            UWorld* W=nullptr;for (const auto& C:GEngine->GetWorldContexts()) if (C.WorldType==EWorldType::Game) W=C.World();
            auto* PC=W?W->GetFirstPlayerController():nullptr;auto* Pawn=PC?Cast<ATrainPawn>(PC->GetPawn()):nullptr;
            auto* PS=Pawn?Pawn->GetPlayerState<ATrainPlayerState>():nullptr;
            const double Now=FPlatformTime::Seconds(),Time=Now-Since;
            if (Now-Began>35) { UE_LOG(LogTemp,Error,TEXT("Train visual smoke timed out at step %d"),Step);FPlatformMisc::RequestExitWithStatus(false,1);return false; }
            if (!W || !Pawn || !PS) return true;
            const FString Map=UGameplayStatics::GetCurrentLevelName(W);
            auto* ASC=Pawn->GetAbilitySystemComponent();
            if (Step==0 && Map==TEXT("Wagon") && Time>1.5) { Pose(Pawn,FVector(-1450,100,95),225);Next(); }
            else if (Step==1 && Time>.85) { Capture(TEXT("wagon-arsenal-final.png"));Next(); }
            else if (Step==2 && Time>.2) { Pose(Pawn,FVector(900,-180,95));Move(PC,true);Next(); }
            else if (Step==3) { CaptureMotion(TEXT("conducteur-course-sabre"),Time,.25,.14,6);if (Time>1.10) Next(); }
            else if (Step==4 && Time>.25) { Move(PC,false);Pose(Pawn,FVector(1200,-230,95));Next(); }
            else if (Step==5 && Time>.55) { Pose(Pawn,FVector(1200,-230,95));ASC->TryActivateAbilityByClass(UTrainLightAttack::StaticClass());Next(); }
            else if (Step==6) { CaptureMotion(TEXT("conducteur-sabre-legere"),Time,.06,.09,4);if (Time>.8) Next(); }
            else if (Step==7)
            {
                if (!bMotionStarted) bMotionStarted=ASC->TryActivateAbilityByClass(UTrainHeavyAttack::StaticClass());
                CaptureMotion(TEXT("conducteur-sabre-lourde"),Time,.06,.12,6);if (Time>1.05) Next();
            }
            else if (Step==8)
            {
                if (!bMotionStarted) { Pawn->ServerLoadout(0,1,false,false);Pose(Pawn,FVector(900,-180,95));Move(PC,true);bMotionStarted=true; }
                CaptureMotion(TEXT("conducteur-course-marteau"),Time,.25,.14,6);if (Time>1.1) Next();
            }
            else if (Step==9 && Time>.25) { Move(PC,false);Pose(Pawn,FVector(1200,-230,95));Next(); }
            else if (Step==10 && Time>.55) { ASC->TryActivateAbilityByClass(UTrainLightAttack::StaticClass());Next(); }
            else if (Step==11) { CaptureMotion(TEXT("conducteur-marteau-legere"),Time,.06,.10,5);if (Time>.9) Next(); }
            else if (Step==12)
            {
                if (!bMotionStarted) bMotionStarted=ASC->TryActivateAbilityByClass(UTrainHeavyAttack::StaticClass());
                CaptureMotion(TEXT("conducteur-marteau-lourde"),Time,.06,.12,6);if (Time>1.05) Next();
            }
            else if (Step==13)
            {
                if (!bMotionStarted)
                {
                    Pose(Pawn,FVector(820,-230,95));
                    DodgeStartZ=Pawn->GetActorLocation().Z;
                    DodgePeakZ=DodgeStartZ;
                    bMotionStarted=ASC->TryActivateAbilityByClass(UTrainDash::StaticClass());
                }
                const FVector Location=Pawn->GetActorLocation();
                DodgePeakZ=FMath::Max(DodgePeakZ,Location.Z);
                CaptureMotion(TEXT("conducteur-esquive"),Time,.04,.055,6);
                if (Time>.75)
                {
                    const double Distance=Location.X-820;
                    const double Rise=DodgePeakZ-DodgeStartZ;
                    const bool Landed=Pawn->GetCharacterMovement()->IsMovingOnGround();
                    UE_LOG(LogTemp, Display, TEXT("Dodge smoke: %.1f cm forward, %.1f cm up, landed=%d"),Distance,Rise,Landed);
                    if (!Landed || Distance < 150 || Distance > 235 || Rise < 12 || Rise > 40)
                    {
                        UE_LOG(LogTemp, Error, TEXT("Dodge did not make a short horizontal leap"));
                        FPlatformMisc::RequestExitWithStatus(false,1);
                        return false;
                    }
                    Next();
                }
            }
            else if (Step==14 && Time>.65) { W->ServerTravel(TEXT("/Game/Maps/Pompage"));Next(); }
            else if (Step==15 && Map==TEXT("Pompage") && Time>.75)
            {
                TActorIterator<ATrainEnemy> First(W);if (!First) return true;
                Enemy=*First;const FVector P=Enemy->GetActorLocation();Pose(Pawn,FVector(P.X-300,P.Y-40,95),7);
                Health=PS->Health;Next();
            }
            else if (Step==16 && Time>.7) { Capture(TEXT("pompage-automate.png"));Next(); }
            else if (Step==17 && PS->Health<Health) { Capture(TEXT("pompage-degats-recus.png"));Next(); }
            else if (Step==18 && Time>.28 && Enemy.IsValid())
            {
                Pawn->SetActorRotation(FRotator(0,(Enemy->GetActorLocation()-Pawn->GetActorLocation()).Rotation().Yaw,0));
                ASC->TryActivateAbilityByClass(UTrainHeavyAttack::StaticClass());Next();
            }
            else if (Step==19 && Time>.33) { Capture(TEXT("pompage-degats-infliges.png"));Next(); }
            else if (Step==20 && Time>.7) { W->ServerTravel(TEXT("/Game/Maps/Fonderie"));Next(); }
            else if (Step==21 && Map==TEXT("Fonderie") && Time>.6) { Pose(Pawn,FVector(1050,730,95),225);Next(); }
            else if (Step==22 && Time>.8) { Capture(TEXT("halle-des-turbines.png"));Next(); }
            else if (Step==23 && Time>.5)
            {
                UE_LOG(LogTemp,Display,TEXT("TRAIN_VISUAL_SMOKE_COMPLETE"));FPlatformMisc::RequestExit(false);return false;
            }
            return true;
        }
    };
    FAutoConsoleCommand VisualSmoke(TEXT("Train.VisualSmoke"),TEXT("Development: capture actual gameplay, both weapons, feedback and two rooms."),
        FConsoleCommandDelegate::CreateLambda([]()
        {
            if (!IsRunningGame() || !FParse::Param(FCommandLine::Get(),TEXT("TrainVisualSmoke"))) return;
            auto Test=MakeShared<FVisualSmoke>();FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Test](float Delta){return Test->Tick(Delta);}));
        }));
    FAutoConsoleCommandWithWorldAndArgs PerfView(TEXT("Train.PerfView"),
        TEXT("Development profiling: fix the player's position (X Y [Yaw])."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
        {
            if (!IsRunningGame() || !FParse::Param(FCommandLine::Get(), TEXT("TrainPerf")) || Args.Num() < 2) return;
            auto* Controller = World ? World->GetFirstPlayerController() : nullptr;
            auto* Pawn = Controller ? Cast<ATrainPawn>(Controller->GetPawn()) : nullptr;
            if (!Pawn) return;
            Controller->SetIgnoreMoveInput(true);
            Pawn->GetCharacterMovement()->StopMovementImmediately();
            const float Yaw = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 0.f;
            Pawn->SetActorLocationAndRotation(FVector(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]), 95),
                FRotator(0, Yaw, 0), false, nullptr, ETeleportType::TeleportPhysics);
        }));
}
#endif
