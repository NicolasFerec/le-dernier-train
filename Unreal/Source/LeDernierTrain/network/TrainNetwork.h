#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Engine/StaticMeshActor.h"
#include "TrainNetwork.generated.h"

FText TrainText(const TCHAR* FR, const TCHAR* EN);

UENUM()
enum class ETrainNotice : uint8
{
    OnlyHostCanPause,
    OnlyHostCanDepart,
    HostLeft,
};

UCLASS()
class ATrainPlayerState : public APlayerState
{
    GENERATED_BODY()
public:
    ATrainPlayerState();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    virtual void CopyProperties(APlayerState* NewState) override;
    UPROPERTY(Replicated) bool IsHost = false;
    UPROPERTY(Replicated) float Health = 100;
    UPROPERTY(Replicated) float MaxHealth = 100;
    UPROPERTY(Replicated) int32 Character=0;
    UPROPERTY(Replicated) int32 Weapon=0;
    UPROPERTY(ReplicatedUsing=OnRep_Loadout) bool HealthUpgrade=false;
    UPROPERTY(ReplicatedUsing=OnRep_Loadout) bool SpeedUpgrade=false;
    UFUNCTION() void OnRep_Loadout();
    UPROPERTY(Replicated) float DamageDealt = 0;
    UPROPERTY(Replicated) float DamageTaken = 0;
    UPROPERTY(Replicated) int32 Kills = 0;
    UPROPERTY(Replicated) double DashReadyAt = 0;
};

UCLASS()
class ATrainGameState : public AGameStateBase
{
    GENERATED_BODY()
public:
    static constexpr double ExitRadius = 250.0;
    bool CanFallenHostDepart(const ATrainPlayerState* Host) const;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    UPROPERTY(Replicated) int32 RemainingEnemies = 0;
    UPROPERTY(Replicated) int32 RoomsCleared = 0;
    UPROPERTY(Replicated) double DepartureAt = 0;
    UPROPERTY(Replicated) bool RunFinished = false;
    UPROPERTY(Replicated) bool Victory = false;
    UPROPERTY(Replicated) FVector DeparturePoint = FVector::ZeroVector;
    UPROPERTY(Replicated) int32 RoomIndex=0;
    UPROPERTY(Replicated) bool RoomReady=true;
    UPROPERTY(Replicated) int32 RoomSeed=0;
    UPROPERTY(Replicated) int32 EncounterThreat=0;
    UPROPERTY(Replicated) int32 LayoutProps=0;
};

UCLASS()
class ATrainPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    ATrainPlayerController();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual bool InputKey(const FInputKeyEventArgs& Params) override;
    virtual void ClientReturnToMainMenuWithTextReason_Implementation(const FText& Reason) override;
    UFUNCTION(Client, Unreliable) void ClientHurt();
    UFUNCTION(Client, Reliable) void ClientNotice(ETrainNotice Notice);
    UFUNCTION(Client, Reliable) void ClientReward(int32 Points);
    void CombatText(FVector Point,float Damage,bool Hurt);
    FText Notice() const;
    void StopMenuAudio();
private:
    void SetupLocalWorld();
    FString ActiveMap;
    TArray<TWeakObjectPtr<class UAudioComponent>> MenuAudio;
    FText NoticeText;
    double NoticeUntil = 0;
    UPROPERTY() TObjectPtr<class UTrainHUD> HUD;
    UPROPERTY() TObjectPtr<class UTrainMenu> Summary;
};

UENUM()
enum class ETrainEnemyType : uint8
{
    Patrol, Runner, Sentry, Gunner, Lockkeeper, Guardian
};

UCLASS()
class ATrainEnemy : public ACharacter
{
    GENERATED_BODY()
public:
    ATrainEnemy();
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    void Hit(class ATrainPawn* Attacker, float Damage);
    void Configure(ETrainEnemyType NewType);
    ETrainEnemyType GetEnemyType() const { return Type; }
    FText DisplayName() const;
    const TCHAR* AnimationFolder() const;
    bool Runs() const { return Type == ETrainEnemyType::Runner; }
    bool IsBoss() const;
    UPROPERTY(Replicated) float Health = 90;
    UPROPERTY(Replicated) float MaxHealth = 90;
    UFUNCTION(NetMulticast, Reliable) void MulticastAttack(FVector Origin, FRotator Direction, float Range, float Width, float Windup);
    UFUNCTION(NetMulticast,Reliable) void MulticastHit(float Damage,bool Dead);
private:
    UPROPERTY() TObjectPtr<class UWidgetComponent> HealthBar;
    float AttackCooldown = 0;
    float RepathAfter=0;
    float StaggerTime=0;
    FTimerHandle HitTimer;
    UFUNCTION() void ApplyAppearance();
    UPROPERTY(ReplicatedUsing=ApplyAppearance) ETrainEnemyType Type = ETrainEnemyType::Patrol;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> AttackMarker;
    FTimerHandle MarkerTimer;
    FTimerHandle FlashTimer;
    int32 AttackCount = 0;
};

UCLASS()
class ATrainStation : public AStaticMeshActor
{
    GENERATED_BODY()
public:
    ATrainStation();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    UPROPERTY(ReplicatedUsing=UpdateLabel) bool Crew=false;
    // Character, arsenal, passives, departure, crew. Kept with replicated session actors.
    UPROPERTY(EditAnywhere,ReplicatedUsing=UpdateLabel) int32 Kind=3;
    static FText Title(int32 Kind);
    UFUNCTION() void UpdateLabel();
private:
    UPROPERTY() TObjectPtr<class UWidgetComponent> Prompt;
};

UCLASS()
class ATrainDummy : public AStaticMeshActor
{
    GENERATED_BODY()
public:
    ATrainDummy();
    void Hit(float Damage);
    UFUNCTION(NetMulticast,Unreliable) void MulticastHit(float Damage);
private:
    UPROPERTY() TObjectPtr<class UWidgetComponent> Label;
};

UCLASS()
class UTrainEnemyHealth : public UUserWidget
{
    GENERATED_BODY()
public:
    TWeakObjectPtr<ATrainEnemy> Enemy;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    FSlateBrush Background,Fill;
};

UCLASS()
class UTrainHUD : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    UPROPERTY() TObjectPtr<UTexture2D> Portrait;
    UPROPERTY() TObjectPtr<UTexture2D> ScoutPortrait;
    FSlateBrush PortraitBrush;
    FSlateBrush ScoutPortraitBrush;
};
