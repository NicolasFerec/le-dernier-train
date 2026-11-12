#if !UE_BUILD_SHIPPING
#include "../TrainPrototype.h"
#include "../network/TrainNetwork.h"
#include "../network/TrainMelee.h"
#include "../network/TrainDash.h"
#include "AbilitySystemComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
    struct FScoutReview
    {
        double Began=FPlatformTime::Seconds();
        int32 Frame=-1;
        FString Label=TEXT("after");
        bool IdleOnly=FParse::Param(FCommandLine::Get(),TEXT("TrainScoutIdleOnly"));
        TWeakObjectPtr<ACameraActor> Camera;
        void Pose(ATrainPawn* P,float Yaw)
        {
            P->GetCharacterMovement()->StopMovementImmediately();
            P->SetActorLocationAndRotation(FVector(-1450,100,95),FRotator(0,Yaw,0),false,nullptr,ETeleportType::TeleportPhysics);
        }
        bool Tick(float)
        {
            if(FPlatformTime::Seconds()-Began>150)
            {
                UE_LOG(LogTemp,Error,TEXT("TRAIN_SCOUT_REVIEW_TIMEOUT frame=%d"),Frame);
                FPlatformMisc::RequestExitWithStatus(false,1);return false;
            }
            UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::Game)W=C.World();
            auto* PC=W?W->GetFirstPlayerController():nullptr;
            auto* P=PC?Cast<ATrainPawn>(PC->GetPawn()):nullptr;
            auto* PS=P?P->GetPlayerState<ATrainPlayerState>():nullptr;
            const auto* GS=W?W->GetGameState<ATrainGameState>():nullptr;
            if(!P||!PS||!GS||!GS->RoomReady||FPlatformTime::Seconds()-Began<4)return true;
            if(Frame<0)
            {
                FParse::Value(FCommandLine::Get(),TEXT("TrainScoutReviewLabel="),Label);
                // Isolated review fixture: no unlock purchase or profile/save write.
                PS->Character=P->Character=1;PS->Weapon=P->Weapon=0;P->ApplyLoadout();Pose(P,45);
                Camera=W->SpawnActor<ACameraActor>();
                Camera->GetCameraComponent()->SetProjectionMode(ECameraProjectionMode::Orthographic);
                Camera->GetCameraComponent()->SetOrthoWidth(700);
                PC->SetViewTarget(Camera.Get());PC->SetIgnoreMoveInput(true);
                FApp::SetFixedDeltaTime(1./30.);FApp::SetUseFixedTimeStep(true);Frame=0;
                UE_LOG(LogTemp,Display,TEXT("TRAIN_SCOUT_REVIEW_MODEL %s bones=%d"),*P->GetMesh()->GetSkeletalMeshAsset()->GetName(),P->GetMesh()->GetNumBones());
            }
            if(Frame==120)Pose(P,225);
            if(Frame==240)Pose(P,135);
            if(Frame>=270&&Frame<300)P->AddMovementInput(FVector(1,0,0),1,true);
            if(Frame==300)Pose(P,45);
            auto* ASC=P->GetAbilitySystemComponent();
            auto Aim=[PC,P]() { FVector2D S;if(PC->ProjectWorldLocationToScreen(P->GetActorLocation()+P->GetActorForwardVector()*350,S))PC->SetMouseLocation(S.X,S.Y); };
            if(Frame==360){Aim();ASC->TryActivateAbilityByClass(UTrainLightAttack::StaticClass());}
            if(Frame==390){Aim();ASC->TryActivateAbilityByClass(UTrainHeavyAttack::StaticClass());}
            if(Frame==420){Pose(P,225);ASC->TryActivateAbilityByClass(UTrainDash::StaticClass());}
            if(Frame==450)Pose(P,45);
            if(Frame>=(IdleOnly?240:510))
            {
                FApp::SetUseFixedTimeStep(false);
                UE_LOG(LogTemp,Display,TEXT("TRAIN_SCOUT_REVIEW_COMPLETE label=%s frames=%d fps=30"),*Label,Frame);
                FPlatformMisc::RequestExitWithStatus(false,0);return false;
            }
            const FRotator View(-48,45,0);
            Camera->SetActorLocationAndRotation(P->GetActorLocation()+FVector(0,0,-30)-View.Vector()*1800,View);
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("ScoutIdleReview")/Label/FString::Printf(TEXT("frame-%04d.png"),Frame),false,false);
            ++Frame;return true;
        }
    };
    FAutoConsoleCommand ScoutReview(TEXT("Train.ScoutReview"),TEXT("Development: compare Scout idle from front/back, run/stop, attack recovery."),FConsoleCommandDelegate::CreateLambda([]()
    {
        if(!IsRunningGame()||!FParse::Param(FCommandLine::Get(),TEXT("TrainScoutReview")))return;
        auto State=MakeShared<FScoutReview>();
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([State](float D){return State->Tick(D);}));
    }));
}
#endif
