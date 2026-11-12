#include "TrainNetwork.h"
#include "../TrainRooms.h"
#include "TrainDash.h"
#include "../character/TrainAnimInstance.h"
#include "../character/TrainCombatEffect.h"
#include "../progression/TrainProfile.h"
#include "../ui/TrainDamageNumber.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
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
    DOREPLIFETIME(ATrainGameState,RoomReady);DOREPLIFETIME(ATrainGameState,RoomSeed);
    DOREPLIFETIME(ATrainGameState,EncounterThreat);DOREPLIFETIME(ATrainGameState,LayoutProps);
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
    MenuAudio.Reset();
    for (const TCHAR* Track:Tracks)
        if (auto* Sound=LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("/Game/Audio/%s.%s"),Track,Track)))
            if (auto* Audio=UGameplayStatics::SpawnSound2D(this,Sound); IsMenu && Audio)
                MenuAudio.Add(Audio);
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
void ATrainPlayerController::StopMenuAudio()
{
    for (const auto& Audio : MenuAudio)
        if (Audio.IsValid())
            Audio->Stop();
    MenuAudio.Reset();
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

namespace
{
    struct FEnemyPattern
    {
        float Health, Speed, Reach, Damage, Windup, Recovery, Size;
    };

    const FEnemyPattern& EnemyPattern(ETrainEnemyType Type)
    {
        static constexpr FEnemyPattern Patterns[] = {
            {90, 235, 185, 12, .46f, .90f, 1.f},
            {48, 360, 165, 8, .32f, .67f, .76f},
            {125, 170, 225, 17, .68f, 1.15f, 1.12f},
            {65, 150, 620, 11, .92f, 1.50f, .92f},
            {310, 142, 305, 22, 1.10f, 1.34f, 1.32f},
            {470, 133, 350, 26, 1.24f, 1.43f, 1.55f},
        };
        return Patterns[static_cast<uint8>(Type)];
    }
}

ATrainEnemy::ATrainEnemy()
{
    AIControllerClass=AAIController::StaticClass();AutoPossessAI=EAutoPossessAI::PlacedInWorldOrSpawned;
    bReplicates = true; SetReplicateMovement(true);
    GetCapsuleComponent()->InitCapsuleSize(48,105);
    GetMesh()->SetRelativeLocation(FVector(0,0,-105));
    // The automaton's source skeleton faces +Y. Character Movement and the hit arc face +X.
    GetMesh()->SetRelativeRotation(FRotator(0,-90,0));
    GetMesh()->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Automaton/SK_Automaton.SK_Automaton")));
    GetMesh()->SetAnimInstanceClass(UTrainAnimInstance::StaticClass());
    GetMesh()->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Characters/Automaton/M_Automaton.M_Automaton")));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    AttackMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AttackMarker"));
    AttackMarker->SetupAttachment(GetRootComponent());
    AttackMarker->SetAbsolute(true, true, true);
    AttackMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AttackMarker->SetCanEverAffectNavigation(false);
    AttackMarker->SetCastShadow(false);
    AttackMarker->SetVisibility(false);
    AttackMarker->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Effects/M_Telegraph.M_Telegraph")));
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
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ATrainEnemy, Health);
    DOREPLIFETIME(ATrainEnemy, MaxHealth);
    DOREPLIFETIME(ATrainEnemy, Type);
}

void ATrainEnemy::Configure(ETrainEnemyType NewType)
{
    Type = NewType;
    MaxHealth = EnemyPattern(Type).Health;
    Health = MaxHealth;
    ApplyAppearance();
    ForceNetUpdate();
}

bool ATrainEnemy::IsBoss() const
{
    return Type == ETrainEnemyType::Lockkeeper || Type == ETrainEnemyType::Guardian;
}

FText ATrainEnemy::DisplayName() const
{
    switch (Type)
    {
    case ETrainEnemyType::Lockkeeper: return TrainText(TEXT("L’Éclusier"), TEXT("The Lockkeeper"));
    case ETrainEnemyType::Guardian: return TrainText(TEXT("Gardien de la station"), TEXT("Station Guardian"));
    default: return FText::GetEmpty();
    }
}

