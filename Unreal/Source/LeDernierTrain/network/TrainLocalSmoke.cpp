#if !UE_BUILD_SHIPPING
#include "TrainNetwork.h"
#include "TrainDash.h"
#include "TrainMelee.h"
#include "../TrainPrototype.h"
#include "../TrainSettings.h"
#include "../world/TrainRoomGeneration.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
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
        bool Passed = false, PredictedDash = false;
        bool Traveling=false,RoomReported=false;
        double RoomReadyAt=0;
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
                if (!Host) Pawn->ServerLoadout(1,1,false,false);
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
                if (Time>1.5 && !Dash)
                {
                    auto* Abilities = Pawn->GetAbilitySystemComponent();
                    Dash = Abilities->TryActivateAbilityByClass(UTrainDash::StaticClass());
                    const auto* Montage = Abilities->GetCurrentMontage();
                    const auto* Source = Montage ? Montage->GetFirstAnimReference() : nullptr;
                    PredictedDash = Source && Source->GetFName() == TEXT("Dodge");
                }
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
                bool ScoutReady=false;
                for (TActorIterator<ATrainPawn> It(W);It;++It)
                    ScoutReady|=It->Character==1 && It->GetMesh()->GetBoneIndex(TEXT("index_01_r"))!=INDEX_NONE;
                UE_LOG(LogTemp,Display,TEXT("TRAIN_COOP_SCOUT_UE5 %s"),ScoutReady?TEXT("OK"):TEXT("FAILED"));
                Passed = ScoutReady && GS->PlayerArray.Num()==2 && (Host ? Accepted && Moved : Moved && Dash && PredictedDash && Light && Heavy && Predicted);
                const FString Json=FString::Printf(TEXT("{\"role\":\"%s\",\"success\":%s,\"players\":%d,\"moved\":%s,\"dash\":%s,\"light\":%s,\"heavy\":%s,\"predicted_animation\":%s,\"server_accepted_dash\":%s}"),
                    Host?TEXT("host"):TEXT("guest"),Passed?TEXT("true"):TEXT("false"),GS->PlayerArray.Num(),Moved?TEXT("true"):TEXT("false"),Dash?TEXT("true"):TEXT("false"),Light?TEXT("true"):TEXT("false"),Heavy?TEXT("true"):TEXT("false"),Predicted?TEXT("true"):TEXT("false"),Accepted?TEXT("true"):TEXT("false"));
                const FString Dir=FPaths::ProjectSavedDir()/TEXT("LocalCoopSmoke");IFileManager::Get().MakeDirectory(*Dir,true);
                FFileHelper::SaveStringToFile(Json,*(Dir/(Host?TEXT("host.json"):TEXT("guest.json"))));
                UE_LOG(LogTemp,Display,TEXT("TRAIN_LOCAL_COOP_SMOKE %s"),*Json);
                if (!Host)
                    UE_LOG(LogTemp, Display, TEXT("Dash montage predicted immediately: %s"), PredictedDash ? TEXT("yes") : TEXT("no"));
            }
            if (Reported)
            {
                if (!Passed) { FPlatformMisc::RequestExitWithStatus(false,1);return false; }
                if (Host && Time>7 && !Traveling) { Traveling=true;W->ServerTravel(TEXT("/Game/Maps/Pompage")); }
                int32 Enemies=0;for (TActorIterator<ATrainEnemy> It(W);It;++It) ++Enemies;
                TArray<FString> Modules,Transforms;
                for (TActorIterator<ATrainRoomProp> It(W);It;++It)
                {
                    const FVector P=It->GetActorLocation();
                    Transforms.Add(FString::Printf(TEXT("[%d,%.3f,%.3f,%.3f,%.3f]"),It->Kind,P.X,P.Y,P.Z,It->GetActorRotation().Yaw));
                    Modules.Add(FString::Printf(TEXT("%d:%d:%d:%d"),It->Kind,FMath::RoundToInt(P.X/10),FMath::RoundToInt(P.Y/10),(FMath::RoundToInt(It->GetActorRotation().Yaw)+360)%360));
                }
                if (GS->RoomIndex==1 && GS->RoomReady && Modules.Num()==GS->LayoutProps && Enemies==GS->RemainingEnemies && !RoomReadyAt)
                    RoomReadyAt=FPlatformTime::Seconds();
                if (RoomReadyAt && !RoomReported && FPlatformTime::Seconds()-RoomReadyAt>1)
                {
                    RoomReported=true;Passed=Passed&&Modules.Num()>=3&&Enemies>0;Modules.Sort();
                    const FString Json=FString::Printf(TEXT("{\"success\":%s,\"seed\":%d,\"props\":%d,\"enemies\":%d,\"layout\":%u,\"transforms\":[%s]}"),
                        Passed?TEXT("true"):TEXT("false"),GS->RoomSeed,Modules.Num(),Enemies,FCrc::StrCrc32(*FString::Join(Modules,TEXT("|"))),*FString::Join(Transforms,TEXT(",")));
                    const FString Role=Host?TEXT("host"):TEXT("guest");
                    FFileHelper::SaveStringToFile(Json,*(FPaths::ProjectSavedDir()/TEXT("LocalCoopSmoke")/(TEXT("room-")+Role+TEXT(".json"))));
                    UE_LOG(LogTemp,Display,TEXT("TRAIN_PCG_COOP %s %s"),*Role,*Json);
                    FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("PrototypeCaptures")/(TEXT("pcg-coop-")+Role+TEXT(".png")),true,false);
                }
                // Leave the host alive long enough for the guest's final replication check.
                if (RoomReported && FPlatformTime::Seconds()-RoomReadyAt>(Host?5:2))
                {
                    if (FParse::Param(FCommandLine::Get(),TEXT("TrainCoopSmokeExit"))) FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
                    return false;
                }
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
