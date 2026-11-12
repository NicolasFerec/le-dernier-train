#include "TrainCombatEffect.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

ATrainCombatEffect::ATrainCombatEffect()
{
    PrimaryActorTick.bCanEverTick = true;
    Shape = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shape"));
    SetRootComponent(Shape);
    Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Shape->SetCanEverAffectNavigation(false);
    Shape->SetCastShadow(false);
    SlashMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Effects/Slash.Slash"));
    ImpactMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Effects/Impact.Impact"));
    Surface=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Effects/M_Combat.M_Combat"));
}
void ATrainCombatEffect::Spawn(UWorld* World, const FVector& Position, const FRotator& Facing, bool Impact, bool Heavy)
{
    if (!World || World->GetNetMode() == NM_DedicatedServer) return;
    auto* Effect = World->SpawnActor<ATrainCombatEffect>(Position,Facing);
    if (!Effect) return;
    Effect->Shape->SetStaticMesh(Impact?Effect->ImpactMesh:Effect->SlashMesh);
    Effect->Shape->SetMaterial(0,Effect->Surface);
    Effect->Material = Effect->Shape->CreateDynamicMaterialInstance(0);
    if (Effect->Material)
        Effect->Material->SetVectorParameterValue(TEXT("Tint"),Impact?FLinearColor(3.5f,1.6f,.3f):FLinearColor(1.2f,2.1f,2.5f));
    Effect->SetActorScale3D(FVector(Heavy?1.25f:1.f));
    Effect->Duration = Impact?.15f:.16f;
    Effect->SetLifeSpan(Effect->Duration);
}
void ATrainCombatEffect::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Age += DeltaSeconds;
    if (Material) Material->SetScalarParameterValue(TEXT("Fade"),FMath::Max(0.f,1.f-Age/Duration));
}