const TCHAR* ATrainEnemy::AnimationFolder() const
{
    if (Type == ETrainEnemyType::Guardian) return TEXT("Faubourgs/GuardianRig");
    if (Type == ETrainEnemyType::Sentry || Type == ETrainEnemyType::Lockkeeper) return TEXT("Faubourgs/SentryRig");
    return TEXT("Automaton");
}

void ATrainEnemy::ApplyAppearance()
{
    const auto& Pattern = EnemyPattern(Type);
    GetCharacterMovement()->MaxWalkSpeed = Pattern.Speed;
    const FString Folder = AnimationFolder();
    const FString Name = FPaths::GetCleanFilename(Folder);
    const FString MeshPath = FString::Printf(TEXT("/Game/Characters/%s/SK_%s.SK_%s"), *Folder, *Name, *Name);
    if (auto* Variant = LoadObject<USkeletalMesh>(nullptr, *MeshPath); Variant && Variant != GetMesh()->GetSkeletalMeshAsset())
    {
        GetMesh()->EmptyOverrideMaterials();
        GetMesh()->SetSkeletalMesh(Variant);
    }
    const bool Imported = Folder != TEXT("Automaton") && GetMesh()->GetSkeletalMeshAsset()
        && GetMesh()->GetSkeletalMeshAsset()->GetName() == FString(TEXT("SK_")) + Name;
    if (Imported)
    {
        // The Meshy GLB rig can retain a .01 armature scale after Interchange import.
        const FBoxSphereBounds Bounds = GetMesh()->GetSkeletalMeshAsset()->GetBounds();
        const float Height = 205.f * Pattern.Size;
        const float MeshScale = Height / FMath::Max(1.f, float(Bounds.BoxExtent.Z * 2.0));
        GetMesh()->SetRelativeScale3D(FVector(MeshScale));
        GetMesh()->SetRelativeLocation(FVector(0, 0, -105.f - (Bounds.Origin.Z - Bounds.BoxExtent.Z) * MeshScale));
        HealthBar->SetRelativeLocation(FVector(0, 0, Height - 92.f));
    }
    else
    {
        GetMesh()->SetRelativeScale3D(FVector(Pattern.Size));
        HealthBar->SetRelativeLocation(FVector(0, 0, 105 + 40 * Pattern.Size));
    }
    GetMesh()->InitAnim(true);
}

