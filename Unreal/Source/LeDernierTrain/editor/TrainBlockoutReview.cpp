#if !UE_BUILD_SHIPPING
#include "../TrainPrototype.h"
#include "../network/TrainNetwork.h"
#include "../world/TrainRoomGeneration.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Kismet/GameplayStatics.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
    struct FBlockoutReview
    {
        double Began=FPlatformTime::Seconds(),Since=Began;
        int32 Step=0,Waypoint=0;
        // Walk both branches around the island, then cross to the real exit.
        const TArray<FVector> Route={{-1650,-700,95},{-1100,650,95},{-650,400,95},
            {1050,650,95},{1350,-750,95},{-800,-650,95},{-950,550,95},
            {1350,450,95},{2050,450,95},{2490,450,95},{2600,1250,95},{2820,1570,95}};
        void Capture(const TCHAR* Name)
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("BlockoutReview")/Name,false,false);
        }
        bool Fail(const TCHAR* Why)
        {
            UE_LOG(LogTemp,Error,TEXT("TRAIN_BLOCKOUT_REVIEW_FAILED: %s step=%d waypoint=%d"),Why,Step,Waypoint);
            FPlatformMisc::RequestExitWithStatus(false,1);return false;
        }
        bool Tick(float Delta)
        {
            const double Now=FPlatformTime::Seconds();
            if (Now-Began>100) return Fail(TEXT("timeout"));
            UWorld* W=nullptr;for (const auto& C:GEngine->GetWorldContexts()) if (C.WorldType==EWorldType::Game) W=C.World();
            auto* PC=W?W->GetFirstPlayerController():nullptr;
            auto* P=PC?Cast<ATrainPawn>(PC->GetPawn()):nullptr;
            auto* GS=W?W->GetGameState<ATrainGameState>():nullptr;
            if (!P || !GS || !GS->RoomReady) return true;
            if (Step==5)
            {
                if (UGameplayStatics::GetCurrentLevelName(W)==TEXT("Faubourgs02"))
                {
                    UE_LOG(LogTemp,Display,TEXT("TRAIN_BLOCKOUT_REVIEW_COMPLETE: native movement both routes, 4 starts, 3 reachable enemies, no PCG furniture, exit travel to room 2"));
                    FPlatformMisc::RequestExitWithStatus(false,0);return false;
                }
                return true;
            }
            if (UGameplayStatics::GetCurrentLevelName(W)!=TEXT("QuaiBlockout")) return Fail(TEXT("wrong map"));
            if (Step==0 && Now-Since>3)
            {
                if (GS->RoomIndex!=1 || GS->RemainingEnemies!=3) return Fail(TEXT("room identity or encounter count"));
                if (TActorIterator<ATrainRoomGenerator> Generator(W); Generator) return Fail(TEXT("procedural generator in authored blockout"));
                auto Reachable=[W](FVector A,FVector B)
                {
                    auto* Path=UNavigationSystemV1::FindPathToLocationSynchronously(W,A,B);
                    return Path&&Path->IsValid()&&!Path->IsPartial();
                };
                int32 Starts=0;
                for (TActorIterator<APlayerStart> It(W);It;++It)
                {
                    ++Starts;
                    if (!Reachable(It->GetActorLocation(),GS->DeparturePoint-FVector(180,0,60))) return Fail(TEXT("start disconnected from exit"));
                }
                if (Starts!=4) return Fail(TEXT("missing cooperative starts"));
                for (TActorIterator<ATrainEnemy> It(W);It;++It)
                {
                    if (!Reachable(P->GetActorLocation(),It->GetActorLocation())) return Fail(TEXT("enemy unreachable"));
                    if (!It->GetIsReplicated()) return Fail(TEXT("enemy not replicated"));
                    // Review captures and traversal isolate composition, not combat balance.
                    It->SetActorTickEnabled(false);It->GetCharacterMovement()->DisableMovement();
                }
                for (int32 I=1;I<Route.Num();++I)
                    if (!Reachable(Route[I-1],Route[I])) return Fail(TEXT("route disconnected"));
                PC->SetIgnoreMoveInput(true);
                P->SetActorLocation(Route[0],false,nullptr,ETeleportType::TeleportPhysics);
                auto* Cam=W->SpawnActor<ACameraActor>();
                const FVector Focus(100,300,0),Location(-5200,-5000,8200);
                Cam->SetActorLocationAndRotation(Location,(Focus-Location).Rotation());
                Cam->GetCameraComponent()->SetProjectionMode(ECameraProjectionMode::Orthographic);
                Cam->GetCameraComponent()->SetOrthoWidth(10200);
                PC->SetViewTarget(Cam);
                Step=1;Since=Now;
            }
            else if (Step==1 && Now-Since>2)
            {
                Capture(TEXT("01-vue-ensemble.png"));Step=2;Since=Now;
            }
            else if (Step==2 && Now-Since>1)
            {
                PC->SetViewTarget(P);Step=3;Since=Now;
            }
            else if (Step==3 && Now-Since>1.5)
            {
                Capture(TEXT("02-debarquement.png"));Step=4;Since=Now;Waypoint=1;
            }
            else if (Step==4)
            {
                const FVector Direction=Route[Waypoint]-P->GetActorLocation();
                if (Now-Since>12) return Fail(TEXT("native character stuck"));
                if (Direction.Size2D()>55)
                {
                    P->AddMovementInput(Direction.GetSafeNormal2D(),1,true);
                }
                else
                {
                    P->GetCharacterMovement()->StopMovementImmediately();
                    if (Waypoint==2) Capture(TEXT("03-cour-nord.png"));
                    if (Waypoint==5) Capture(TEXT("04-contournement-sud.png"));
                    if (Waypoint==8) Capture(TEXT("05-passage.png"));
                    if (Waypoint==11) Capture(TEXT("06-sortie.png"));
                    UE_LOG(LogTemp,Display,TEXT("TRAIN_BLOCKOUT_WALK %d grounded=%d location=%s"),Waypoint,
                        P->GetCharacterMovement()->IsMovingOnGround(),*P->GetActorLocation().ToCompactString());
                    if (!P->GetCharacterMovement()->IsMovingOnGround()) return Fail(TEXT("not grounded"));
                    ++Waypoint;Since=Now;
                    if (Waypoint==Route.Num())
                    {
                        Step=6;
                    }
                }
            }
            else if (Step==6 && Now-Since>1)
            {
                for (TActorIterator<ATrainEnemy> It(W);It;++It) It->Destroy();
                GS->RemainingEnemies=0;
                W->GetAuthGameMode<ATrainGameMode>()->RequestDeparture(P);
                Step=5;
            }
            return true;
        }
    };
    FAutoConsoleCommand Review(TEXT("Train.BlockoutReview"),TEXT("Development: inspect and walk the authored Quai blockout, then verify departure."),
        FConsoleCommandDelegate::CreateLambda([]()
        {
            if (!IsRunningGame() || !FParse::Param(FCommandLine::Get(),TEXT("TrainBlockoutReview"))) return;
            auto Test=MakeShared<FBlockoutReview>();
            FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Test](float Delta){return Test->Tick(Delta);}));
        }));
}
#endif
