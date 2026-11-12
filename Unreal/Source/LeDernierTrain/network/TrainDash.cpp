#include "TrainDash.h"
#include "TrainNetwork.h"
#include "../TrainPrototype.h"
#include "../character/TrainAnimInstance.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
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
    auto* Character = Cast<ATrainPawn>(ActorInfo->AvatarActor.Get());
    auto* PS = Character ? Character->GetPlayerState<ATrainPlayerState>() : nullptr;
    auto* State = Character ? Character->GetWorld()->GetGameState<ATrainGameState>() : nullptr;
    if (!Character || !Character->CanDodge() || !PS || PS->Health <= 0 || UGameplayStatics::IsGamePaused(Character) || !State || !State->RoomReady || State->RunFinished || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
    if (auto* Anim = Cast<UTrainAnimInstance>(Character->GetMesh()->GetAnimInstance()); Anim && Anim->Dodge)
    {
        constexpr float PoseDuration = LeapDuration + .07f;
        // The fractional-loop API includes the play count in the replicated montage.
        ActorInfo->AbilitySystemComponent->PlaySlotAnimationAsDynamicMontage_WithFractionalLoops(
            this, ActivationInfo, Anim->Dodge, TEXT("DefaultSlot"), .025f, .065f,
            Anim->Dodge->GetPlayLength() / PoseDuration, 0.f, 1.f);
    }
    if (Character->HasAuthority())
    {
        PS->DashReadyAt = State->GetServerWorldTimeSeconds() + CooldownDuration;
        PS->ForceNetUpdate();
        Character->MulticastDashSound();
    }
    auto* Movement = Character->GetCharacterMovement();
    FVector Direction = Movement->GetCurrentAcceleration().GetSafeNormal2D();
    if (Direction.IsNearlyZero()) Direction = Character->GetVelocity().GetSafeNormal2D();
    if (Direction.IsNearlyZero()) Direction = Character->GetActorForwardVector();
    const FRotator Facing(0, Direction.Rotation().Yaw, 0);
    Character->SetActorRotation(Facing);
    auto* Task = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
        this, TEXT("Dodge"), Direction, 1150.f, LeapDuration, false, nullptr,
        ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector,
        Movement->GetMaxSpeed(), true);
    // A finite dodge must stop at its duration even when a frame crosses the end.
    // ConstantForce defaults to preserving a full last tick for sustained forces.
    if (auto Source=Movement->GetRootMotionSource(TEXT("Dodge")))
        Source->Settings.UnSetFlag(ERootMotionSourceSettingsFlags::DisablePartialEndTick);
    Task->ReadyForActivation();
    auto* Cooldown = UAbilityTask_WaitDelay::WaitDelay(this, CooldownDuration);
    Cooldown->OnFinish.AddDynamic(this, &UTrainDash::Finished); Cooldown->ReadyForActivation();
}
void UTrainDash::Finished() { EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false); }
