#include "TrainNetwork.h"
#include "../character/TrainAnimInstance.h"
#include "../progression/TrainProfile.h"
#include "../ui/TrainDamageNumber.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/GameViewportClient.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Animation/SkeletalMeshActor.h"
#include "Animation/AnimSequence.h"
#include "Widgets/Images/SImage.h"
#include "TimerManager.h"
#include "AIController.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "NavigationSystem.h"
#include "TrainSession.h"
#include "../TrainSettings.h"
#include "Sound/SoundBase.h"
#include "../TrainPrototype.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "../ui/TrainWorldPrompt.h"
#include "../ui/TrainInputHints.h"
#include "CommonInputSubsystem.h"
#include "InputKeyEventArgs.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Materials/MaterialInterface.h"

ATrainPlayerState::ATrainPlayerState() { SetShouldUpdateReplicatedPing(true); SetNetUpdateFrequency(20); }
void ATrainPlayerState::OnRep_Loadout() { if (auto* Pawn=Cast<ATrainPawn>(GetPawn())) Pawn->ApplyLoadout(); }
void ATrainPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ATrainPlayerState, MaxHealth);DOREPLIFETIME(ATrainPlayerState,Character);DOREPLIFETIME(ATrainPlayerState,Weapon);
    DOREPLIFETIME(ATrainPlayerState,HealthUpgrade);DOREPLIFETIME(ATrainPlayerState,SpeedUpgrade);
    DOREPLIFETIME(ATrainPlayerState, IsHost); DOREPLIFETIME(ATrainPlayerState, Health); DOREPLIFETIME(ATrainPlayerState, DamageDealt);
    DOREPLIFETIME(ATrainPlayerState, DamageTaken); DOREPLIFETIME(ATrainPlayerState, Kills); DOREPLIFETIME(ATrainPlayerState, DashReadyAt);
}
void ATrainPlayerState::CopyProperties(APlayerState* NewState)
{
    Super::CopyProperties(NewState);
    if (auto* Target = Cast<ATrainPlayerState>(NewState))
    { Target->IsHost=IsHost; Target->Health = Health; Target->DamageDealt = DamageDealt; Target->DamageTaken = DamageTaken; Target->Kills = Kills;Target->MaxHealth=MaxHealth;Target->Character=Character;Target->Weapon=Weapon;Target->HealthUpgrade=HealthUpgrade;Target->SpeedUpgrade=SpeedUpgrade; }
}
void ATrainGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ATrainGameState,RoomIndex);
    DOREPLIFETIME(ATrainGameState, RemainingEnemies); DOREPLIFETIME(ATrainGameState, RoomsCleared);
    DOREPLIFETIME(ATrainGameState, DepartureAt); DOREPLIFETIME(ATrainGameState, RunFinished);
    DOREPLIFETIME(ATrainGameState, Victory); DOREPLIFETIME(ATrainGameState, DeparturePoint);
}
bool ATrainGameState::CanFallenHostDepart(const ATrainPlayerState* Host) const
{
    if (!Host || !Host->IsHost || Host->Health > 0 || RoomIndex == 0 || RemainingEnemies > 0 || RunFinished)
        return false;

    for (const APlayerState* Player : PlayerArray)
    {
        const auto* Survivor = Cast<ATrainPlayerState>(Player);
        const APawn* Pawn = Survivor ? Survivor->GetPawn() : nullptr;
        if (Pawn && Survivor->Health > 0 && FVector::Dist2D(Pawn->GetActorLocation(), DeparturePoint) <= ExitRadius)
            return true;
    }
    return false;
}

