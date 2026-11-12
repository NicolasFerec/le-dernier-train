#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../network/TrainNetwork.h"
#include "TrainRoomGeneration.generated.h"

namespace TrainEncounters
{
    int32 Cost(ETrainEnemyType Type);
    int32 Budget(int32 Room, int32 Crew);
    TArray<ETrainEnemyType> Compose(int32 Room, int32 Crew, int32 Seed);
}

// PCG creates these ordinary replicated actors. Native collision and Recast modifiers
// give the host and guests the same solid furniture without generating a second layout.
UCLASS()
class ATrainRoomProp : public AActor
{
    GENERATED_BODY()
public:
    ATrainRoomProp();
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    UPROPERTY(EditAnywhere, ReplicatedUsing=ApplyModule) int32 Kind=0;
    // One-based PCG input: Create Points replaces a zero seed with a positional hash.
    UPROPERTY(EditAnywhere) int32 SpawnKindCode=0;
    UFUNCTION(CallInEditor) void ApplyModule();
    static FVector HalfSize(int32 Kind);
private:
    UPROPERTY() TObjectPtr<class UStaticMeshComponent> Visual;
    UPROPERTY() TObjectPtr<class UBoxComponent> Body;
    UPROPERTY() TArray<TObjectPtr<class UStaticMesh>> Models;
};

// Authority-only PCG execution; output actors and enemies use Unreal replication.
UCLASS()
class ATrainRoomGenerator : public AActor
{
    GENERATED_BODY()
public:
    ATrainRoomGenerator();
    void Initialize(int32 InRoom, int32 InSeed);
    virtual void Tick(float DeltaSeconds) override;
    const TArray<FVector>& GetSpawnLocations() const { return SpawnLocations; }
private:
    UPROPERTY() TObjectPtr<class UPCGComponent> PCG;
    UPROPERTY() TObjectPtr<class UPCGGraph> Graph;
    int32 Room=0, Seed=0, Stage=0;
    double Since=0;
    FVector Entry, Exit;
    TArray<ETrainEnemyType> Encounter;
    TArray<FVector> SpawnLocations;
    void GenerateLayout();
    void Generated(class UPCGComponent* Component);
    bool SpawnEncounter();
    bool Reachable(const FVector& Point) const;
};
