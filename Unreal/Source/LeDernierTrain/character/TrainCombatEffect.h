#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrainCombatEffect.generated.h"

// Short-lived, local visual feedback. Hit authority remains with the combat actors.
UCLASS()
class ATrainCombatEffect : public AActor
{
    GENERATED_BODY()
public:
    ATrainCombatEffect();
    static void Spawn(UWorld* World, const FVector& Position, const FRotator& Facing, bool Impact, bool Heavy=false);
    virtual void Tick(float DeltaSeconds) override;
private:
    UPROPERTY() TObjectPtr<class UStaticMeshComponent> Shape;
    UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> Material;
    UPROPERTY() TObjectPtr<class UStaticMesh> SlashMesh;
    UPROPERTY() TObjectPtr<class UStaticMesh> ImpactMesh;
    UPROPERTY() TObjectPtr<class UMaterialInterface> Surface;
    float Age = 0;
    float Duration = .18f;
};
