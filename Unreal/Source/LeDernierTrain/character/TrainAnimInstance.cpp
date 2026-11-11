#include "TrainAnimInstance.h"
#include "../network/TrainNetwork.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/Pawn.h"

namespace
{
    UAnimSequence* Sequence(bool Enemy,FName Name)
    { return LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/Characters/%s/Animations/%s.%s"),Enemy?TEXT("Automaton"):TEXT("Conductor"),*Name.ToString(),*Name.ToString())); }
}
void UTrainAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();const bool Enemy=GetOwningActor() && GetOwningActor()->IsA<ATrainEnemy>();
    Idle=Sequence(Enemy,TEXT("Idle"));Run=Sequence(Enemy,Enemy?TEXT("Walk"):TEXT("Run"));Death=Sequence(Enemy,TEXT("Death"));
    if (!Enemy) Dodge=Sequence(false,TEXT("Dodge"));
}
void UTrainAnimInstance::Action(FName Name,float Duration)
{
    const bool Enemy=GetOwningActor() && GetOwningActor()->IsA<ATrainEnemy>();
    if (auto* Seq=Sequence(Enemy,Name))
        PlaySlotAnimationAsDynamicMontage(Seq,TEXT("DefaultSlot"),.07f,.12f,Seq->GetPlayLength()/FMath::Max(.1f,Duration),1);
}
void FTrainAnimProxy::Initialize(UAnimInstance* Instance)
{
    Idle.SetLoopAnimation(true);Run.SetLoopAnimation(true);
    Locomotion.A.SetLinkNode(&Idle);Locomotion.B.SetLinkNode(&Run);Actions.Source.SetLinkNode(&Locomotion);
    Actions.SlotName=TEXT("DefaultSlot");
    FAnimInstanceProxy::Initialize(Instance);
}
void FTrainAnimProxy::PreUpdate(UAnimInstance* Instance,float DeltaSeconds)
{
    FAnimInstanceProxy::PreUpdate(Instance,DeltaSeconds);
    auto* Anim=CastChecked<UTrainAnimInstance>(Instance);
    const auto* Pawn=Anim->TryGetPawnOwner(); const float Speed=Pawn?Pawn->GetVelocity().Size2D():0;
    const auto* PS=Pawn?Pawn->GetPlayerState<ATrainPlayerState>():nullptr;
    const bool Dead=PS && PS->Health<=0;
    Idle.SetSequence(Dead?Anim->Death:Anim->Idle);Idle.SetLoopAnimation(!Dead);Run.SetSequence(Anim->Run);
    if (Dead && Anim->Death) Idle.SetAccumulatedTime(FMath::Max(0.f,Anim->Death->GetPlayLength()-.01f));
    Locomotion.Alpha=FMath::FInterpTo(Locomotion.Alpha,!Dead&&Speed>8?1.f:0.f,DeltaSeconds,18.f);
    // The conductor's clip plants the foot at about 430 cm/s at its original rate.
    Run.SetPlayRate(FMath::Clamp(Speed/(Pawn&&Pawn->IsA<ATrainEnemy>() ? 115.f:430.f),.65f,1.4f));
}
void FTrainAnimProxy::UpdateAnimationNode(const FAnimationUpdateContext& Context) { Actions.Update_AnyThread(Context); }
bool FTrainAnimProxy::Evaluate(FPoseContext& Output) { Actions.Evaluate_AnyThread(Output);return true; }