ATrainPlayerController::ATrainPlayerController() { bShouldPerformFullTickWhenPaused = true; }
bool ATrainPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
    // Common Input tracks Slate events, including mouse movement and menu buttons.
    // Also report gameplay analog motion, without treating stick drift/zero as use.
    if (auto* Input=UCommonInputSubsystem::Get(GetLocalPlayer()); Input &&
        (Params.Event==IE_Pressed || (Params.Event==IE_Axis && FMath::Abs(Params.AmountDepressed)>(Params.IsGamepad() ? .24f : 0.f))))
        Input->SetCurrentInputType(Params.IsGamepad() ? ECommonInputType::Gamepad : ECommonInputType::MouseAndKeyboard);
    return Super::InputKey(Params);
}
void ATrainPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority()) if (auto* PS=GetPlayerState<ATrainPlayerState>()) PS->IsHost=IsLocalController();
    SetupLocalWorld();
}
void ATrainPlayerController::SetupLocalWorld()
{
    if (!IsLocalController()) return;
    ActiveMap=UGameplayStatics::GetCurrentLevelName(this);
    const bool IsMenu=ActiveMap==TEXT("Menu");
    UTrainSettings::Get()->ApplyRuntime(GetWorld(),IsMenu);
    const TCHAR* Tracks[]={IsMenu ? TEXT("depot_music") : TEXT("wagon_music"),IsMenu ? TEXT("depot_air") : TEXT("wagon_air")};
    for (const TCHAR* Track:Tracks)
        if (auto* Sound=LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("/Game/Audio/%s.%s"),Track,Track))) UGameplayStatics::SpawnSound2D(this,Sound);
    if (HUD) { HUD->RemoveFromParent(); HUD=nullptr; }
    if (Summary) { Summary->RemoveFromParent(); Summary=nullptr; }
    // The level's static preview enemies are replaced by authoritative characters.
    for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
        if (It->ActorHasTag(TEXT("Automate"))) { It->SetActorHiddenInGame(true); It->SetActorEnableCollision(false); }
    if (UGameplayStatics::GetCurrentLevelName(this) == TEXT("Menu"))
    {
        auto* Menu = CreateWidget<UTrainMenu>(this);
        Menu->AddToViewport(10); bShowMouseCursor = true;
        FInputModeUIOnly Mode; Mode.SetWidgetToFocus(Menu->TakeWidget()); SetInputMode(Mode);
    }
    else
    {
        HUD = CreateWidget<UTrainHUD>(this); HUD->AddToViewport();
        bShowMouseCursor = false;
        FInputModeGameOnly Mode; Mode.SetConsumeCaptureMouseDown(false); SetInputMode(Mode);
    }
}
void ATrainPlayerController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!IsLocalController()) return;
    if (ActiveMap != UGameplayStatics::GetCurrentLevelName(this)) SetupLocalWorld();
    const auto* State = GetWorld()->GetGameState<ATrainGameState>();
    if (State && State->RunFinished && !Summary)
    {
        Summary = CreateWidget<UTrainMenu>(this); Summary->SummaryMode = true; Summary->AddToViewport(20);
        UTrainSettings::Get()->ApplyRuntime(GetWorld(),true);
        bShowMouseCursor = true; FInputModeUIOnly Mode; Mode.SetWidgetToFocus(Summary->TakeWidget()); SetInputMode(Mode);
    }
}
void ATrainPlayerController::ClientHurt_Implementation() { if (UTrainSettings::Get()->Vibration) PlayDynamicForceFeedback(.45f,.12f,true,true,true,true); }
void ATrainPlayerController::ClientReward_Implementation(int32 Points)
{ if (auto* Profile=GetGameInstance()->GetSubsystem<UTrainProfile>()) Profile->Reward(Points); }
void ATrainPlayerController::CombatText(FVector Point,float Damage,bool Hurt)
{
    if (!IsLocalController() || !GetLocalPlayer()) return;
    auto* Popup=CreateWidget<UTrainDamageNumber>(this);Popup->WorldPoint=Point;Popup->Damage=Damage;Popup->Hurt=Hurt;
    Popup->AddToViewport(5);
    FVector2D Screen;if (ProjectWorldLocationToScreen(Point,Screen,true))
    { Popup->SetAlignmentInViewport(FVector2D(.5,.5));Popup->SetPositionInViewport(Screen+FVector2D(0,Hurt?22:-16),true); }
}
void ATrainPlayerController::ClientNotice_Implementation(ETrainNotice Notice)
{
    switch (Notice)
    {
    case ETrainNotice::OnlyHostCanPause:
        NoticeText = TrainText(TEXT("Seul l’hôte peut mettre la partie en pause"), TEXT("Only the host can pause the game"));
        break;
    case ETrainNotice::OnlyHostCanDepart:
        NoticeText = TrainText(TEXT("Seul l’hôte peut lancer le départ"), TEXT("Only the host can start departure"));
        break;
    case ETrainNotice::HostLeft:
        if (auto* Session = GetGameInstance<UTrainSession>())
            Session->Leave(TrainText(TEXT("L’hôte a quitté la session."), TEXT("The host left the session.")));
        return;
    }
    NoticeUntil = FPlatformTime::Seconds() + 2.5;
}
FText ATrainPlayerController::Notice() const { return FPlatformTime::Seconds() < NoticeUntil ? NoticeText : FText::GetEmpty(); }
void ATrainPlayerController::ClientReturnToMainMenuWithTextReason_Implementation(const FText& Reason)
{ if (auto* Session = GetGameInstance<UTrainSession>()) Session->Leave(Reason); }

