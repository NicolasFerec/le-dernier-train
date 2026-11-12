#include "TrainRoomGeneration.h"
#include "../TrainPrototype.h"
#include "../network/TrainSession.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "NavAreas/NavArea_Null.h"
#include "Net/UnrealNetwork.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "Elements/PCGCreatePoints.h"
#include "Elements/PCGSpawnActor.h"

namespace
{
    const TCHAR* PropMeshes[] = {TEXT("/Game/Props/Cargo.Cargo"), TEXT("/Game/Props/Faubourgs/Pump.Pump"),
        TEXT("/Game/Props/Faubourgs/MarketStall.MarketStall"), TEXT("/Game/Props/Faubourgs/Manifold.Manifold"),
        TEXT("/Game/Props/Boiler.Boiler"), TEXT("/Game/Props/Faubourgs/Switchboard.Switchboard"),
        TEXT("/Game/Props/Faubourgs/Kiosk.Kiosk")};
    constexpr float Heights[] = {125,210,250,180,240,220,310};
    constexpr int32 Threat[] = {8,11,14,17,21,25,29,33,37,43};
    // Patrol, runner, sentry, gunner. Boss costs are included in the room budget.
    constexpr int32 Rosters[10][4] = {{3,2,0,0},{4,3,0,0},{3,4,1,0},{4,2,1,1},{3,2,0,1},
        {5,4,2,1},{5,4,3,1},{4,3,4,2},{4,4,4,3},{4,5,2,2}};
    int32 Theme(int32 Room, FRandomStream& Random)
    {
        if (Random.FRand()<.28f) return 0;
        switch (Room)
        {
            case 1: case 3: return Random.FRand()<.75f?0:6;
            case 2: case 5: return 1;
            case 4: case 7: return Random.FRand()<.5f?1:3;
            case 6: return 2;
            case 8: return 4;
            case 9: return 5;
            default: return Random.FRand()<.6f?1:3;
        }
    }
}

int32 TrainEncounters::Cost(ETrainEnemyType Type)
{
    constexpr int32 Costs[]={2,1,4,3,10,16};
    return Costs[static_cast<uint8>(Type)];
}
int32 TrainEncounters::Budget(int32 Room,int32 Crew)
{
    return Threat[FMath::Clamp(Room,1,10)-1]+FMath::Clamp(Crew-1,0,3)*(3+Room/3);
}
TArray<ETrainEnemyType> TrainEncounters::Compose(int32 Room,int32 Crew,int32 Seed)
{
    Room=FMath::Clamp(Room,1,10);
    FRandomStream Random(Seed);
    int32 Counts[4];for (int32 I=0;I<4;++I) Counts[I]=Rosters[Room-1][I];
    // Equivalent-cost substitutions vary the mix without rolling an easier next room.
    if (Random.FRand()<.5f && Counts[0]>2) { --Counts[0];Counts[1]+=2; }
    if (Room>=3 && Random.FRand()<.5f && Counts[0]>=3) { Counts[0]-=2;++Counts[2]; }
    if (Room>=4 && Random.FRand()<.5f && Counts[1]>=3) { Counts[1]-=3;++Counts[3]; }
    int32 Extra=Budget(Room,Crew)-Threat[Room-1];
    while (Extra>0)
    {
        const int32 Kind=Room>=4&&Extra>=3&&Random.FRand()<.3f?3:Extra>=2?0:1;
        ++Counts[Kind];Extra-=Cost(static_cast<ETrainEnemyType>(Kind));
    }
    TArray<ETrainEnemyType> Result;
    for (int32 Kind=0;Kind<4;++Kind) for (int32 I=0;I<Counts[Kind];++I) Result.Add(static_cast<ETrainEnemyType>(Kind));
    if (Room==5) Result.Add(ETrainEnemyType::Lockkeeper);
    if (Room==10) Result.Add(ETrainEnemyType::Guardian);
    return Result;
}

