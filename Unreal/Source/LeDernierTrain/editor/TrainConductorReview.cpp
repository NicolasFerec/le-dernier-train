#if !UE_BUILD_SHIPPING
#include "../TrainPrototype.h"
#include "../TrainSettings.h"
#include "../network/TrainNetwork.h"
#include "../network/TrainMelee.h"
#include "../network/TrainDash.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "InputKeyEventArgs.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
namespace
{
    struct FConductorReview
    {
        double Began=FPlatformTime::Seconds();
        int32 Frame=-1;
        void Loadout(ATrainPawn* P,int32 Weapon)
        {
            // Review-only fixture: normal gameplay correctly disallows changing
            // weapons after leaving the wagon. Never write the player's save.
            auto* PS=P->GetPlayerState<ATrainPlayerState>();
            P->Character=PS->Character=0;P->Weapon=PS->Weapon=Weapon;P->ApplyLoadout();
            UE_LOG(LogTemp,Display,TEXT("CONDUCTOR_REVIEW_LOADOUT character=%d weapon=%d"),P->Character,P->Weapon);
        }
        void Pose(ATrainPawn* P,float Yaw=225)
        {
            P->GetCharacterMovement()->StopMovementImmediately();
            P->SetActorLocationAndRotation(FVector(-750,650,95),FRotator(0,Yaw,0),false,nullptr,ETeleportType::TeleportPhysics);
        }
        void Move(APlayerController* PC,bool Down)
        {
            for (FName Name:{FName(TEXT("MoveUp")),FName(TEXT("MoveRight"))})
                PC->InputKey(FInputKeyEventArgs::CreateSimulated(UTrainSettings::Get()->Key(Name,false),Down?IE_Pressed:IE_Released,Down?1:0));
        }
        bool Tick(float)
        {
            if (FPlatformTime::Seconds()-Began>180) { UE_LOG(LogTemp,Error,TEXT("CONDUCTOR_REVIEW_TIMEOUT %d"),Frame);FPlatformMisc::RequestExitWithStatus(false,1);return false; }
            UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::Game)W=C.World();
            auto* PC=W?W->GetFirstPlayerController():nullptr;auto* P=PC?Cast<ATrainPawn>(PC->GetPawn()):nullptr;
            const auto* GS=W?W->GetGameState<ATrainGameState>():nullptr;
            if(!P||!GS||!GS->RoomReady||FPlatformTime::Seconds()-Began<4)return true;
            if(Frame<0)
            {
                for(TActorIterator<ATrainEnemy> It(W);It;++It)It->Destroy();
                Loadout(P,0);Pose(P);
                UE_LOG(LogTemp,Display,TEXT("CONDUCTOR_REVIEW_MODEL %s bones=%d"),*P->GetMesh()->GetSkeletalMeshAsset()->GetName(),P->GetMesh()->GetNumBones());
                FApp::SetFixedDeltaTime(1./30.);FApp::SetUseFixedTimeStep(true);Frame=0;
            }
            auto* ASC=P->GetAbilitySystemComponent();
            auto Aim=[PC,P]() { FVector2D Screen;if(PC->ProjectWorldLocationToScreen(P->GetActorLocation()+P->GetActorForwardVector()*400,Screen))PC->SetMouseLocation(Screen.X,Screen.Y); };
            if(Frame==60) { Pose(P,0);Move(PC,true); }
            if(Frame==120) { Move(PC,false);Pose(P); }
            if(Frame==150||Frame==164) { Aim();ASC->TryActivateAbilityByClass(UTrainLightAttack::StaticClass()); }
            if(Frame==210) { Aim();ASC->TryActivateAbilityByClass(UTrainHeavyAttack::StaticClass()); }
            if(Frame==270) { Loadout(P,1);Pose(P); }
            if(Frame==300) { Aim();ASC->TryActivateAbilityByClass(UTrainLightAttack::StaticClass()); }
            if(Frame==345) { Aim();ASC->TryActivateAbilityByClass(UTrainHeavyAttack::StaticClass()); }
            if(Frame==405) { Pose(P,0);ASC->TryActivateAbilityByClass(UTrainDash::StaticClass()); }
            if(Frame>=450)
            {
                FApp::SetUseFixedTimeStep(false);
                UE_LOG(LogTemp,Display,TEXT("TRAIN_CONDUCTOR_REVIEW_COMPLETE frames=%d fps=30"),Frame);
                FPlatformMisc::RequestExitWithStatus(false,0);return false;
            }
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("ConductorUE5Review/Frames")/FString::Printf(TEXT("frame-%04d.png"),Frame),false,false);
            ++Frame;return true;
        }
    };
    FAutoConsoleCommand Review(TEXT("Train.ConductorReview"),TEXT("Development: record real native animation and abilities at a fixed 30 fps."),FConsoleCommandDelegate::CreateLambda([]()
    {
        if(!IsRunningGame()||!FParse::Param(FCommandLine::Get(),TEXT("TrainConductorReview")))return;
        auto State=MakeShared<FConductorReview>();
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([State](float D){return State->Tick(D);}));
    }));
}
#endif