ATrainEnemy::ATrainEnemy()
{
    AIControllerClass=AAIController::StaticClass();AutoPossessAI=EAutoPossessAI::PlacedInWorldOrSpawned;
    bReplicates = true; SetReplicateMovement(true);
    GetCapsuleComponent()->InitCapsuleSize(48,105);
    GetMesh()->SetRelativeLocation(FVector(0,0,-105));
    GetMesh()->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Automaton/SK_Automaton.SK_Automaton")));
    GetMesh()->SetAnimInstanceClass(UTrainAnimInstance::StaticClass());
    GetMesh()->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Characters/Automaton/M_Automaton.M_Automaton")));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    HealthBar=CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBar"));HealthBar->SetupAttachment(GetRootComponent());
    HealthBar->SetRelativeLocation(FVector(0,0,145));HealthBar->SetWidgetSpace(EWidgetSpace::Screen);
    HealthBar->SetDrawAtDesiredSize(true);HealthBar->SetWidgetClass(UTrainEnemyHealth::StaticClass());
    GetCharacterMovement()->bRunPhysicsWithNoController = true;
    GetCharacterMovement()->MaxWalkSpeed = 155;
    GetCharacterMovement()->bOrientRotationToMovement=true;GetCharacterMovement()->RotationRate=FRotator(0,360,0);
    PrimaryActorTick.bCanEverTick = true;
    SetNetUpdateFrequency(30); SetMinNetUpdateFrequency(15);
}
void ATrainEnemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{ Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(ATrainEnemy, Health); }
void ATrainEnemy::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (auto* Widget=Cast<UTrainEnemyHealth>(HealthBar->GetUserWidgetObject())) Widget->Enemy=this;
    const auto* State = GetWorld()->GetGameState<ATrainGameState>();
    if (!HasAuthority() || Health <= 0 || !State || State->RunFinished || UGameplayStatics::IsGamePaused(this)) return;
    ATrainPawn* Target = nullptr; double Distance = TNumericLimits<double>::Max();
    for (TActorIterator<ATrainPawn> It(GetWorld()); It; ++It)
    {
        const auto* PS = It->GetPlayerState<ATrainPlayerState>();
        const double Current = FVector::DistSquared2D(GetActorLocation(), It->GetActorLocation());
        if (PS && PS->Health > 0 && Current < Distance) { Target = *It; Distance = Current; }
    }
    if (!Target) return;
    AttackCooldown-=DeltaSeconds;RepathAfter-=DeltaSeconds;
    if (!GetWorldTimerManager().IsTimerActive(HitTimer))
    {
        if (Distance>FMath::Square(155.0))
        {
            if (RepathAfter<=0)
                if (auto* AI=Cast<AAIController>(GetController())) { AI->MoveToActor(Target,125);RepathAfter=.35f; }
        }
        else if (AttackCooldown<=0)
        {
            if (auto* AI=Cast<AAIController>(GetController())) AI->StopMovement();
            AttackCooldown=1.7f;AttackDirection=(Target->GetActorLocation()-GetActorLocation()).Rotation();
            SetActorRotation(FRotator(0,AttackDirection.Yaw,0));MulticastAttack();
            GetWorldTimerManager().SetTimer(HitTimer,FTimerDelegate::CreateWeakLambda(this,[this]()
            {
                for (TActorIterator<ATrainPawn> It(GetWorld());It;++It)
                {
                    const FVector Delta=It->GetActorLocation()-GetActorLocation();
                    if (Delta.SizeSquared2D()<FMath::Square(185.f) && FVector::DotProduct(Delta.GetSafeNormal2D(),AttackDirection.Vector())>.15f) It->ReceiveHit(12);
                }
            }),.48f,false);
        }
    }

}
void ATrainEnemy::Hit(ATrainPawn* Attacker, float Damage)
{
    auto* State = GetWorld()->GetGameState<ATrainGameState>();
    if (!HasAuthority() || Health <= 0 || !State || State->RunFinished) return;
    const float Actual = FMath::Min(Health, Damage); Health -= Actual;
    MulticastHit(Actual,Health<=0);
    if (auto* PS = Attacker->GetPlayerState<ATrainPlayerState>())
    { PS->DamageDealt += Actual; if (Health <= 0) ++PS->Kills; PS->ForceNetUpdate(); }
    if (Health <= 0)
    {
        GetWorldTimerManager().ClearTimer(HitTimer);
        State->RemainingEnemies = FMath::Max(0, State->RemainingEnemies - 1);
        if (State->RemainingEnemies == 0) ++State->RoomsCleared;
        Destroy();
    }
    else ForceNetUpdate();
}
void ATrainEnemy::MulticastAttack_Implementation()
{
    if (auto* Anim=Cast<UTrainAnimInstance>(GetMesh()->GetAnimInstance())) Anim->Action(TEXT("Attack"),1.05f);
    if (auto* Sound=LoadObject<USoundBase>(nullptr,TEXT("/Game/Audio/enemy_strike.enemy_strike"))) UGameplayStatics::PlaySoundAtLocation(this,Sound,GetActorLocation(),.7f);
}
void ATrainEnemy::MulticastHit_Implementation(float Damage,bool Dead)
{
    if (auto* PC=Cast<ATrainPlayerController>(GetWorld()->GetFirstPlayerController())) PC->CombatText(GetActorLocation()+FVector(0,0,145),Damage,false);
    const TCHAR* Track=Dead?TEXT("enemy_fall"):TEXT("enemy_hit");
    if (auto* Sound=LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("/Game/Audio/%s.%s"),Track,Track))) UGameplayStatics::PlaySoundAtLocation(this,Sound,GetActorLocation(),.8f);
    if (!Dead) { if (auto* Anim=Cast<UTrainAnimInstance>(GetMesh()->GetAnimInstance())) Anim->Action(TEXT("Hit"),.24f);return; }
    FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    // The corpse mesh is its root component, so the living mesh's capsule-relative offset
    // must be converted to a world transform before spawning the cosmetic actor.
    if (auto* Corpse=GetWorld()->SpawnActor<ASkeletalMeshActor>(ASkeletalMeshActor::StaticClass(),GetMesh()->GetComponentTransform(),Params))
    {
        auto* Mesh=Corpse->GetSkeletalMeshComponent();Mesh->SetSkeletalMesh(GetMesh()->GetSkeletalMeshAsset());
        Mesh->SetMaterial(0,GetMesh()->GetMaterial(0));
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->PlayAnimation(LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Automaton/Animations/Death.Death")),false);
        Corpse->SetActorEnableCollision(false);Corpse->SetLifeSpan(4);
    }
}
TSharedRef<SWidget> UTrainHUD::RebuildWidget()
{
    static const FProgressBarStyle Bars=FProgressBarStyle()
        .SetBackgroundImage(FSlateColorBrush(FLinearColor(.035,.045,.05)))
        .SetFillImage(FSlateColorBrush(FLinearColor::White));
    static const FSlateRoundedBoxBrush Card(FLinearColor(.012,.018,.025,.94),6.f,FLinearColor(.48,.28,.13,.9),1.f);
    auto Overlay = SNew(SOverlay);
    auto Team=SNew(SVerticalBox);
    auto Member=[this](int32 Slot)->ATrainPlayerState*
    {
        auto* GS=GetWorld()->GetGameState<ATrainGameState>();
        auto* PC=GetOwningPlayer();
        if (!GS || !PC) return nullptr;
        if (Slot==0) return PC->GetPlayerState<ATrainPlayerState>();
        int32 Other=1;
        for (APlayerState* PS:GS->PlayerArray)
            if (PS!=PC->PlayerState && Other++==Slot) return Cast<ATrainPlayerState>(PS);
        return nullptr;
    };
    Portrait=LoadObject<UTexture2D>(nullptr,TEXT("/Game/UI/Portraits/Conductor.Conductor"));
    PortraitBrush.SetResourceObject(Portrait);PortraitBrush.ImageSize=FVector2D(72,72);
    for (int32 Slot=3;Slot>=0;--Slot)
    {
        const bool Local=Slot==0;
        Team->AddSlot().AutoHeight().Padding(0,3)
            [SNew(SBox).WidthOverride(Local?350:265)
                .Visibility_Lambda([Member,Slot](){return Member(Slot)?EVisibility::HitTestInvisible:EVisibility::Collapsed;})
                [SNew(SBorder).Padding(Local?10:7).BorderImage(&Card)
                    [SNew(SHorizontalBox)
                        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,10,0)
                            [SNew(SBox).WidthOverride(Local?66:42).HeightOverride(Local?66:42)
                                [SNew(SImage).Image(&PortraitBrush).ColorAndOpacity_Lambda([Member,Slot]()
                                    {auto* PS=Member(Slot);return PS&&PS->Character?FLinearColor(.68,.86,1):FLinearColor::White;})]]
                        +SHorizontalBox::Slot().FillWidth(1)
                            [SNew(SVerticalBox)
                                +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,4)
                                    [SNew(STextBlock).ColorAndOpacity(FLinearColor(.94,.84,.67)).Font(FCoreStyle::GetDefaultFontStyle("Bold",Local?16:12))
                                        .Text_Lambda([this,Member,Slot](){auto* PS=Member(Slot);
                                            if (!PS) return FText::GetEmpty();
                                            if (GetWorld()->GetNetMode()==NM_Standalone)
                                                return PS->Character?TrainText(TEXT("L’Éclaireur"),TEXT("The Scout")):TrainText(TEXT("Le Conducteur"),TEXT("The Conductor"));
                                            return FText::FromString(PS->GetPlayerName());})]
                                +SVerticalBox::Slot().AutoHeight()
                                    [SNew(SBox).HeightOverride(Local?12:8)
                                        [SNew(SProgressBar).Style(&Bars).FillColorAndOpacity(FLinearColor(.65,.16,.13))
                                            .Percent_Lambda([Member,Slot]()->TOptional<float>{auto* PS=Member(Slot);return PS?PS->Health/FMath::Max(1.f,PS->MaxHealth):0;})]]
                                +SVerticalBox::Slot().AutoHeight().Padding(0,4,0,0)
                                    [SNew(SBox).HeightOverride(5).Visibility(Local?EVisibility::HitTestInvisible:EVisibility::Collapsed)
                                        [SNew(SProgressBar).Style(&Bars).FillColorAndOpacity(FLinearColor(.35,.78,.76))
                                            .Percent_Lambda([this,Member,Slot]()->TOptional<float>{auto* PS=Member(Slot);auto* GS=GetWorld()->GetGameState<ATrainGameState>();return PS&&GS?FMath::Clamp(float(1-(PS->DashReadyAt-GS->GetServerWorldTimeSeconds())/.65),0.f,1.f):1.f;})]]
                                +SVerticalBox::Slot().AutoHeight().Padding(0,4,0,0)
                                    [SNew(STextBlock).ColorAndOpacity(FLinearColor(.8,.77,.7)).Font(FCoreStyle::GetDefaultFontStyle("Regular",Local?11:9))
                                        .Text_Lambda([Member,Slot](){auto* PS=Member(Slot);return PS?FText::FromString(FString::Printf(TEXT("%.0f / %.0f"),PS->Health,PS->MaxHealth)):FText::GetEmpty();})]]]]];
    }
    Overlay->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(28)[Team];
    Overlay->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(24)
        [SNew(STextBlock).ColorAndOpacity(FLinearColor(.94,.84,.67)).Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
            .Text_Lambda([this]()
            {
                const auto* PC = Cast<ATrainPlayerController>(GetOwningPlayer());
                if (PC && !PC->Notice().IsEmpty()) return PC->Notice();
                const auto* GS = GetWorld()->GetGameState<ATrainGameState>();
                if (UGameplayStatics::IsGamePaused(this)) return TrainText(TEXT("L’hôte a mis la partie en pause"),TEXT("The host paused the game"));
                if (GS && GS->DepartureAt > 0)
                    return FText::Format(TrainText(TEXT("Rassemblez-vous à la sortie · Départ dans {0} s"),TEXT("Gather at the exit · Departing in {0} s")),
                        FText::AsNumber(FMath::Max(0, FMath::CeilToInt(GS->DepartureAt - GS->GetServerWorldTimeSeconds()))));
                return FText::GetEmpty();
            })];
    Overlay->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(24, 60)
        [SNew(SHorizontalBox)
            .Visibility_Lambda([this,Member]()
            {
                const auto* GS = GetWorld()->GetGameState<ATrainGameState>();
                const auto* Pawn = Cast<ATrainPawn>(GetOwningPlayerPawn());
                return GS && GS->DepartureAt <= 0 && Pawn && !Pawn->IsMenuOpen() && GS->CanFallenHostDepart(Member(0))
                    ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
            })
            + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 10, 0)
                [SNew(STrainInputGlyph).Size(26).Key_Lambda([this]() { return TrainInput::ActionKey(this, TEXT("Interact")); })]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [SNew(STextBlock).ColorAndOpacity(FLinearColor(.94,.84,.67)).Text_Lambda([]()
                {
                    return TrainText(TEXT("Autoriser le départ"), TEXT("Authorize departure"));
                })]];
    Overlay->AddSlot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(28,60)
        [SNew(STextBlock).ColorAndOpacity(FLinearColor(.94,.84,.67)).Font(FCoreStyle::GetDefaultFontStyle("Bold",15))
            .Text_Lambda([this](){auto* GS=GetWorld()->GetGameState<ATrainGameState>();
                if (!GS || GS->RoomIndex==0) return TrainText(TEXT("WAGON-ATELIER"),TEXT("WORKSHOP CAR"));
                return FText::Format(TrainText(TEXT("Salle {0} / 2 · {1} automates"),TEXT("Room {0} / 2 · {1} automatons")),FText::AsNumber(GS->RoomIndex),FText::AsNumber(GS->RemainingEnemies));})];
    return SNew(SBox).HAlign(HAlign_Center)
        [SNew(SBox).WidthOverride_Lambda([this](){auto* V=GetWorld()->GetGameViewport();FVector2D S;V->GetViewportSize(S);return FMath::Min(S.X,S.Y*21.f/9.f)/UWidgetLayoutLibrary::GetViewportScale(this);})[Overlay]];
}