ATrainRoomProp::ATrainRoomProp()
{
    bReplicates=true;bAlwaysRelevant=true;SetReplicateMovement(true);
    Body=CreateDefaultSubobject<UBoxComponent>(TEXT("ModuleCollision"));SetRootComponent(Body);
    Body->SetMobility(EComponentMobility::Movable);Body->SetCollisionProfileName(TEXT("BlockAll"));
    Body->bDynamicObstacle=true;Body->SetAreaClassOverride(UNavArea_Null::StaticClass());
    Visual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ModuleMesh"));Visual->SetupAttachment(Body);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);Visual->SetCanEverAffectNavigation(false);
    for (const TCHAR* Path:PropMeshes) Models.Add(LoadObject<UStaticMesh>(nullptr,Path));
}
void ATrainRoomProp::BeginPlay() { Super::BeginPlay();ApplyModule(); }
void ATrainRoomProp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{ Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(ATrainRoomProp,Kind); }
FVector ATrainRoomProp::HalfSize(int32 Kind)
{
    Kind=FMath::Clamp(Kind,0,6);
    const auto* Defaults=GetDefault<ATrainRoomProp>();
    if (!Defaults->Models[Kind]) return FVector(100,100,100);
    const auto B=Defaults->Models[Kind]->GetBounds();
    return B.BoxExtent*(Heights[Kind]/FMath::Max(1.,B.BoxExtent.Z*2));
}
void ATrainRoomProp::ApplyModule()
{
    if (HasAuthority() && SpawnKindCode>0) { Kind=SpawnKindCode-1;SpawnKindCode=0; }
    Kind=FMath::Clamp(Kind,0,6);
    auto* Model=Models[Kind].Get();if (!Model) return;
    const auto B=Model->GetBounds();const double Scale=Heights[Kind]/FMath::Max(1.,B.BoxExtent.Z*2);
    Visual->SetStaticMesh(Model);Visual->SetRelativeScale3D(FVector(Scale));
    // Actor origin is the collision centre; PCG supplies a grounded transform.
    Visual->SetRelativeLocation(FVector(-B.Origin.X*Scale,-B.Origin.Y*Scale,-B.Origin.Z*Scale));
    const FVector Extent=HalfSize(Kind);
    Body->SetBoxExtent(FVector(Extent.X*.82,Extent.Y*.82,Extent.Z));
    if (HasAuthority()) ForceNetUpdate();
}

