#include "TrainWorldAuthoring.h"
#include "Engine/SkeletalMesh.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#if WITH_EDITOR
#include "Builders/CubeBuilder.h"
#include "ActorFactories/ActorFactory.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "Components/BrushComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#endif
void UTrainWorldAuthoring::BuildNavigation(UObject* WorldContext,FVector Extent)
{
#if WITH_EDITOR
    auto* World=GEngine->GetWorldFromContextObject(WorldContext,EGetWorldErrorMode::ReturnNull);if (!World) return;
    auto* Volume=World->SpawnActor<ANavMeshBoundsVolume>();
    auto* Builder=NewObject<UCubeBuilder>();Builder->X=Extent.X*2;Builder->Y=Extent.Y*2;Builder->Z=600;
    UActorFactory::CreateBrushForVolumeActor(Volume,Builder);Volume->SetActorLocation(FVector(0,0,150));
    Volume->GetBrushComponent()->UpdateBounds();Volume->ReregisterAllComponents();
    for (TActorIterator<ANavMeshBoundsVolume> It(World);It;++It) if (*It!=Volume) It->Destroy();
    if (auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
    {
        Nav->RemoveNavigationBuildLock(ENavigationBuildLock::AsyncLoadLock,UNavigationSystemV1::ELockRemovalRebuildAction::NoRebuild);
        Nav->OnNavigationBoundsUpdated(Volume);
        Nav->Tick(.1f);Nav->ProcessPendingOctreeUpdates();
        Nav->GetDefaultNavDataInstance(FNavigationSystem::Create);
        Nav->Build();
        for (auto Data:Nav->NavDataSet) if (Data) Data->EnsureBuildCompletion();
        UE_LOG(LogTemp,Display,TEXT("Train navigation: %d data actors, volume %s"),Nav->NavDataSet.Num(),*Volume->GetComponentsBoundingBox(true).ToString());
    }
#endif
}
void UTrainWorldAuthoring::AssignSurface(USkeletalMesh* Mesh,UMaterialInterface* Material)
{
#if WITH_EDITOR
    if (!Mesh || !Material) return;
    Mesh->Modify();auto Materials=Mesh->GetMaterials();
    for (auto& Slot:Materials) Slot.MaterialInterface=Material;
    Mesh->SetMaterials(Materials);Mesh->PostEditChange();Mesh->MarkPackageDirty();
#endif
}
AActor* UTrainWorldAuthoring::AddRepeatedMesh(UObject* WorldContext,UStaticMesh* Mesh,UMaterialInterface* Material,const TArray<FTransform>& Transforms)
{
#if WITH_EDITOR
    auto* World=GEngine->GetWorldFromContextObject(WorldContext,EGetWorldErrorMode::ReturnNull);if (!World || !Mesh) return nullptr;
    auto* Owner=World->SpawnActor<AActor>();Owner->SetActorLabel(TEXT("Sol instancié"));
    auto* Instances=NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner,TEXT("RepeatedMesh"),RF_Transactional);
    Owner->SetRootComponent(Instances);Owner->AddInstanceComponent(Instances);
    Instances->SetStaticMesh(Mesh);Instances->SetMaterial(0,Material);Instances->SetMobility(EComponentMobility::Static);
    Instances->RegisterComponent();Instances->AddInstances(Transforms,false,true);Owner->MarkPackageDirty();return Owner;
#else
    return nullptr;
#endif
}
