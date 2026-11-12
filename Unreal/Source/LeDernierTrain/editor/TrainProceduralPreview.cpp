#if !UE_BUILD_SHIPPING
#include "../TrainPrototype.h"
#include "../network/TrainNetwork.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
    FAutoConsoleCommand Preview(TEXT("Train.PCGPreview"),TEXT("Development: capture a generated room from a fixed overview camera."),FConsoleCommandDelegate::CreateLambda([]()
    {
        if (!IsRunningGame()||!FParse::Param(FCommandLine::Get(),TEXT("TrainPCGPreview"))) return;
        const double Start=FPlatformTime::Seconds();
        auto Stage=MakeShared<int32>(0);auto Since=MakeShared<double>(Start);
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Stage,Since,Start](float)
        {
            UWorld* World=nullptr;for (const auto& C:GEngine->GetWorldContexts()) if (C.WorldType==EWorldType::Game) World=C.World();
            auto* GS=World?World->GetGameState<ATrainGameState>():nullptr;
            auto* PC=World?World->GetFirstPlayerController():nullptr;
            const double Now=FPlatformTime::Seconds();
            if (Now-Start>25) { FPlatformMisc::RequestExitWithStatus(false,1);return false; }
            if (!GS||!PC||!GS->RoomReady||!GS->RoomIndex) return true;
            if (*Stage==0)
            {
                PC->ConsoleCommand(TEXT("r.SetRes 1600x900w"));PC->SetIgnoreMoveInput(true);
                for (TActorIterator<ATrainEnemy> It(World);It;++It)
                {
                    It->SetActorTickEnabled(false);It->GetCharacterMovement()->StopMovementImmediately();
                    if (auto* AI=Cast<AAIController>(It->GetController())) AI->StopMovement();
                }
                const FRotator Facing(-48,45,0);
                auto* Camera=World->SpawnActor<ACameraActor>(FVector(0,0,100)-Facing.Vector()*5000,Facing);
                Camera->GetCameraComponent()->SetProjectionMode(ECameraProjectionMode::Orthographic);
                Camera->GetCameraComponent()->SetOrthoWidth(5500);PC->SetViewTarget(Camera);
                *Stage=1;*Since=Now;
            }
            else if (*Stage==1 && Now-*Since>2)
            {
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("ProceduralCaptures")/
                    FString::Printf(TEXT("room-%02d-seed-%d.png"),GS->RoomIndex,GS->RoomSeed),true,false);
                UE_LOG(LogTemp,Display,TEXT("TRAIN_PCG_PREVIEW room=%d seed=%d props=%d"),GS->RoomIndex,GS->RoomSeed,GS->LayoutProps);
                *Stage=2;*Since=Now;
            }
            else if (*Stage==2&&Now-*Since>1) { FPlatformMisc::RequestExitWithStatus(false,0);return false; }
            return true;
        }));
    }));
}
#endif
