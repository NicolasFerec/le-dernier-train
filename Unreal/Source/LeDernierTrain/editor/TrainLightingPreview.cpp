#if !UE_BUILD_SHIPPING
#include "../TrainPrototype.h"
#include "../network/TrainNetwork.h"
#include "Components/LocalLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Light.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
    // Same camera and frozen pose: lit / local lamps off / shadow casting off.
    // These development-only comparisons distinguish real light/shadows from bloom.
    FAutoConsoleCommand LightingPreview(TEXT("Train.LightingPreview"), TEXT("Development: compare local lighting and shadows at a fixed gameplay position."),
        FConsoleCommandDelegate::CreateLambda([]()
        {
            if (!IsRunningGame() || !FParse::Param(FCommandLine::Get(), TEXT("TrainLightingPreview"))) return;
            struct FState { int32 Step=0; double Since=FPlatformTime::Seconds(); TArray<TWeakObjectPtr<ULocalLightComponent>> Lamps; };
            auto State=MakeShared<FState>();
            const double Start=FPlatformTime::Seconds();
            FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([State,Start](float)
            {
                const double Now=FPlatformTime::Seconds();
                if (Now-Start>60) { FPlatformMisc::RequestExitWithStatus(false,1);return false; }
                UWorld* W=nullptr;for (const auto& C:GEngine->GetWorldContexts()) if (C.WorldType==EWorldType::Game) W=C.World();
                auto* PC=W?W->GetFirstPlayerController():nullptr;
                auto* Pawn=PC?Cast<ATrainPawn>(PC->GetPawn()):nullptr;
                auto* GS=W?W->GetGameState<ATrainGameState>():nullptr;
                if (!Pawn || !GS || !GS->RoomReady) return true;
                if (State->Step==0)
                {
                    float X=-1450,Y=100;
                    FParse::Value(FCommandLine::Get(),TEXT("TrainLightX="),X);
                    FParse::Value(FCommandLine::Get(),TEXT("TrainLightY="),Y);
                    PC->ConsoleCommand(TEXT("r.SetRes 1600x900w"));PC->SetIgnoreMoveInput(true);
                    Pawn->SetActorLocationAndRotation(FVector(X,Y,95),FRotator(0,225,0),false,nullptr,ETeleportType::TeleportPhysics);
                    Pawn->GetCharacterMovement()->StopMovementImmediately();
                    for (TActorIterator<ATrainEnemy> It(W);It;++It) It->Destroy();
                    for (TActorIterator<ALight> It(W);It;++It)
                        if (auto* Light=Cast<ULocalLightComponent>(It->GetLightComponent())) State->Lamps.Add(Light);
                    State->Step=1;State->Since=Now;
                }
                else if (Now-State->Since>2.)
                {
                    FString Label=TEXT("wagon");FParse::Value(FCommandLine::Get(),TEXT("TrainLightLabel="),Label);
                    const TCHAR* Suffix[]={TEXT("lit"),TEXT("lamps-off"),TEXT("shadows-off")};
                    if (State->Step<=3)
                    {
                        Pawn->GetMesh()->bPauseAnims=true;
                        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("LightingReview")/(Label+TEXT("-")+Suffix[State->Step-1]+TEXT(".png")),true,false);
                        UE_LOG(LogTemp,Display,TEXT("TRAIN_LIGHT_CAPTURE %s %s lamps=%d"),*Label,Suffix[State->Step-1],State->Lamps.Num());
                    }
                    // Apply changes on the following tick, after the screenshot has rendered.
                    ++State->Step;State->Since=Now;
                    if (State->Step==5) { FPlatformMisc::RequestExitWithStatus(false,0);return false; }
                }
                else if (Now-State->Since>.3)
                {
                    for (const auto& Weak:State->Lamps) if (auto* Lamp=Weak.Get())
                    {
                        Lamp->SetVisibility(State->Step!=2);
                        Lamp->SetCastShadows(State->Step!=3);
                    }
                }
                return true;
            }));
        }));
}
#endif