void ATrainEnemy::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (auto* Widget = Cast<UTrainEnemyHealth>(HealthBar->GetUserWidgetObject()))
        Widget->Enemy = this;
    const auto* State = GetWorld()->GetGameState<ATrainGameState>();
    if (!HasAuthority() || Health <= 0 || !State || !State->RoomReady || State->RunFinished || UGameplayStatics::IsGamePaused(this))
        return;

    StaggerTime = FMath::Max(0.f,StaggerTime-DeltaSeconds);
    if (StaggerTime>0) return;
    ATrainPawn* Target = nullptr;
    double Distance = TNumericLimits<double>::Max();
    for (TActorIterator<ATrainPawn> It(GetWorld()); It; ++It)
    {
        const auto* PS = It->GetPlayerState<ATrainPlayerState>();
        const double Current = FVector::DistSquared2D(GetActorLocation(), It->GetActorLocation());
        if (PS && PS->Health > 0 && Current < Distance)
        {
            Target = *It;
            Distance = Current;
        }
    }
    if (!Target || GetWorldTimerManager().IsTimerActive(HitTimer))
        return;
    if (Distance > FMath::Square(1220.f))
    {
        if (auto* AI = Cast<AAIController>(GetController()); AI && GetVelocity().SizeSquared2D() > 1.f)
            AI->StopMovement();
        return;
    }

    const auto& Pattern = EnemyPattern(Type);
    const FVector Origin = GetActorLocation();
    AttackCooldown -= DeltaSeconds;
    RepathAfter -= DeltaSeconds;
    auto* AI = Cast<AAIController>(GetController());
    FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(TrainEnemySight), false, this);
    TraceParams.AddIgnoredActor(Target);
    const bool Blocked = GetWorld()->LineTraceTestByChannel(Origin, Target->GetActorLocation(), ECC_Visibility, TraceParams);
    if (Distance > FMath::Square(Pattern.Reach * .83f) || Blocked)
    {
        if (AI && RepathAfter <= 0)
        {
            AI->MoveToActor(Target, Pattern.Reach * .5f, false);
            RepathAfter = .35f;
        }
        return;
    }
    if (AttackCooldown > 0)
        return;

    if (AI) AI->StopMovement();
    const FRotator Direction(0, (Target->GetActorLocation() - Origin).Rotation().Yaw, 0);
    SetActorRotation(Direction);
    const bool Enraged = IsBoss() && Health <= MaxHealth * .5f;
    const bool Radial = Type == ETrainEnemyType::Guardian
        ? (++AttackCount % (Enraged ? 2 : 3) == 0)
        : Type == ETrainEnemyType::Lockkeeper && (++AttackCount % 2 == 0);
    const float Range = Radial && Enraged ? Pattern.Reach * 1.12f : Pattern.Reach;
    const float Width = Radial ? 0.f : Type == ETrainEnemyType::Gunner ? 85.f : Range * .82f;
    const float Windup = Pattern.Windup * (Enraged ? .82f : 1.f);
    const float Damage = Pattern.Damage;
    AttackCooldown = Pattern.Recovery * (Enraged ? .75f : 1.f);
    MulticastAttack(Origin, Direction, Range, Width, Windup);
    GetWorldTimerManager().SetTimer(HitTimer, FTimerDelegate::CreateWeakLambda(this, [this, Origin, Direction, Range, Width, Damage]()
    {
        if (auto* GS = GetWorld()->GetGameState<ATrainGameState>(); !GS || GS->RunFinished)
            return;
        for (TActorIterator<ATrainPawn> It(GetWorld()); It; ++It)
        {
            const FVector Delta = Direction.UnrotateVector(It->GetActorLocation() - Origin);
            const bool InArea = Width == 0 ? Delta.SizeSquared2D() <= FMath::Square(Range)
                : Delta.X >= 0 && Delta.X <= Range && FMath::Abs(Delta.Y) <= Width * .5f;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(TrainEnemyAttack), false, this);
            Query.AddIgnoredActor(*It);
            if (InArea && !GetWorld()->LineTraceTestByChannel(Origin, It->GetActorLocation(), ECC_Visibility, Query))
                It->ReceiveHit(Damage);
        }
    }), Windup, false);
}