ATrainStation::ATrainStation()
{
    bReplicates=true; SetReplicateMovement(true); PrimaryActorTick.bCanEverTick=true;
    auto* Mesh=GetStaticMeshComponent(); Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Props/Departure.Departure")));
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    auto* Footprint=CreateDefaultSubobject<UBoxComponent>(TEXT("FurnitureCollision"));
    Footprint->SetupAttachment(GetRootComponent());Footprint->SetRelativeLocation(FVector(0,0,85));
    Footprint->SetBoxExtent(FVector(85,40,85));Footprint->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Prompt=CreateDefaultSubobject<UWidgetComponent>(TEXT("Prompt")); Prompt->SetupAttachment(GetRootComponent());
    Prompt->SetAbsolute(false,true,true); Prompt->SetRelativeLocation(FVector(0,0,205));
    Prompt->SetWidgetSpace(EWidgetSpace::Screen); Prompt->SetDrawAtDesiredSize(true); Prompt->SetPivot(FVector2D(.5,1));
    Prompt->SetWidgetClass(UTrainWorldPrompt::StaticClass());

}
void ATrainStation::BeginPlay() { Super::BeginPlay(); UpdateLabel(); }
void ATrainStation::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    auto* PC=GetWorld()->GetFirstPlayerController(); auto* Pawn=PC ? Cast<ATrainPawn>(PC->GetPawn()) : nullptr;
    const auto* GS=GetWorld()->GetGameState<ATrainGameState>();
    const auto* PS=Pawn ? Pawn->GetPlayerState<ATrainPlayerState>() : nullptr;
    const bool Show=PC && PC->GetLocalPlayer() && Pawn && PS && PS->Health>0 && !Pawn->IsMenuOpen()
        && !UGameplayStatics::IsGamePaused(this) && GS && !GS->RunFinished && (Kind!=3 || GS->RemainingEnemies==0)
        && FVector::Dist2D(Pawn->GetActorLocation(),GetActorLocation())<(Kind==3 ? 250:230);
    Prompt->SetVisibility(Show);
    if (auto* Widget=Cast<UTrainWorldPrompt>(Prompt->GetUserWidgetObject()))
    { Widget->Station=this; if (PC && PC->GetLocalPlayer() && Widget->GetOwningPlayer()!=PC) Widget->SetOwningPlayer(PC); }
}

