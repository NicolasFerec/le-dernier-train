#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "TrainPrototype.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class STextBlock;
class UInputAction;
class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;
class SBox;

UCLASS()
class UTrainMenu : public UUserWidget
{
    GENERATED_BODY()

public:
    bool PauseMode = false;
    bool SessionMode = false;
    bool SummaryMode = false;
    int32 Station=-1;
    void Resume();

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    virtual FReply NativeOnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event) override;

private:
    TSharedRef<SWidget> BuildContent();
    TSharedRef<SWidget> BuildSettings();
    TSharedRef<SWidget> BuildSession();
    TSharedRef<SWidget> BuildSummary();
    TSharedRef<SWidget> BuildStation();
    void Refresh();
    void ChangeTab(int32 Delta);
    void FocusFirst();
    UPROPERTY(Transient) TObjectPtr<UTexture2D> Backdrop;
    TSharedPtr<SBox> RootHost;
    TWeakPtr<SWidget> FirstFocus;
    FSlateBrush BackgroundBrush, BorderBrush, PanelBrush, DimBrush;
    FButtonStyle ButtonStyle, ActiveTabStyle;
    FSliderStyle SliderStyle;
    FTextBlockStyle BindingTextStyle;
    int32 Page = 0;
    int32 Tab = 0;
    bool bSelectingKey = false;
    FString BindingNotice;
    FDelegateHandle SessionListener;
};

UCLASS()
class UTrainStatus : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetMessage(const FText& Value);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
    TSharedPtr<STextBlock> Label;
};

UCLASS()
class ATrainPawn : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ATrainPawn();
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    void TogglePause();
    void RefreshControls();
    void ShowSession();
    void ShowStation(int32 Station);
    void SendLoadout();
    void PredictMelee(bool Heavy);
    void CancelPredictedMelee();
    void FinishPredictedMelee() { PredictedMelee=false; }
    bool IsMenuOpen() const { return PauseMenu!=nullptr; }
    void ReceiveHit(float Damage);
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    virtual void OnRep_PlayerState() override;
    virtual void PossessedBy(AController* NewController) override;
    UFUNCTION(Server, Reliable) void ServerAttack(bool Heavy);
    UFUNCTION(Server, Reliable) void ServerInteract();
    UFUNCTION(NetMulticast, Unreliable) void MulticastSwing(bool Heavy, FRotator Direction);
    UFUNCTION(NetMulticast, Unreliable) void MulticastDodge();
    UFUNCTION(NetMulticast, Unreliable) void MulticastDamage(float Damage,bool Dead);
    UFUNCTION(Server, Reliable) void ServerLoadout(int32 NewCharacter,int32 NewWeapon,bool Health,bool Speed);
    UFUNCTION() void ApplyLoadout();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    UPROPERTY(ReplicatedUsing=ApplyLoadout) int32 Character=0;
    UPROPERTY(ReplicatedUsing=ApplyLoadout) int32 Weapon=0;

private:
    void Interact();
    void Dash();
    void Attack();
    void HeavyAttack();
    FVector2D MovementInput() const;
    void SetMessage(const FText& Text);

    UPROPERTY()
    TObjectPtr<UCameraComponent> Camera;
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> HeldWeapon;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> OffhandWeapon;
    UPROPERTY() TObjectPtr<class USpringArmComponent> SpringArm;
    UPROPERTY()
    TObjectPtr<UTrainStatus> Message;
    UPROPERTY() TObjectPtr<UTrainMenu> PauseMenu;

    UPROPERTY(Transient)
    TObjectPtr<UInputMappingContext> MovementContext;
    UPROPERTY(Transient)
    TObjectPtr<UInputAction> MoveAction;
    TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> InputSubsystem;

    FVector Heading = FVector::ForwardVector;
    UPROPERTY() TObjectPtr<UAbilitySystemComponent> Abilities;
    float AttackCooldown = 0;
    float SwingTime = 0;
    FRotator SwingDirection;
    bool PredictedMelee=false;
    FTimerHandle StrikeTimer;
    void ResolveStrike(bool Heavy,FRotator Direction);
    bool bWagon = false;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
};

UCLASS()
class ATrainGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ATrainGameMode();
    virtual void StartPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& Error) override;
    virtual void Logout(AController* Exiting) override;
    void RequestDeparture(class ATrainPawn* Requester);
private:
    bool EveryoneReady() const;
    void Depart();
    void FinishRun(bool Victory);
    bool Rewarded=false;
};
