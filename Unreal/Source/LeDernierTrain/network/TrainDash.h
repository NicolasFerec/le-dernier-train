#pragma once
#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "TrainDash.generated.h"
// Epic GAS supplies activation prediction, task replication and root-motion correction.
UCLASS()
class UTrainDash : public UGameplayAbility
{
    GENERATED_BODY()
public:
    static constexpr float LeapDuration = .24f;
    static constexpr float CooldownDuration = .52f;

    UTrainDash();
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
private:
    UFUNCTION() void Finished();
};
