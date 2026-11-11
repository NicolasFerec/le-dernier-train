#include "TrainDash.h"
#include "TrainNetwork.h"
#include "../TrainPrototype.h"
#include "../character/TrainAnimInstance.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
UTrainDash::UTrainDash()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}
void UTrainDash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    auto* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    auto* PS = Character ? Character->GetPlayerState<ATrainPlayerState>() : nullptr;
    auto* State = Character ? Character->GetWorld()->GetGameState<ATrainGameState>() : nullptr;
    if (!Character || !PS || PS->Health <= 0 || UGameplayStatics::IsGamePaused(Character) || !State || State->RunFinished || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
    if (auto* Anim = Cast<UTrainAnimInstance>(Character->GetMesh()->GetAnimInstance()); Anim && Anim->Dodge)
    {
        constexpr float PoseDuration = .32f;
        // The fractional-loop API includes the play count in the replicated montage.
        ActorInfo->AbilitySystemComponent->PlaySlotAnimationAsDynamicMontage_WithFractionalLoops(
            this, ActivationInfo, Anim->Dodge, TEXT("DefaultSlot"), .035f, .09f,
            Anim->Dodge->GetPlayLength() / PoseDuration, 0.f, 1.f);
    }
    if (Character->HasAuthority())
    {
        PS->DashReadyAt = State->GetServerWorldTimeSeconds() + .65;
        PS->ForceNetUpdate();
        if (auto* Pawn = Cast<ATrainPawn>(Character)) Pawn->MulticastDashSound();
    }
    FVector Direction = Character->GetVelocity().GetSafeNormal2D();
    if (Direction.IsNearlyZero()) Direction = Character->GetActorForwardVector();
    auto* Task = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(this, TEXT("Dodge"), Direction,
        850, .16f, false, nullptr, ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, 345, false);
    Task->ReadyForActivation();
    auto* Cooldown = UAbilityTask_WaitDelay::WaitDelay(this, .65f);
    Cooldown->OnFinish.AddDynamic(this, &UTrainDash::Finished); Cooldown->ReadyForActivation();
}
void UTrainDash::Finished() { EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false); }