void ATrainStation::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{ Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(ATrainStation,Crew);DOREPLIFETIME(ATrainStation,Kind); }
void ATrainStation::UpdateLabel()
{
    if (auto* Widget=Cast<UTrainWorldPrompt>(Prompt->GetUserWidgetObject())) Widget->Station=this;
}

FText ATrainStation::Title(int32 Kind)
{
    const TCHAR* FR[]={TEXT("Personnages"),TEXT("Arsenal"),TEXT("Compétences passives"),TEXT("Départ"),TEXT("Équipage")};
    const TCHAR* EN[]={TEXT("Characters"),TEXT("Arsenal"),TEXT("Passive skills"),TEXT("Departure"),TEXT("Crew")};
    return TrainText(FR[FMath::Clamp(Kind,0,4)],EN[FMath::Clamp(Kind,0,4)]);
}
ATrainDummy::ATrainDummy()
{
    bReplicates=true;
    GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
    GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Props/TrainingDummy.TrainingDummy")));
    GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    auto* Body = CreateDefaultSubobject<UCapsuleComponent>(TEXT("PracticeTargetBody"));
    Body->SetupAttachment(GetRootComponent());
    Body->InitCapsuleSize(38.f, 77.f);
    Body->SetRelativeLocation(FVector(0, 0, 77.f));
    Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Body->SetCollisionResponseToAllChannels(ECR_Ignore);
    Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    Body->SetGenerateOverlapEvents(false);
    Body->SetCanEverAffectNavigation(false);
}
void ATrainDummy::Hit(float Damage) { if (HasAuthority()) MulticastHit(Damage); }
void ATrainDummy::MulticastHit_Implementation(float Damage)
{
    if (auto* PC=Cast<ATrainPlayerController>(GetWorld()->GetFirstPlayerController())) PC->CombatText(GetActorLocation()+FVector(0,0,165),Damage,false);
    if (auto* Sound=LoadObject<USoundBase>(nullptr,TEXT("/Game/Audio/dummy_hit.dummy_hit"))) UGameplayStatics::PlaySoundAtLocation(this,Sound,GetActorLocation(),.8f);
}
TSharedRef<SWidget> UTrainEnemyHealth::RebuildWidget()
{
    static const FProgressBarStyle Bars=FProgressBarStyle()
        .SetBackgroundImage(FSlateColorBrush(FLinearColor(.02,.025,.03)))
        .SetFillImage(FSlateColorBrush(FLinearColor::White));
    return SNew(SBox).WidthOverride(82).HeightOverride(5)
        [SNew(SProgressBar).Style(&Bars).FillColorAndOpacity(FLinearColor(.76,.2,.13))
            .Percent_Lambda([this]()->TOptional<float>{return Enemy.IsValid()?FMath::Clamp(Enemy->Health/90.f,0.f,1.f):0.f;})];
}
