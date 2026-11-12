#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "TrainRoomGeneration.h"
#include "../TrainPrototype.h"
#include "../TrainRooms.h"
#include "../network/TrainSession.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainEncounterBudget,"LeDernierTrain.Generation.EncounterBudgets",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTrainEncounterBudget::RunTest(const FString&)
{
    for (int32 Crew=1;Crew<=4;++Crew) for (int32 Seed=1;Seed<=128;++Seed)
    {
        int32 Previous=0;
        for (int32 Room=1;Room<=10;++Room)
        {
            const auto Plan=TrainEncounters::Compose(Room,Crew,Seed);
            TestTrue(TEXT("Same seed reproduces encounter composition"),Plan==TrainEncounters::Compose(Room,Crew,Seed));
            int32 Threat=0,Bosses=0;
            for (auto Type:Plan)
            {
                Threat+=TrainEncounters::Cost(Type);
                Bosses+=Type==ETrainEnemyType::Lockkeeper||Type==ETrainEnemyType::Guardian;
                if (Room<3) TestTrue(TEXT("Armoured sentries are introduced after the opening"),Type!=ETrainEnemyType::Sentry);
                if (Room<4) TestTrue(TEXT("Ranged threats are introduced after melee roles"),Type!=ETrainEnemyType::Gunner);
            }
            TestEqual(TEXT("Random composition spends the exact threat budget"),Threat,TrainEncounters::Budget(Room,Crew));
            TestTrue(TEXT("Threat strictly increases after every room, including the first boss"),Threat>Previous);
            TestEqual(TEXT("Exactly one boss in rooms five and ten"),Bosses,Room==5||Room==10?1:0);
            Previous=Threat;
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainRoomSeeds,"LeDernierTrain.Generation.RoomSeeds",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
namespace
{
    class FCheckRoomSeeds : public IAutomationLatentCommand
    {
        FAutomationTestBase* Test;
        int32 Step=-1;
        double Since=FPlatformTime::Seconds();
        TMap<int32,FString> Layouts,Spawns;
        TMap<ETrainEnemyType,float> Health;
        struct FCase { int32 Room,Seed; };
        const TArray<FCase> Cases={{1,101},{1,202},{1,101},{4,301},{4,302},{6,401},{6,402},{10,501},{10,502},
            {2,601},{2,602},{3,701},{3,702},{5,801},{5,802},{7,901},{7,902},{8,1001},{8,1002},{9,1101},{9,1102}};
        int32 ExpectedSeed() const { return HashCombineFast(Cases[Step].Seed,Cases[Step].Room)&MAX_int32; }
    public:
        explicit FCheckRoomSeeds(FAutomationTestBase* InTest):Test(InTest) {}
        bool Update() override
        {
            UWorld* World=nullptr;for (const auto& C:GEngine->GetWorldContexts()) if (C.WorldType==EWorldType::PIE) World=C.World();
            auto* GS=World?World->GetGameState<ATrainGameState>():nullptr;
            auto* PC=World?World->GetFirstPlayerController():nullptr;
            auto* Pawn=PC?Cast<ATrainPawn>(PC->GetPawn()):nullptr;
            if (FPlatformTime::Seconds()-Since>30) { Test->AddError(FString::Printf(TEXT("Procedural case %d timed out"),Step));return true; }
            if (!GS||!Pawn||!GS->RoomReady) return false;
            if (Step>=0)
            {
                if (GS->RoomSeed!=ExpectedSeed()) return false;
                Test->TestTrue(TEXT("PCG produced furniture"),GS->LayoutProps>=3);
                Test->TestTrue(TEXT("Exit remains connected to the entry"),
                    [] (UWorld* W,FVector A,FVector B) { auto* P=UNavigationSystemV1::FindPathToLocationSynchronously(W,A,B);return P&&P->IsValid()&&!P->IsPartial(); }(World,Pawn->GetActorLocation(),GS->DeparturePoint-FVector(180,0,0)));
                TArray<FString> Props;
                for (TActorIterator<ATrainRoomProp> It(World);It;++It)
                {
                    Props.Add(FString::Printf(TEXT("%d:%s:%0.f"),It->Kind,*It->GetActorLocation().ToCompactString(),It->GetActorRotation().Yaw));
                    Test->TestTrue(TEXT("PCG furniture uses native replicated actors"),It->GetIsReplicated());
                    Test->TestTrue(TEXT("Every PCG module is grounded, with no default point buried at the origin"),
                        FMath::Abs(It->GetActorLocation().Z-ATrainRoomProp::HalfSize(It->Kind).Z)<1.);
                }
                Props.Sort();const FString Layout=FString::Join(Props,TEXT("|"));
                FString Spawn;
                for (TActorIterator<ATrainRoomGenerator> It(World);It;++It)
                    for (const FVector& P:It->GetSpawnLocations()) Spawn+=P.ToCompactString();
                if (Step==2)
                {
                    Test->TestEqual(TEXT("Repeating a seed reproduces the layout"),Layout,Layouts[101]);
                    Test->TestEqual(TEXT("Repeating a seed reproduces spawn positions"),Spawn,Spawns[101]);
                }
                if (Step>0 && Cases[Step-1].Room==Cases[Step].Room)
                {
                    Test->TestTrue(TEXT("Another seed changes furniture positions"),Layout!=Layouts[Cases[Step-1].Seed]);
                    Test->TestTrue(TEXT("Another seed changes enemy positions"),Spawn!=Spawns[Cases[Step-1].Seed]);
                }
                Layouts.Add(Cases[Step].Seed,Layout);Spawns.Add(Cases[Step].Seed,Spawn);
                int32 Count=0,Threat=0;
                for (TActorIterator<ATrainEnemy> It(World);It;++It)
                {
                    ++Count;Threat+=TrainEncounters::Cost(It->GetEnemyType());
                    if (Health.Contains(It->GetEnemyType())) Test->TestEqual(TEXT("An enemy type keeps its health across rooms and seeds"),It->MaxHealth,Health[It->GetEnemyType()]);
                    else Health.Add(It->GetEnemyType(),It->MaxHealth);
                    auto* P=UNavigationSystemV1::FindPathToLocationSynchronously(World,Pawn->GetActorLocation(),It->GetActorLocation());
                    Test->TestTrue(TEXT("Every random spawn is reachable"),P&&P->IsValid()&&!P->IsPartial());
                    Test->TestTrue(TEXT("No enemy spawns on the entrance"),FVector::Dist2D(Pawn->GetActorLocation(),It->GetActorLocation())>1150);
                }
                Test->TestEqual(TEXT("No planned enemy was dropped"),Count,GS->RemainingEnemies);
                Test->TestEqual(TEXT("Spawned enemies match the difficulty budget"),Threat,GS->EncounterThreat);
            }
            if (++Step>=Cases.Num()) return true;
            World->GetGameInstance<UTrainSession>()->ExpeditionSeed=Cases[Step].Seed;
            World->ServerTravel(FString(TEXT("/Game/Maps/"))+TrainRooms::Faubourgs[Cases[Step].Room-1].Map);
            Since=FPlatformTime::Seconds();return false;
        }
    };
}
bool FTrainRoomSeeds::RunTest(const FString&)
{
    FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Maps/Wagon"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCheckRoomSeeds>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
