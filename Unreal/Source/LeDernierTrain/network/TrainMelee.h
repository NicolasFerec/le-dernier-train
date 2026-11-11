#pragma once
#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "TrainMelee.generated.h"

UCLASS()
class UTrainLightAttack : public UGameplayAbility
{
    GENERATED_BODY()
public:
    UTrainLightAttack();
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,bool Replicate,bool Cancelled) override;
protected:
    bool Heavy=false;
private:
    UFUNCTION() void Finished();
};
UCLASS()
class UTrainHeavyAttack : public UTrainLightAttack
{
    GENERATED_BODY()
public:
    UTrainHeavyAttack() { Heavy=true; }
};
