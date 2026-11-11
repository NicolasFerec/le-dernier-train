#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "AnimNodes/AnimNode_TwoWayBlend.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "TrainAnimInstance.generated.h"

// Native animation nodes evaluate on Unreal's animation worker. Gameplay only supplies speed/actions.
struct FTrainAnimProxy : public FAnimInstanceProxy
{
    FTrainAnimProxy()=default;
    explicit FTrainAnimProxy(UAnimInstance* Instance):FAnimInstanceProxy(Instance) {}
    virtual void Initialize(UAnimInstance* Instance) override;
    virtual void PreUpdate(UAnimInstance* Instance,float DeltaSeconds) override;
    virtual void UpdateAnimationNode(const FAnimationUpdateContext& Context) override;
    virtual bool Evaluate(FPoseContext& Output) override;
    virtual FAnimNode_Base* GetCustomRootNode() override { return &Actions; }
    // These nodes live in a native proxy, outside an animation Blueprint's baked data.
    FAnimNode_SequencePlayer_Standalone Idle,Run;
    FAnimNode_TwoWayBlend Locomotion;
    FAnimNode_Slot Actions;
};
UCLASS(Transient)
class UTrainAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Idle;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Run;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Death;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Dodge;
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FTrainAnimProxy(this); }
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override { delete static_cast<FTrainAnimProxy*>(Proxy); }
    virtual void NativeInitializeAnimation() override;
    void Action(FName Name,float Duration);
};
