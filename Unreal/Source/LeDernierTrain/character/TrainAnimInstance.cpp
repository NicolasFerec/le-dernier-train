#include "TrainAnimInstance.h"
#include "../network/TrainNetwork.h"
#include "../TrainPrototype.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"

namespace
{
    UAnimSequence* Sequence(const AActor* Owner, FName Name)
    {
        const auto* Enemy = Cast<ATrainEnemy>(Owner);
        const auto* Player = Cast<ATrainPawn>(Owner);
        return LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/Characters/%s/Animations/%s.%s"),
            Enemy ? Enemy->AnimationFolder() : Player&&Player->Character?TEXT("Scout"):TEXT("ConductorUE5"), *Name.ToString(), *Name.ToString()));
    }
}
void UTrainAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();const bool Enemy=GetOwningActor() && GetOwningActor()->IsA<ATrainEnemy>();
    const auto* Owner = GetOwningActor();
    Idle = Sequence(Owner, TEXT("Idle"));
    Run = Sequence(Owner, Enemy && !CastChecked<ATrainEnemy>(Owner)->Runs() ? TEXT("Walk") : TEXT("Run"));
    Death = Sequence(Owner, TEXT("Death"));
    if (!Enemy) Dodge = Sequence(Owner, TEXT("Dodge"));
    ActionClips.Reset();
    for (FName Name : {FName(TEXT("Hit")), FName(TEXT("Death"))}) ActionClips.Add(Name,Sequence(Owner,Name));
    if (Enemy) ActionClips.Add(TEXT("Attack"),Sequence(Owner,TEXT("Attack")));
    else
    {
        for (FName Name : {FName(TEXT("Light")),FName(TEXT("LightReturn")),FName(TEXT("Heavy"))}) ActionClips.Add(Name,Sequence(Owner,Name));
        if (const auto* Player=Cast<ATrainPawn>(Owner);Player&&Player->Character)
            ActionClips.Add(TEXT("DualHeavy"),Sequence(Owner,TEXT("DualHeavy")));
        else for(FName Name:{FName(TEXT("HammerLight")),FName(TEXT("HammerHeavy"))}) ActionClips.Add(Name,Sequence(Owner,Name));
    }
}
void UTrainAnimInstance::Action(FName Name,float Duration)
{
    if (const auto* Seq=ActionClips.Find(Name); Seq && *Seq)
        PlaySlotAnimationAsDynamicMontage(*Seq,TEXT("DefaultSlot"),.025f,.07f,(*Seq)->GetPlayLength()/FMath::Max(.1f,Duration),1);
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
    Locomotion.Alpha=FMath::FInterpTo(Locomotion.Alpha,!Dead&&Speed>8?1.f:0.f,DeltaSeconds,24.f);
    // Measured backward foot velocity during planted phases of the imported clips.
    // Account for the runner and bosses' visual scale as well as their movement speed.
    const auto* Enemy = Cast<ATrainEnemy>(Pawn);
    const auto* Player = Cast<ATrainPawn>(Pawn);
    const float StrideSpeed = Enemy ? (Enemy->Runs()?454.f:137.f)*Enemy->GetMesh()->GetComponentScale().Z : Player&&Player->Character ? 476.f : 483.f;
    Run.SetPlayRate(FMath::Clamp(Speed/FMath::Max(1.f,StrideSpeed),.35f,2.f));
}
void FTrainAnimProxy::UpdateAnimationNode(const FAnimationUpdateContext& Context) { Actions.Update_AnyThread(Context); }
bool FTrainAnimProxy::Evaluate(FPoseContext& Output) { Actions.Evaluate_AnyThread(Output);return true; }
