#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "TrainPrototype.h"
#include "network/TrainNetwork.h"
#include "progression/TrainProfile.h"
#include "ui/TrainDamageNumber.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "Engine/Engine.h"
#include "Animation/SkeletalMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainGameplayParity,"LeDernierTrain.Gameplay.WagonAndRun",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
namespace
{
    TWeakObjectPtr<UTrainProfile> OriginalProfile;
    int32 OriginalPoints=0,OriginalCharacter=0,OriginalWeapon=0;
    TArray<FName> OriginalUnlocks;
    class FCheckGameplay : public IAutomationLatentCommand
    {
        FAutomationTestBase* Test;
        int32 Phase=0;
        double Since=FPlatformTime::Seconds();
        TWeakObjectPtr<ATrainEnemy> MovingEnemy;
        FVector EnemyStart;
        int32 FirstKills=0;
        void Next() { ++Phase;Since=FPlatformTime::Seconds(); }
    public:
        explicit FCheckGameplay(FAutomationTestBase* InTest):Test(InTest) {}
        virtual bool Update() override
        {
            const double Time=FPlatformTime::Seconds()-Since;
            if (Time>35) { Test->AddError(FString::Printf(TEXT("Gameplay phase %d timed out"),Phase));return true; }
            UWorld* World=nullptr;
            for (const auto& Context:GEngine->GetWorldContexts()) if (Context.WorldType==EWorldType::PIE) World=Context.World();
            auto* PC=World?World->GetFirstPlayerController():nullptr;
            auto* Pawn=PC?Cast<ATrainPawn>(PC->GetPawn()):nullptr;
            auto* GS=World?World->GetGameState<ATrainGameState>():nullptr;
            auto* PS=Pawn?Pawn->GetPlayerState<ATrainPlayerState>():nullptr;
            if (!Pawn || !PS || !GS) return false;
            auto* Profile=World->GetGameInstance()->GetSubsystem<UTrainProfile>();
            const FString Map=UGameplayStatics::GetCurrentLevelName(World);
            auto Teleport=[Pawn](FVector Point,FRotator Rot=FRotator::ZeroRotator)
            { Pawn->SetActorLocationAndRotation(Point,Rot,false,nullptr,ETeleportType::TeleportPhysics);Pawn->GetCharacterMovement()->StopMovementImmediately(); };
            if (Phase==0)
            {
                int32 Stations=0,Dummies=0;TSet<int32> Kinds;
                for (TActorIterator<ATrainStation> It(World);It;++It) { ++Stations;Kinds.Add(It->Kind); }
                for (TActorIterator<ATrainDummy> It(World);It;++It) ++Dummies;
                Test->TestEqual(TEXT("Five distinct workshop stations"),Stations,5);
                Test->TestEqual(TEXT("Every workshop purpose is represented"),Kinds.Num(),5);
                Test->TestEqual(TEXT("Three safe practice targets"),Dummies,3);
                Test->TestNotNull(TEXT("Layer conductor is a skeletal mesh"),Pawn->GetMesh()->GetSkeletalMeshAsset());
                Test->TestTrue(TEXT("Dedicated ground root is present"),Pawn->GetMesh()->GetBoneIndex(TEXT("root"))!=INDEX_NONE);
                Test->TestTrue(TEXT("Hand bone is present"),Pawn->GetMesh()->GetBoneIndex(TEXT("RightHand"))!=INDEX_NONE);
                const double BodyHeight=FVector::Dist(Pawn->GetMesh()->GetBoneLocation(TEXT("Head")),Pawn->GetMesh()->GetBoneLocation(TEXT("RightFoot")));
                Test->TestTrue(TEXT("Animation preserves a human body scale"),BodyHeight>100 && BodyHeight<220);
                OriginalProfile=Profile;OriginalPoints=Profile->Data->Points;OriginalUnlocks=Profile->Data->Unlocks;
                OriginalCharacter=Profile->Data->Character;OriginalWeapon=Profile->Data->Weapon;
                Profile->Data->Points=10;Profile->Data->Unlocks.Empty();
                for (FName Item:{FName(TEXT("hammer")),FName(TEXT("health")),FName(TEXT("speed")),FName(TEXT("character")),FName(TEXT("daggers"))})
                    Test->TestTrue(TEXT("Persistent unlock can be bought"),Profile->Buy(Item));
                Test->TestFalse(TEXT("An owned passive cannot be charged twice"),Profile->Buy(TEXT("health")));
                Test->TestTrue(TEXT("Sabre can be equipped"),Profile->Select(0,0));Pawn->SendLoadout();
                Teleport(FVector(1200,-230,95));Pawn->ServerAttack(false);Next();return false;
            }
            if (Phase==1)
            {
                if (Time<.55) return false;
                TArray<UUserWidget*> Popups;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World,Popups,UTrainDamageNumber::StaticClass(),false);
                bool Sabre=false;for (auto* Popup:Popups) if (auto* Number=Cast<UTrainDamageNumber>(Popup)) Sabre|=Number->Damage==30;
                Test->TestTrue(TEXT("Light sabre hit shows its damage on the dummy"),Sabre);
                Test->TestTrue(TEXT("Hammer can be equipped"),Profile->Select(0,1));Pawn->SendLoadout();
                Teleport(FVector(1200,-230,95));Pawn->ServerAttack(false);Next();return false;
            }
            if (Phase==2)
            {
                if (Time<.35) return false;
                Test->TestEqual(TEXT("Constitution increases max health"),PS->MaxHealth,120.f);
                Test->TestTrue(TEXT("Mobility increases native movement speed"),Pawn->GetCharacterMovement()->MaxWalkSpeed>390);
                auto* Weapon=FindObject<UStaticMeshComponent>(Pawn,TEXT("HeldWeapon"));
                Test->TestNotNull(TEXT("Weapon component exists"),Weapon);
                if (Weapon)
                {
                    Test->TestEqual(TEXT("Weapon follows the actual hand"),Weapon->GetAttachSocketName(),FName(TEXT("RightHand")));
                    Test->TestTrue(TEXT("Grip is within the palm"),FVector::Dist(Weapon->GetComponentLocation(),Pawn->GetMesh()->GetSocketLocation(TEXT("RightHand")))<12);
                    Test->TestTrue(TEXT("Hammer uses the Layer hammer mesh"),Weapon->GetStaticMesh()&&Weapon->GetStaticMesh()->GetName()==TEXT("hammer"));
                }
                TArray<UUserWidget*> Popups;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World,Popups,UTrainDamageNumber::StaticClass(),false);
                bool Hammer=false;for (auto* Popup:Popups) if (auto* Number=Cast<UTrainDamageNumber>(Popup)) Hammer|=Number->Damage==45;
                Test->TestTrue(TEXT("Light hammer hit shows its damage on the dummy"),Hammer);
                Test->TestEqual(TEXT("Practice does not hurt the player"),PS->Health,120.f);
                Test->TestEqual(TEXT("Practice does not farm combat statistics"),PS->DamageDealt,0.f);
                Next();return false;
            }
            if (Phase==3)
            {
                if (Time<.7) return false;
                Pawn->ServerAttack(true);Next();return false;
            }
            if (Phase==4)
            {
                if (Time<.4) return false;
                TArray<UUserWidget*> Popups;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World,Popups,UTrainDamageNumber::StaticClass(),false);
                bool Heavy=false;for (auto* Popup:Popups) if (auto* Number=Cast<UTrainDamageNumber>(Popup)) Heavy|=Number->Damage==90;
                Test->TestTrue(TEXT("Heavy hammer hit has its own damage and feedback"),Heavy);
                Profile->Select(1,1);Pawn->SendLoadout();
                Test->TestEqual(TEXT("Scout loadout is shared by the player state"),PS->Character,1);
                Test->TestEqual(TEXT("Scout has different health"),PS->MaxHealth,100.f);
                Profile->Select(0,0);Pawn->SendLoadout();
                Teleport(GS->DeparturePoint+FVector(-150,0,95));World->GetAuthGameMode<ATrainGameMode>()->RequestDeparture(Pawn);
                Next();return false;
            }
            if (Phase==5)
            {
                if (Map!=TEXT("Pompage")) return false;
                FNavLocation Location;auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
                Test->TestTrue(TEXT("Room has a baked native navigation mesh"),Nav&&Nav->ProjectPointToNavigation(FVector(-1000,-500,0),Location,FVector(120,120,250)));
                TActorIterator<ATrainEnemy> First(World);if (First) MovingEnemy=*First;
                if (!MovingEnemy.IsValid()) return false;
                EnemyStart=MovingEnemy->GetActorLocation();Teleport(EnemyStart+FVector(-350,0,0));
                Next();return false;
            }
            if (Phase==6)
            {
                if (Time<1.2) return false;
                Test->TestTrue(TEXT("Automaton follows the player using native path following"),MovingEnemy.IsValid()&&FVector::Dist2D(EnemyStart,MovingEnemy->GetActorLocation())>45);
                PS->Health=76;
                TArray<ATrainEnemy*> Enemies;for (TActorIterator<ATrainEnemy> It(World);It;++It) Enemies.Add(*It);
                const FVector FirstBody=Enemies.IsEmpty()?FVector::ZeroVector:Enemies[0]->GetMesh()->GetComponentLocation();
                FirstKills=Enemies.Num();for (auto* Enemy:Enemies) Enemy->Hit(Pawn,10000);
                bool FallenAtImpact=false;
                for (TActorIterator<ASkeletalMeshActor> Body(World);Body;++Body)
                    FallenAtImpact|=FVector::Dist(Body->GetActorLocation(),FirstBody)<2;
                Test->TestTrue(TEXT("The death animation starts at the enemy's location"),FallenAtImpact);
                Test->TestEqual(TEXT("Clearing the first room opens its exit"),GS->RemainingEnemies,0);
                Test->TestEqual(TEXT("First cleared room is counted"),GS->RoomsCleared,1);
                Teleport(GS->DeparturePoint+FVector(-150,0,95));World->GetAuthGameMode<ATrainGameMode>()->RequestDeparture(Pawn);
                Next();return false;
            }
            if (Phase==7)
            {
                if (Map!=TEXT("Fonderie") || GS->RemainingEnemies==0) return false;
                Test->TestEqual(TEXT("Room number advances"),GS->RoomIndex,2);
                Test->TestEqual(TEXT("Health survives travel instead of healing every room"),PS->Health,76.f);
                Test->TestEqual(TEXT("Kill statistics survive travel"),PS->Kills,FirstKills);
                TArray<ATrainEnemy*> Enemies;for (TActorIterator<ATrainEnemy> It(World);It;++It) Enemies.Add(*It);
                for (auto* Enemy:Enemies) Enemy->Hit(Pawn,10000);
                Teleport(GS->DeparturePoint+FVector(-150,0,95));World->GetAuthGameMode<ATrainGameMode>()->RequestDeparture(Pawn);
                Next();return false;
            }
            if (Phase==8)
            {
                if (Time<.3) return false;
                Test->TestTrue(TEXT("Expedition ends with the victory summary"),GS->RunFinished&&GS->Victory);
                Test->TestEqual(TEXT("Both rooms count in the summary"),GS->RoomsCleared,2);
                Test->TestEqual(TEXT("Permanent reward is granted once"),Profile->Data->Points,4);
                Test->TestTrue(TEXT("All kills are credited"),PS->Kills>=12);
                return true;
            }
            return false;
        }
    };
}
DEFINE_LATENT_AUTOMATION_COMMAND(FRestoreTrainProfile);
bool FRestoreTrainProfile::Update()
{
    if (OriginalProfile.IsValid())
    {
        auto* Data=OriginalProfile->Data.Get();Data->Points=OriginalPoints;Data->Unlocks=OriginalUnlocks;
        Data->Character=OriginalCharacter;Data->Weapon=OriginalWeapon;OriginalProfile->Save();
    }
    return true;
}
bool FTrainGameplayParity::RunTest(const FString& Parameters)
{
    FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Maps/Wagon"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCheckGameplay>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FRestoreTrainProfile());ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