void ATrainEnemy::Hit(ATrainPawn* Attacker, float Damage)
{
    auto* State = GetWorld()->GetGameState<ATrainGameState>();
    if (!HasAuthority() || Health <= 0 || !State || State->RunFinished) return;
    const float Actual = FMath::Min(Health, Damage); Health -= Actual;
    if (!IsBoss() && Health > 0)
    {
        GetWorldTimerManager().ClearTimer(HitTimer);
        AttackCooldown = FMath::Max(AttackCooldown, .30f);
        StaggerTime = Damage>=60?.26f:.16f;
        if (auto* AI = Cast<AAIController>(GetController())) AI->StopMovement();
        const FVector Away = (GetActorLocation()-Attacker->GetActorLocation()).GetSafeNormal2D();
        GetCharacterMovement()->AddImpulse(Away * (Damage>=60?250.f:125.f), true);
    }
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
void ATrainEnemy::MulticastAttack_Implementation(FVector Origin, FRotator Direction, float Range, float Width, float Windup)
{
    if (auto* Anim = Cast<UTrainAnimInstance>(GetMesh()->GetAnimInstance()))
        Anim->Action(TEXT("Attack"), Windup * 2.f);
    if (!AttackMarker->GetMaterial(0))
        return;

    const bool Radial = Width == 0;
    if (auto* Material = AttackMarker->CreateDynamicMaterialInstance(0))
    {
        Material->SetScalarParameterValue(TEXT("Radial"),Radial?1.f:0.f);
        Material->SetScalarParameterValue(TEXT("StartedAt"),GetWorld()->GetTimeSeconds());
        Material->SetScalarParameterValue(TEXT("Windup"),Windup);
    }
    FVector Marker = Origin - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 3.f);
    if (!Radial) Marker += Direction.Vector() * (Range * .5f);
    AttackMarker->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Radial
        ? TEXT("/Engine/BasicShapes/Cylinder.Cylinder") : TEXT("/Engine/BasicShapes/Cube.Cube")));
    AttackMarker->SetWorldLocationAndRotation(Marker, Direction);
    AttackMarker->SetWorldScale3D(Radial ? FVector(Range / 50.f, Range / 50.f, .012f)
        : FVector(Range / 100.f, Width / 100.f, .012f));
    AttackMarker->SetVisibility(true);
    GetWorldTimerManager().SetTimer(MarkerTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        AttackMarker->SetVisibility(false);
        if (auto* Sound=LoadObject<USoundBase>(nullptr,TEXT("/Game/Audio/enemy_strike.enemy_strike")))
            UGameplayStatics::PlaySoundAtLocation(this,Sound,GetActorLocation(),.65f);
    }), Windup, false);
}
void ATrainEnemy::MulticastHit_Implementation(float Damage,bool Dead)
{
    if (auto* PC=Cast<ATrainPlayerController>(GetWorld()->GetFirstPlayerController())) PC->CombatText(GetActorLocation()+FVector(0,0,145),Damage,false);
    const TCHAR* Track=Dead?TEXT("enemy_fall"):TEXT("enemy_hit");
    if (auto* Sound=LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("/Game/Audio/%s.%s"),Track,Track))) UGameplayStatics::PlaySoundAtLocation(this,Sound,GetActorLocation(),.8f);
    ATrainCombatEffect::Spawn(GetWorld(),GetActorLocation(),GetActorRotation(),true,Damage>=60);
    GetMesh()->SetOverlayMaterial(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Effects/M_HitFlash.M_HitFlash")));
    GetWorldTimerManager().SetTimer(FlashTimer,FTimerDelegate::CreateWeakLambda(this,[this](){GetMesh()->SetOverlayMaterial(nullptr);}),.075f,false);
    if (!IsBoss() || Dead)
    {
        GetWorldTimerManager().ClearTimer(MarkerTimer);
        AttackMarker->SetVisibility(false);
    }
    if (!Dead)
    {
        if (!IsBoss())
            if (auto* Anim=Cast<UTrainAnimInstance>(GetMesh()->GetAnimInstance())) Anim->Action(TEXT("Hit"),.18f);
        return;
    }
    FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    // The corpse mesh is its root component, so the living mesh's capsule-relative offset
    // must be converted to a world transform before spawning the cosmetic actor.
    if (auto* Corpse=GetWorld()->SpawnActor<ASkeletalMeshActor>(ASkeletalMeshActor::StaticClass(),GetMesh()->GetComponentTransform(),Params))
    {
        auto* Mesh=Corpse->GetSkeletalMeshComponent();Mesh->SetSkeletalMesh(GetMesh()->GetSkeletalMeshAsset());
        Mesh->SetMaterial(0,GetMesh()->GetMaterial(0));
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        const FString DeathPath = FString::Printf(TEXT("/Game/Characters/%s/Animations/Death.Death"), AnimationFolder());
        Mesh->PlayAnimation(LoadObject<UAnimSequence>(nullptr, *DeathPath), false);
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
    ScoutPortrait=LoadObject<UTexture2D>(nullptr,TEXT("/Game/UI/Portraits/Scout.Scout"));
    ScoutPortraitBrush.SetResourceObject(ScoutPortrait);ScoutPortraitBrush.ImageSize=FVector2D(72,72);
    ScoutPortraitBrush.SetUVRegion(FBox2D(FVector2D(.26,0),FVector2D(.74,.32)));
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
                                [SNew(SImage).Image_Lambda([this,Member,Slot]()->const FSlateBrush*
                                    {auto* PS=Member(Slot);return PS&&PS->Character?&ScoutPortraitBrush:&PortraitBrush;})]]
                        +SHorizontalBox::Slot().FillWidth(1)
                            [SNew(SVerticalBox)
                                +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,4)
                                    [SNew(STextBlock).ColorAndOpacity(FLinearColor(.94,.84,.67)).Font(FCoreStyle::GetDefaultFontStyle("Bold",Local?16:12))
                                        .Text_Lambda([this,Member,Slot](){auto* PS=Member(Slot);
                                            if (!PS) return FText::GetEmpty();
                                            if (GetWorld()->GetNetMode()==NM_Standalone)
                                                return PS->Character?TrainText(TEXT("L’Éclaireuse"),TEXT("The Scout")):TrainText(TEXT("Le Conducteur"),TEXT("The Conductor"));
                                            return FText::FromString(PS->GetPlayerName());})]
                                +SVerticalBox::Slot().AutoHeight()
                                    [SNew(SBox).HeightOverride(Local?12:8)
                                        [SNew(SProgressBar).Style(&Bars).FillColorAndOpacity(FLinearColor(.65,.16,.13))
                                            .Percent_Lambda([Member,Slot]()->TOptional<float>{auto* PS=Member(Slot);return PS?PS->Health/FMath::Max(1.f,PS->MaxHealth):0;})]]
                                +SVerticalBox::Slot().AutoHeight().Padding(0,4,0,0)
                                    [SNew(SBox).HeightOverride(5).Visibility(Local?EVisibility::HitTestInvisible:EVisibility::Collapsed)
                                        [SNew(SProgressBar).Style(&Bars).FillColorAndOpacity(FLinearColor(.35,.78,.76))
                                            .Percent_Lambda([this,Member,Slot]()->TOptional<float>{auto* PS=Member(Slot);auto* GS=GetWorld()->GetGameState<ATrainGameState>();return PS&&GS?FMath::Clamp(float(1-(PS->DashReadyAt-GS->GetServerWorldTimeSeconds())/UTrainDash::CooldownDuration),0.f,1.f):1.f;})]]
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
                if (!GS->RoomReady) return TrainText(TEXT("Préparation de la salle…"),TEXT("Preparing room…"));
                const auto& Room = TrainRooms::Faubourgs[GS->RoomIndex - 1];
                return FText::Format(TrainText(TEXT("{0} · {1}/{2} · Automates : {3}"), TEXT("{0} · {1}/{2} · Automatons: {3}")),
                    TrainText(Room.French, Room.English), FText::AsNumber(GS->RoomIndex),
                    FText::AsNumber(TrainRooms::Count), FText::AsNumber(GS->RemainingEnemies));})];
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
void ATrainStation::BeginPlay()
{
    Super::BeginPlay();
    // Imported furniture has its own origin and scale. Anchor interaction and collision
    // to its actual bounds instead of scaling the old placeholder's child components.
    const FBoxSphereBounds Bounds = GetStaticMeshComponent()->Bounds;
    if (auto* Footprint = FindComponentByClass<UBoxComponent>())
    {
        Footprint->SetAbsolute(false,true,true);
        Footprint->SetWorldLocation(Bounds.Origin);
        Footprint->SetBoxExtent(FVector(Bounds.BoxExtent.X*.82f,Bounds.BoxExtent.Y*.82f,FMath::Max(20.f,float(Bounds.BoxExtent.Z))));
    }
    Prompt->SetWorldLocation(Bounds.Origin+FVector(0,0,Bounds.BoxExtent.Z+22));
    UpdateLabel();
}
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
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
        [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
            .Visibility_Lambda([this]() { return Enemy.IsValid() && Enemy->IsBoss() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
            .Text_Lambda([this]() { return Enemy.IsValid() ? Enemy->DisplayName() : FText::GetEmpty(); })]
        + SVerticalBox::Slot().AutoHeight()
        [SNew(SBox).WidthOverride_Lambda([this]() -> FOptionalSize { return Enemy.IsValid() && Enemy->IsBoss() ? 165.f : 82.f; }).HeightOverride(5)
            [SNew(SProgressBar).Style(&Bars).FillColorAndOpacity(FLinearColor(.76,.2,.13))
                .Percent_Lambda([this]() -> TOptional<float> { return Enemy.IsValid()
                    ? FMath::Clamp(Enemy->Health / FMath::Max(1.f, Enemy->MaxHealth), 0.f, 1.f) : 0.f; })]];
}
