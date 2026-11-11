#include "TrainMelee.h"
#include "TrainNetwork.h"
#include "../TrainPrototype.h"
#include "NativeGameplayTags.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
UE_DEFINE_GAMEPLAY_TAG_STATIC(TrainAttacking,"State.Attacking");
UTrainLightAttack::UTrainLightAttack()
{
    InstancingPolicy=EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy=EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    ActivationOwnedTags.AddTag(TrainAttacking);ActivationBlockedTags.AddTag(TrainAttacking);
}
void UTrainLightAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,const FGameplayEventData* TriggerEventData)
{
    auto* Pawn=Cast<ATrainPawn>(ActorInfo->AvatarActor.Get());
    auto* PS=Pawn?Pawn->GetPlayerState<ATrainPlayerState>():nullptr;
    auto* State=Pawn?Pawn->GetWorld()->GetGameState<ATrainGameState>():nullptr;
    if (!Pawn || !PS || PS->Health<=0 || !State || State->RunFinished || UGameplayStatics::IsGamePaused(Pawn)
        || !Pawn->GetCharacterMovement()->IsMovingOnGround()
        || !CommitAbility(Handle,ActorInfo,ActivationInfo))
    { EndAbility(Handle,ActorInfo,ActivationInfo,true,true);return; }
    // GAS predicts acceptance and prevents overlapping light/heavy abilities.
    // Cosmetic motion starts locally; the server still resolves every hit.
    if (!Pawn->HasAuthority()) Pawn->PredictMelee(Heavy);
    else Pawn->ServerAttack(Heavy);
    const float Cooldown = Pawn->MeleeDuration(Heavy);
    auto* Wait=UAbilityTask_WaitDelay::WaitDelay(this,Cooldown);Wait->OnFinish.AddDynamic(this,&UTrainLightAttack::Finished);Wait->ReadyForActivation();
}
void UTrainLightAttack::Finished() { EndAbility(CurrentSpecHandle,CurrentActorInfo,CurrentActivationInfo,true,false); }
void UTrainLightAttack::EndAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,bool Replicate,bool Cancelled)
{
    if (ActorInfo) if (auto* Pawn=Cast<ATrainPawn>(ActorInfo->AvatarActor.Get()))
    { if (Cancelled) Pawn->CancelPredictedMelee();else Pawn->FinishPredictedMelee(); }
    Super::EndAbility(Handle,ActorInfo,ActivationInfo,Replicate,Cancelled);
}