ATrainRoomGenerator::ATrainRoomGenerator()
{
    PrimaryActorTick.bCanEverTick=true;
    auto* Bounds=CreateDefaultSubobject<UBoxComponent>(TEXT("GenerationBounds"));SetRootComponent(Bounds);
    Bounds->SetBoxExtent(FVector(2400,1600,500));Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Bounds->SetCanEverAffectNavigation(false);
    PCG=CreateDefaultSubobject<UPCGComponent>(TEXT("RoomPCG"));
    PCG->GenerationTrigger=EPCGComponentGenerationTrigger::GenerateOnDemand;
}
void ATrainRoomGenerator::Initialize(int32 InRoom,int32 InSeed)
{
    Room=InRoom;Seed=InSeed;Stage=1;Since=GetWorld()->GetTimeSeconds();
    Entry=FVector(-2070,-740,10);Exit=FVector(1920,740,10);
    TActorIterator<APlayerStart> Start(GetWorld());if (Start) Entry=Start->GetActorLocation();
    for (TActorIterator<ATrainStation> It(GetWorld());It;++It) if (It->Kind==3) Exit=It->GetActorLocation()-FVector(180,0,0);
    auto* GS=GetWorld()->GetGameState<ATrainGameState>();
    const int32 Crew=FMath::Clamp(GetGameInstance<UTrainSession>()->ExpeditionCrew,1,4);
    Encounter=TrainEncounters::Compose(Room,Crew,Seed);
    GS->RemainingEnemies=Encounter.Num();GS->RoomReady=false;GS->RoomSeed=Seed;
    GS->EncounterThreat=TrainEncounters::Budget(Room,Crew);
}
bool ATrainRoomGenerator::Reachable(const FVector& Point) const
{
    auto* Path=UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(),Entry,Point);
    return Path&&Path->IsValid()&&!Path->IsPartial();
}
void ATrainRoomGenerator::GenerateLayout()
{
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    auto* Route=UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(),Entry,Exit);
    if (!Nav||!Route||!Route->IsValid()||Route->IsPartial()) return;
    FRandomStream Random(Seed);
    Graph=NewObject<UPCGGraph>(this);
    UPCGCreatePointsSettings* Points=nullptr;auto* Source=Graph->AddNodeOfType(Points);
    // The native node starts with one example point at the origin. Only emit validated placements.
    Points->PointsToCreate.Reset();
    Points->CoordinateSpace=EPCGCoordinateSpace::World;Points->bCullPointsOutsideVolume=false;
    TArray<TPair<FVector,float>> Placed;
    for (int32 Attempt=0;Attempt<700 && Points->PointsToCreate.Num()<10+Room/2;++Attempt)
    {
        const int32 Kind=Theme(Room,Random);const FVector Half=ATrainRoomProp::HalfSize(Kind);
        const float Radius=FVector2D(Half.X,Half.Y).Size();
        const FVector Guess(Random.FRandRange(-1700,1750),Random.FRandRange(-1050,1150),0);
        FNavLocation Ground;if (!Nav->ProjectPointToNavigation(Guess,Ground,FVector(40,40,200))) continue;
        FVector P=Ground.Location;P.Z=0;
        if (FVector::Dist2D(P,Entry)<650 || FVector::Dist2D(P,Exit)<400) continue;
        if ((Room==5||Room==10)&&FVector::Dist2D(P,FVector(Room==5?430:760,0,0))<600+Radius) continue;
        bool Clear=true;
        FVector FacingPoint=P;float Nearest=MAX_flt;
        for (int32 I=1;I<Route->PathPoints.Num();++I)
        {
            const FVector Closest=FMath::ClosestPointOnSegment(P,Route->PathPoints[I-1],Route->PathPoints[I]);
            const float Distance=FVector::Dist(P,Closest);
            if (Distance<Radius+175) Clear=false;
            if (Distance<Nearest) { Nearest=Distance;FacingPoint=Closest; }
        }
        for (const auto& Other:Placed) if (FVector::Dist2D(P,Other.Key)<Radius+Other.Value+100) Clear=false;
        float Yaw=90.f*Random.RandRange(0,3);
        if (Kind==2||Kind==5||Kind==6) Yaw=90.f*FMath::RoundToInt(((FacingPoint-P).Rotation().Yaw-90.f)/90.f);
        const FQuat Rotation(FRotator(0,Yaw,0));
        for (int32 X:{-1,1}) for (int32 Y:{-1,1})
        {
            FNavLocation Corner;const FVector Sample=P+Rotation.RotateVector(FVector(Half.X*X,Half.Y*Y,0));
            if (!Nav->ProjectPointToNavigation(Sample,Corner,FVector(30,30,200))) Clear=false;
        }
        if (!Clear || GetWorld()->OverlapBlockingTestByChannel(P+FVector(0,0,Half.Z+12),Rotation,ECC_Pawn,
            FCollisionShape::MakeBox(Half+FVector(20,20,0)))) continue;
        FPCGPoint Point;Point.Transform=FTransform(Rotation,P+FVector(0,0,Half.Z));
        Point.Seed=Kind+1; // Keep zero out of Create Points: it is a request for an automatic seed.
        Points->PointsToCreate.Add(Point);Placed.Emplace(P,Radius);
    }
    UPCGSpawnActorSettings* Spawn=nullptr;auto* Spawner=Graph->AddNodeOfType(Spawn);
    Spawn->SetTemplateActorClass(ATrainRoomProp::StaticClass());Spawn->Option=EPCGSpawnActorOption::NoMerging;
    Spawn->AttachOptions=EPCGAttachOptions::NotAttached;Spawn->PostSpawnFunctionNames={TEXT("ApplyModule")};
    FPCGObjectPropertyOverrideDescription Override;Override.InputSource.SetPointProperty(EPCGPointProperties::Seed);Override.PropertyTarget=TEXT("SpawnKindCode");
    Spawn->SpawnedActorPropertyOverrideDescriptions.Add(Override);
    Graph->AddEdge(Source,PCGPinConstants::DefaultOutputLabel,Spawner,PCGPinConstants::DefaultInputLabel);
    Graph->AddEdge(Spawner,PCGPinConstants::DefaultOutputLabel,Graph->GetOutputNode(),PCGPinConstants::DefaultOutputLabel);
    PCG->Seed=Seed;PCG->SetGraphLocal(Graph);PCG->OnPCGGraphGeneratedDelegate.AddUObject(this,&ATrainRoomGenerator::Generated);
    Stage=2;Since=GetWorld()->GetTimeSeconds();PCG->GenerateLocal(EPCGComponentGenerationTrigger::GenerateOnDemand,true,PCGHiGenGrid::UninitializedGridSize());
    UE_LOG(LogTemp,Display,TEXT("TRAIN_PCG_REQUEST room=%d seed=%d props=%d threat=%d enemies=%d"),Room,Seed,Points->PointsToCreate.Num(),GetWorld()->GetGameState<ATrainGameState>()->EncounterThreat,Encounter.Num());
}
void ATrainRoomGenerator::Generated(UPCGComponent*) { Stage=3;Since=GetWorld()->GetTimeSeconds(); }
bool ATrainRoomGenerator::SpawnEncounter()
{
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (!Nav) return false;
    FRandomStream Random(Seed^0x5e37a21);SpawnLocations.Reset();
    for (int32 Index=0;Index<Encounter.Num();++Index)
    {
        const bool Boss=Encounter[Index]==ETrainEnemyType::Lockkeeper||Encounter[Index]==ETrainEnemyType::Guardian;
        bool Found=false;
        for (int32 Attempt=0;Attempt<400;++Attempt)
        {
            // Spread groups through the route; avoid an entire room rushing the entry.
            const int32 Group=Index%4;
            FVector Guess(Random.FRandRange(-1050+Group*700,-650+Group*700),Random.FRandRange(-1000,1100),0);
            if (Attempt>180) Guess=FVector(Random.FRandRange(-1450,1800),Random.FRandRange(-1100,1200),0);
            if (Boss) Guess=FVector(Room==5?430:760,0,0)+FVector(Random.FRandRange(-140,140),Random.FRandRange(-140,140),0);
            FNavLocation Location;if (!Nav->ProjectPointToNavigation(Guess,Location,FVector(70,70,200))) continue;
            const FVector P=Location.Location+FVector(0,0,106);
            if (FVector::Dist2D(P,Entry)<1300 || FVector::Dist2D(P,Exit)<360) continue;
            if (!Boss && (Room==5||Room==10) && FVector::Dist2D(P,FVector(Room==5?430:760,0,0))<480) continue;
            bool Clear=true;for (const FVector& Other:SpawnLocations) if (FVector::Dist2D(P,Other)<160) Clear=false;
            if (!Clear || GetWorld()->OverlapBlockingTestByChannel(P,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(58,100))) continue;
            if (!Reachable(Location.Location)) continue;
            SpawnLocations.Add(P);Found=true;break;
        }
        if (!Found) return false;
    }
    for (int32 I=0;I<Encounter.Num();++I)
    {
        FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Enemy=GetWorld()->SpawnActor<ATrainEnemy>(SpawnLocations[I],FRotator(0,Random.FRandRange(-180,180),0),Params);
        if (!Enemy) { UE_LOG(LogTemp,Error,TEXT("Failed to spawn planned enemy"));return false; }
        Enemy->Configure(Encounter[I]);
    }
    auto* GS=GetWorld()->GetGameState<ATrainGameState>();GS->RoomReady=true;GS->ForceNetUpdate();
    Stage=4;SetActorTickEnabled(false);
    UE_LOG(LogTemp,Display,TEXT("TRAIN_ROOM_READY room=%d seed=%d enemies=%d props=%d"),Room,Seed,Encounter.Num(),GS->LayoutProps);
    return true;
}
void ATrainRoomGenerator::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);if (!HasAuthority()||!Room) return;
    const double Age=GetWorld()->GetTimeSeconds()-Since;
    if (Stage==1 && Age>.15 && !UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(GetWorld())) GenerateLayout();
    if (Stage==3 && Age>.8 && !UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(GetWorld()))
    {
        auto* GS=GetWorld()->GetGameState<ATrainGameState>();GS->LayoutProps=0;
        for (TActorIterator<ATrainRoomProp> It(GetWorld());It;++It) ++GS->LayoutProps;
        if (Reachable(Exit) && SpawnEncounter()) return;
        // A navigable expedition takes priority if an asset's collision changes later.
        if (GS->LayoutProps>0)
        {
            UE_LOG(LogTemp,Warning,TEXT("PCG layout rejected by navigation; clearing furniture before retry"));
            for (TActorIterator<ATrainRoomProp> It(GetWorld());It;++It) It->Destroy();
            Since=GetWorld()->GetTimeSeconds();
        }
    }
    if (Stage!=4 && Age>20)
    {
        UE_LOG(LogTemp,Error,TEXT("Room generation stalled: room=%d stage=%d seed=%d"),Room,Stage,Seed);
        SetActorTickEnabled(false);
    }
}
