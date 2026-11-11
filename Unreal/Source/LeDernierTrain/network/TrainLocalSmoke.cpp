#if !UE_BUILD_SHIPPING
#include "TrainNetwork.h"
#include "TrainDash.h"
#include "TrainMelee.h"
#include "../TrainPrototype.h"
#include "../TrainSettings.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Containers/Ticker.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "UnrealClient.h"

namespace
{
    struct FLocalSmoke
    {
        TWeakObjectPtr<UWorld> World;
        double Began=FPlatformTime::Seconds(),Ready=0;
        FVector Before;
        bool Pressed=false,Released=false,Dash=false,Light=false,Heavy=false,Shot=false,Moved=false,Predicted=false,Placed=false,Reported=false;
        bool Passed = false;
        bool Tick(float Delta)
        {
            for (const auto& Context:GEngine->GetWorldContexts()) if (Context.WorldType==EWorldType::Game) World=Context.World();
            auto* W=World.Get();auto* PC=W?Cast<ATrainPlayerController>(W->GetFirstPlayerController()):nullptr;
            auto* Pawn=PC?Cast<ATrainPawn>(PC->GetPawn()):nullptr;auto* GS=W?W->GetGameState<ATrainGameState>():nullptr;
            if (FPlatformTime::Seconds()-Began>35)
            { UE_LOG(LogTemp,Error,TEXT("TRAIN_LOCAL_COOP_SMOKE timed out"));FPlatformMisc::RequestExitWithStatus(false,1);return false; }
            if (!W || !PC || !Pawn || !GS) return true;
            const bool Host=W->GetNetMode()==NM_ListenServer;
            if (!Ready && GS->PlayerArray.Num()>=2)
            {
                Ready=FPlatformTime::Seconds();Before=Pawn->GetActorLocation();
                if (Host) for (TActorIterator<ATrainPawn> It(W);It;++It)
                    if (*It!=Pawn) { Before=It->GetActorLocation();break; }
            }
            if (!Ready) return true;
            const double Time=FPlatformTime::Seconds()-Ready;
            if (!Host)
            {
                if (Time>.4 && !Pressed)
                { Pressed=true;PC->InputKey(FInputKeyEventArgs::CreateSimulated(UTrainSettings::Get()->Key(TEXT("MoveUp"),false),IE_Pressed,1)); }
                if (Time>1.2 && !Released)
                {
                    Released=true;Moved=FVector::Dist2D(Before,Pawn->GetActorLocation())>50;
                    PC->InputKey(FInputKeyEventArgs::CreateSimulated(UTrainSettings::Get()->Key(TEXT("MoveUp"),false),IE_Released,0));
                }
                if (Time>1.5 && !Dash) { Dash=Pawn->GetAbilitySystemComponent()->TryActivateAbilityByClass(UTrainDash::StaticClass()); }
                if (Time>2.3 && !Light)
                {
                    Light=Pawn->GetAbilitySystemComponent()->TryActivateAbilityByClass(UTrainLightAttack::StaticClass());
                    Predicted=Pawn->GetMesh()->GetAnimInstance()->IsAnyMontagePlaying();
                }
                if (Time>3.1 && !Heavy) Heavy=Pawn->GetAbilitySystemComponent()->TryActivateAbilityByClass(UTrainHeavyAttack::StaticClass());
            }
            else if (Time>4.2 && !Placed)
            {
                Placed=true;int32 Index=0;
                for (TActorIterator<ATrainPawn> It(W);It;++It)
                {
                    if (*It!=Pawn) Moved=FVector::Dist2D(Before,It->GetActorLocation())>50;
                    It->SetActorLocationAndRotation(FVector(1180+Index++*130,-200,95),FRotator(0,225,0),false,nullptr,ETeleportType::TeleportPhysics);
                }
            }
            if (Time>5.5 && !Shot)
            {
                Shot=true;
                const FString File=FPaths::ProjectSavedDir()/TEXT("PrototypeCaptures")/(Host?TEXT("native-coop-host.png"):TEXT("native-coop-guest.png"));
                FScreenshotRequest::RequestScreenshot(File,true,false);
            }
            if (!Reported && Time>(Host?6.2:6.5))
            {
                Reported=true;
                bool Accepted=false;for (APlayerState* State:GS->PlayerArray)
                    if (auto* PS=Cast<ATrainPlayerState>(State);PS && PS->DashReadyAt>0) Accepted=true;
                Passed = GS->PlayerArray.Num()==2 && (Host ? Accepted && Moved : Moved && Dash && Light && Heavy && Predicted);
                const FString Json=FString::Printf(TEXT("{\"role\":\"%s\",\"success\":%s,\"players\":%d,\"moved\":%s,\"dash\":%s,\"light\":%s,\"heavy\":%s,\"predicted_animation\":%s,\"server_accepted_dash\":%s}"),
                    Host?TEXT("host"):TEXT("guest"),Passed?TEXT("true"):TEXT("false"),GS->PlayerArray.Num(),Moved?TEXT("true"):TEXT("false"),Dash?TEXT("true"):TEXT("false"),Light?TEXT("true"):TEXT("false"),Heavy?TEXT("true"):TEXT("false"),Predicted?TEXT("true"):TEXT("false"),Accepted?TEXT("true"):TEXT("false"));
                const FString Dir=FPaths::ProjectSavedDir()/TEXT("LocalCoopSmoke");IFileManager::Get().MakeDirectory(*Dir,true);
                FFileHelper::SaveStringToFile(Json,*(Dir/(Host?TEXT("host.json"):TEXT("guest.json"))));
                UE_LOG(LogTemp,Display,TEXT("TRAIN_LOCAL_COOP_SMOKE %s"),*Json);
            }
            if (Reported)
            {
                if (!FParse::Param(FCommandLine::Get(),TEXT("TrainCoopSmokeExit"))) return false;
                // The host leaves last so the guest can write its complete result first.
                if (!Host || Time>8) { FPlatformMisc::RequestExitWithStatus(false, Passed ? 0 : 1);return false; }
            }
            return true;
        }
    };
    FAutoConsoleCommandWithWorld Smoke(TEXT("Train.LocalCoopSmoke"),TEXT("Development: verify two real local processes and capture the party HUD."),
        FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
        {
            if (!IsRunningGame()) return;
            auto Test=MakeShared<FLocalSmoke>();Test->World=World;
            FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Test](float Delta){return Test->Tick(Delta);}));
        }));
}
#endif
