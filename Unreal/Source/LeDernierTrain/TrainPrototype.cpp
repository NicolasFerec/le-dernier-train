#include "TrainPrototype.h"
#include "TrainRooms.h"
#include "world/TrainRoomGeneration.h"
#include "TrainSettings.h"
#include "TimerManager.h"
#include "network/TrainSession.h"
#include "network/TrainNetwork.h"
#include "network/TrainDash.h"
#include "network/TrainMelee.h"
#include "AbilitySystemComponent.h"
#include "OnlineSubsystemModule.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameSession.h"
#include "Engine/StaticMeshActor.h"
#include "Components/TextRenderComponent.h"
#include "Sound/SoundBase.h"
#include "GameFramework/GameUserSettings.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSlider.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "AnimationRuntime.h"
#include "character/TrainAnimInstance.h"
#include "character/TrainCombatEffect.h"
#include "progression/TrainProfile.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Misc/ConfigCacheIni.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_EDITOR
#include "Kismet2/DebuggerCommands.h"
#include "Framework/Commands/UICommandInfo.h"
#endif

class FTrainModule : public FDefaultGameModuleImpl
{
#if WITH_EDITOR
    FDelegateHandle StartupHandle;
    TOptional<FInputChord> PreviousStopChord;
#endif
public:
    void ConfigureEditorEscape()
    {
#if WITH_EDITOR
        if (FPlayWorldCommands::IsRegistered())
        {
            auto Command = FPlayWorldCommands::Get().StopPlaySession;
            const FInputChord Chord = *Command->GetActiveChord(EMultipleKeyBindingIndex::Primary);
            if (Chord == FInputChord(EKeys::Escape))
            {
                PreviousStopChord = Chord;
                Command->SetActiveChord(FInputChord(EModifierKey::Shift,EKeys::Escape),EMultipleKeyBindingIndex::Primary);
            }
        }
#endif
    }
    virtual void StartupModule() override
    {
        const bool SteamRunning = IsRunningGame() && !FParse::Param(FCommandLine::Get(),TEXT("nosteam"))
            && !FParse::Param(FCommandLine::Get(),TEXT("TrainLocal"))
            && (FPlatformProcess::IsApplicationRunning(TEXT("steam")) || FPlatformProcess::IsApplicationRunning(TEXT("steam.exe"))
                || FPlatformProcess::IsApplicationRunning(TEXT("steam_osx")));
        GConfig->SetBool(TEXT("OnlineSubsystemSteam"),TEXT("bEnabled"),SteamRunning,GEngineIni);
        GConfig->SetString(TEXT("OnlineSubsystem"),TEXT("DefaultPlatformService"),SteamRunning ? TEXT("Steam") : TEXT("Null"),GEngineIni);
        if (SteamRunning) FModuleManager::LoadModuleChecked<FOnlineSubsystemModule>(TEXT("OnlineSubsystem")).ReloadDefaultSubsystem();
#if WITH_EDITOR
        StartupHandle=FCoreDelegates::GetOnPostEngineInit().AddRaw(this,&FTrainModule::ConfigureEditorEscape);
#endif
    }
    virtual void ShutdownModule() override
    {
#if WITH_EDITOR
        FCoreDelegates::GetOnPostEngineInit().Remove(StartupHandle);
        if (PreviousStopChord.IsSet() && FPlayWorldCommands::IsRegistered())
            FPlayWorldCommands::Get().StopPlaySession->SetActiveChord(PreviousStopChord.GetValue(),EMultipleKeyBindingIndex::Primary);
#endif
    }
};
IMPLEMENT_PRIMARY_GAME_MODULE(FTrainModule, LeDernierTrain, "LeDernierTrain");

namespace
{
    const FName MenuMap(TEXT("Menu"));
    const FName WagonMap(TEXT("Wagon"));
    constexpr float CameraFieldHeight = 650.f;
    const FLinearColor Brass(0.65f, 0.35f, 0.17f);
    const FLinearColor Ivory(0.94f, 0.84f, 0.67f);

    FText Text(const TCHAR* French, const TCHAR* English)
    {
        return FText::FromString(UTrainSettings::Get()->Language == TEXT("en") ? English : French);
    }

    bool IsMap(const UObject* Context, FName Map)
    {
        return UGameplayStatics::GetCurrentLevelName(Context) == Map.ToString();
    }

    AActor* Tagged(UWorld* World, FName Tag)
    {
        for (TActorIterator<AActor> It(World); It; ++It)
            if (It->ActorHasTag(Tag))
                return *It;
        return nullptr;
    }
}

ATrainPawn::ATrainPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    GetCapsuleComponent()->InitCapsuleSize(24, 46);
    SetNetUpdateFrequency(60); SetMinNetUpdateFrequency(30);
    bUseControllerRotationYaw = false;
    GetCharacterMovement()->MaxWalkSpeed = 510;
    GetCharacterMovement()->MaxAcceleration = 6800;
    GetCharacterMovement()->BrakingDecelerationWalking = 6200;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0,1440,0);
    GetCharacterMovement()->NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;
    Abilities = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("Abilities"));
    Abilities->SetIsReplicated(true); Abilities->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    GetCapsuleComponent()->InitCapsuleSize(30,90);
    GetMesh()->SetRelativeLocation(FVector(0,0,-90));
    GetMesh()->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/ConductorUE5/SK_ConductorUE5.SK_ConductorUE5")));
    GetMesh()->SetAnimInstanceClass(UTrainAnimInstance::StaticClass());
    GetMesh()->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Characters/Conductor/M_Conductor.M_Conductor")));
    ScoutModel=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Scout/SK_Scout.SK_Scout"));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    HeldWeapon=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeldWeapon"));
    HeldWeapon->SetupAttachment(GetMesh(),TEXT("hand_r"));
    HeldWeapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    OffhandWeapon=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OffhandWeapon"));
    OffhandWeapon->SetupAttachment(GetMesh(),TEXT("hand_l"));OffhandWeapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SpringArm=CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    SpringArm->SetupAttachment(GetRootComponent());SpringArm->SetUsingAbsoluteRotation(true);
    SpringArm->SetRelativeRotation(FRotator(-48,45,0));SpringArm->TargetArmLength=1800;
    SpringArm->TargetOffset=FVector(0,0,45);SpringArm->bDoCollisionTest=false;
    SpringArm->bEnableCameraLag=true;SpringArm->CameraLagSpeed=18;
    Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm,USpringArmComponent::SocketName);
    Camera->SetProjectionMode(ECameraProjectionMode::Orthographic);Camera->SetOrthoWidth(CameraFieldHeight * 16.f / 9.f);
    Camera->bConstrainAspectRatio=false;

}

void ATrainPawn::BeginPlay()
{
    Super::BeginPlay();
    Abilities->InitAbilityActorInfo(this, this);
    if (HasAuthority())
    {
        Abilities->GiveAbility(FGameplayAbilitySpec(UTrainDash::StaticClass(),1));
        Abilities->GiveAbility(FGameplayAbilitySpec(UTrainLightAttack::StaticClass(),1));
        Abilities->GiveAbility(FGameplayAbilitySpec(UTrainHeavyAttack::StaticClass(),1));
    }
    bWagon = IsMap(this, WagonMap);
    if (IsMap(this, MenuMap))
    {
        GetCharacterMovement()->DisableMovement();
        SetActorEnableCollision(false);
        SetActorTickEnabled(false);
        SetActorHiddenInGame(true);
        return;
    }
    ApplyLoadout();
    if (IsLocallyControlled()) SendLoadout();
}

TSharedRef<SWidget> UTrainStatus::RebuildWidget()
{
    return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(16, 0, 16, 26))
        [SNew(SBorder).Padding(FMargin(16, 9)).BorderBackgroundColor(FLinearColor(0.035, 0.045, 0.06, 0.92))
            [SAssignNew(Label, STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
             .ColorAndOpacity(Ivory).Justification(ETextJustify::Center)]];
}

void UTrainStatus::SetMessage(const FText& Value)
{
    if (Label && !Label->GetText().EqualTo(Value))
        Label->SetText(Value);
}

void UTrainStatus::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    Label.Reset();
}

FVector2D ATrainPawn::MovementInput() const
{
    const auto* Enhanced = Cast<UEnhancedInputComponent>(InputComponent);
    return Enhanced && MoveAction
        ? Enhanced->GetBoundActionValue(MoveAction).Get<FVector2D>().GetClampedToMaxSize(1.0)
        : FVector2D::ZeroVector;
}

void ATrainPawn::EndPlay(const EEndPlayReason::Type Reason)
{
    if (InputSubsystem.IsValid() && MovementContext)
        InputSubsystem->RemoveMappingContext(MovementContext);
    Super::EndPlay(Reason);
}

void ATrainPawn::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    Abilities->InitAbilityActorInfo(this, this);
    ApplyLoadout();
}
UAbilitySystemComponent* ATrainPawn::GetAbilitySystemComponent() const { return Abilities; }
void ATrainPawn::OnRep_PlayerState() { Super::OnRep_PlayerState(); Abilities->InitAbilityActorInfo(this, this); if (IsLocallyControlled()) SendLoadout(); }
void ATrainPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    AttackCooldown = FMath::Max(0.f, AttackCooldown - DeltaSeconds);
    SwingTime = FMath::Max(0.f, SwingTime - DeltaSeconds);
    auto* Movement = GetCharacterMovement();
    Movement->bOrientRotationToMovement = SwingTime <= 0 && !Movement->GetRootMotionSource(TEXT("Dodge"));
    if (SwingTime>0) SetActorRotation(SwingDirection);
    if (!IsLocallyControlled()) return;
    ImpactTime = FMath::Max(0.f, ImpactTime - DeltaSeconds);
    SpringArm->TargetOffset = FVector(0,0,45 + FMath::Sin(ImpactTime * 85.f) * ImpactTime * 32.f);
    if (BufferedAttack >= 0)
    {
        InputBufferTime -= DeltaSeconds;
        if (IsMenuOpen() || InputBufferTime <= 0) BufferedAttack = -1;
        else
        {
            const bool Heavy = BufferedAttack == 1;
            if (Abilities->TryActivateAbilityByClass(Heavy ? UTrainHeavyAttack::StaticClass() : UTrainLightAttack::StaticClass()))
                BufferedAttack = -1;
        }
    }
    if (auto* PC=Cast<APlayerController>(GetController()))
    { int32 Width=0,Height=0;PC->GetViewportSize(Width,Height);if (Height>0) Camera->SetOrthoWidth(CameraFieldHeight*float(Width)/Height); }
    const auto* PS = GetPlayerState<ATrainPlayerState>();
    const auto* GS = GetWorld()->GetGameState<ATrainGameState>();
    if (PS && PS->Health > 0 && GS && GS->RoomReady && !GS->RunFinished)
    {
        const auto Input = MovementInput();
        AddMovementInput(FVector(1,1,0).GetSafeNormal(), Input.Y);
        AddMovementInput(FVector(-1,1,0).GetSafeNormal(), Input.X);
    }

}

void ATrainPawn::SetMessage(const FText& TextValue)
{
    if (Message)
        Message->SetMessage(TextValue);
}

void ATrainPawn::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    Abilities->InitAbilityActorInfo(this,this);
    auto* Enhanced = Cast<UEnhancedInputComponent>(Input);
    auto* PC = Cast<APlayerController>(GetController());
    auto* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
    auto* Subsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
    if (Enhanced && Subsystem && !IsMap(this, MenuMap))
    {
        MoveAction = NewObject<UInputAction>(this, TEXT("Move"));
        MoveAction->ValueType = EInputActionValueType::Axis2D;
        MoveAction->AccumulationBehavior = EInputActionAccumulationBehavior::Cumulative;
        MovementContext = NewObject<UInputMappingContext>(this, TEXT("Movement"));
        auto BindDirection = [this](const FKey& Key, bool bVertical, bool bNegative)
        {
            auto& Mapping = MovementContext->MapKey(MoveAction, Key);
            if (bNegative)
                Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(MovementContext));
            if (bVertical)
            {
                auto* Swizzle = NewObject<UInputModifierSwizzleAxis>(MovementContext);
                Swizzle->Order = EInputAxisSwizzle::YXZ;
                Mapping.Modifiers.Add(Swizzle);
            }
        };
        auto* Preferences = UTrainSettings::Get();
        if (!Preferences->HasGameplayBinding(EKeys::Up)) BindDirection(EKeys::Up,true,false);
        if (!Preferences->HasGameplayBinding(EKeys::Down)) BindDirection(EKeys::Down,true,true);
        if (!Preferences->HasGameplayBinding(EKeys::Left)) BindDirection(EKeys::Left,false,true);
        if (!Preferences->HasGameplayBinding(EKeys::Right)) BindDirection(EKeys::Right,false,false);
        for (bool Pad : { false, true })
        {
            BindDirection(Preferences->Key(TEXT("MoveUp"), Pad), true, false);
            BindDirection(Preferences->Key(TEXT("MoveDown"), Pad), true, true);
            BindDirection(Preferences->Key(TEXT("MoveLeft"), Pad), false, true);
            BindDirection(Preferences->Key(TEXT("MoveRight"), Pad), false, false);
        }
        auto* DeadZone = NewObject<UInputModifierDeadZone>(MovementContext);
        DeadZone->LowerThreshold = 0.24f;
        MovementContext->MapKey(MoveAction, EKeys::Gamepad_Left2D).Modifiers.Add(DeadZone);
        Enhanced->BindActionValue(MoveAction);
        Subsystem->AddMappingContext(MovementContext, 0);
        InputSubsystem = Subsystem;
    }
    auto* Preferences = UTrainSettings::Get();
    for (bool Pad : { false, true })
    {
        Input->BindKey(Preferences->Key(TEXT("Interact"), Pad), IE_Pressed, this, &ATrainPawn::Interact);
        Input->BindKey(Preferences->Key(TEXT("Dash"), Pad), IE_Pressed, this, &ATrainPawn::Dash);
        Input->BindKey(Preferences->Key(TEXT("Attack"), Pad), IE_Pressed, this, &ATrainPawn::Attack);
        Input->BindKey(Preferences->Key(TEXT("HeavyAttack"), Pad), IE_Pressed, this, &ATrainPawn::HeavyAttack);
    }
    if (!Preferences->HasGameplayBinding(EKeys::LeftMouseButton)) Input->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ATrainPawn::Attack);
    if (!Preferences->HasGameplayBinding(EKeys::RightMouseButton)) Input->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ATrainPawn::HeavyAttack);
    Input->BindKey(Preferences->Key(TEXT("Pause"),false), IE_Pressed, this, &ATrainPawn::TogglePause).bExecuteWhenPaused = true;
    Input->BindKey(Preferences->Key(TEXT("Pause"),true), IE_Pressed, this, &ATrainPawn::TogglePause).bExecuteWhenPaused = true;
}

void ATrainPawn::RefreshControls()
{
    if (InputSubsystem.IsValid() && MovementContext)
        InputSubsystem->RemoveMappingContext(MovementContext);
    if (auto* Enhanced = Cast<UEnhancedInputComponent>(InputComponent))
    {
        Enhanced->ClearActionValueBindings();
        Enhanced->KeyBindings.Empty();
        SetupPlayerInputComponent(Enhanced);
    }
}

void ATrainPawn::TogglePause()
{
    auto* PC = Cast<APlayerController>(GetController());
    if (!PC) return;
    if (!PauseMenu && GetNetMode() == NM_Client)
    {
        if (auto* Controller = Cast<ATrainPlayerController>(PC)) Controller->ClientNotice(ETrainNotice::OnlyHostCanPause);
        return;
    }
    if (PauseMenu)
    {
        if (PauseMenu->PauseMode) PC->SetPause(false);
        PauseMenu->RemoveFromParent();
        PauseMenu = nullptr;
        RefreshControls();
        PC->bShowMouseCursor = false;
        // Finish routing the menu key before handing input back to gameplay.
        // Otherwise the same Escape event can open pause again immediately.
        GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(PC, [PC]()
        {
            PC->FlushPressedKeys();
            PC->SetInputMode(FInputModeGameOnly());
            UWidgetBlueprintLibrary::SetFocusToGameViewport();
        }));
        UTrainSettings::Get()->ApplyRuntime(GetWorld(), false);
    }
    else
    {
        for (APlayerState* PS:GetWorld()->GetGameState()->PlayerArray) PS->ForceNetUpdate();
        GetWorld()->GetGameState()->ForceNetUpdate();
        PC->SetPause(true);
        PauseMenu = CreateWidget<UTrainMenu>(PC);
        PauseMenu->PauseMode = true;
        PauseMenu->AddToViewport(20);
        PC->bShowMouseCursor = true;
        FInputModeUIOnly Mode;
        Mode.SetWidgetToFocus(PauseMenu->TakeWidget());
        PC->SetInputMode(Mode);
        UTrainSettings::Get()->ApplyRuntime(GetWorld(), true);
    }
}

void ATrainPawn::ShowSession()
{
    if (PauseMenu || !IsLocallyControlled()) return;
    auto* PC = Cast<APlayerController>(GetController());
    PauseMenu = CreateWidget<UTrainMenu>(PC); PauseMenu->SessionMode = true; PauseMenu->AddToViewport(20);
    UTrainSettings::Get()->ApplyRuntime(GetWorld(),true);
    PC->bShowMouseCursor = true; FInputModeUIOnly Mode; Mode.SetWidgetToFocus(PauseMenu->TakeWidget()); PC->SetInputMode(Mode);
}
void ATrainPawn::ShowStation(int32 Kind)
{
    if (Kind==4) { ShowSession();return; }
    if (PauseMenu || !IsLocallyControlled()) return;
    auto* PC=Cast<APlayerController>(GetController());if (!PC) return;
    PauseMenu=CreateWidget<UTrainMenu>(PC);PauseMenu->Station=Kind;PauseMenu->AddToViewport(20);
    UTrainSettings::Get()->ApplyRuntime(GetWorld(),true);
    PC->bShowMouseCursor=true;FInputModeUIOnly Mode;Mode.SetWidgetToFocus(PauseMenu->TakeWidget());PC->SetInputMode(Mode);
}
void ATrainPawn::Interact()
{
    if (IsMenuOpen()) return;
    ATrainStation* Closest=nullptr;float Distance=230;
    for (TActorIterator<ATrainStation> It(GetWorld());It;++It)
    {
        const float D=FVector::Dist2D(GetActorLocation(),It->GetActorLocation());
        if (D<Distance) { Closest=*It;Distance=D; }
    }
    if (Closest && Closest->Kind!=3 && bWagon) ShowStation(Closest->Kind);
    else ServerInteract();
}
void ATrainPawn::ServerInteract_Implementation()
{ if (auto* Mode=GetWorld()->GetAuthGameMode<ATrainGameMode>()) Mode->RequestDeparture(this); }
void ATrainPawn::SendLoadout()
{
    if (!bWagon) return;
    if (auto* Profile=GetGameInstance()->GetSubsystem<UTrainProfile>();Profile && Profile->Data)
        ServerLoadout(Profile->Data->Character,Profile->Data->Weapon,Profile->Has(TEXT("health")),Profile->Has(TEXT("speed")));
}
void ATrainPawn::ServerLoadout_Implementation(int32 NewCharacter,int32 NewWeapon,bool Health,bool Speed)
{
    if (!bWagon || NewCharacter<0 || NewCharacter>1 || NewWeapon<0 || NewWeapon>1) return;
    auto* PS=GetPlayerState<ATrainPlayerState>();if (!PS) return;
    Character=PS->Character=NewCharacter;Weapon=PS->Weapon=NewWeapon;
    PS->HealthUpgrade=Health;PS->SpeedUpgrade=Speed;PS->MaxHealth=(Character?80:100)+(Health?20:0);PS->Health=PS->MaxHealth;
    ApplyLoadout();PS->ForceNetUpdate();ForceNetUpdate();
}
void ATrainPawn::ApplyLoadout()
{
    if (auto* PS=GetPlayerState<ATrainPlayerState>())
    {
        if (HasAuthority()) { Character=PS->Character;Weapon=PS->Weapon; }
        GetCharacterMovement()->MaxWalkSpeed=(Character?560:510)*(PS->SpeedUpgrade?1.15f:1.f);
    }
    auto* Model=Character?ScoutModel.Get():LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/ConductorUE5/SK_ConductorUE5.SK_ConductorUE5"));
    if (Model && Model!=GetMesh()->GetSkeletalMeshAsset())
    {
        GetMesh()->EmptyOverrideMaterials();
        GetMesh()->SetSkeletalMesh(Model);
        GetMesh()->InitAnim(true);
    }
    const bool Hammer=Character==0 && Weapon==1;
    HeldWeapon->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,Hammer ? TEXT("/Game/Weapons/hammer.hammer"):TEXT("/Game/Weapons/sabre.sabre")));
    HeldWeapon->SetRelativeScale3D(FVector(Hammer?.9f:Character&&Weapon?.55f:.95f));
    const auto& Skeleton=GetMesh()->GetSkeletalMeshAsset()->GetRefSkeleton();
    auto Grip=[this,&Skeleton](UStaticMeshComponent* Mesh,const TCHAR* Side,const FVector& HandleCenter)
    {
        const bool UE5 = Skeleton.FindBoneIndex(TEXT("hand_r")) != INDEX_NONE;
        const TCHAR* Suffix = FCString::Strcmp(Side,TEXT("Right"))==0 ? TEXT("r") : TEXT("l");
        const FName Hand(*(UE5 ? FString::Printf(TEXT("hand_%s"),Suffix) : FString::Printf(TEXT("%sHand"),Side)));
        const int32 HandIndex=Skeleton.FindBoneIndex(Hand);
        if (HandIndex==INDEX_NONE) return;
        const FTransform HandPose=FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton,HandIndex);
        auto PalmPoint=[&](const TCHAR* Finger)
        {
            const FString Bone=UE5 ? FString::Printf(TEXT("%s_01_%s"),*FString(Finger).ToLower(),Suffix) : FString::Printf(TEXT("%sHand%s1"),Side,Finger);
            const int32 Index=Skeleton.FindBoneIndex(*Bone);
            return Index!=INDEX_NONE ? HandPose.InverseTransformPosition(FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton,Index).GetLocation()):FVector::ZeroVector;
        };
        const FVector FingerDirection=PalmPoint(TEXT("Middle"));
        const FVector BladeDirection=(PalmPoint(TEXT("Index"))-PalmPoint(TEXT("Pinky"))).GetSafeNormal();
        Mesh->AttachToComponent(GetMesh(),FAttachmentTransformRules::KeepRelativeTransform,Hand);
        if (!BladeDirection.IsNearlyZero() && !FingerDirection.IsNearlyZero())
        {
            // Both imported weapons point along +X; their handles have different pivots.
            const FQuat Axes=FRotationMatrix::MakeFromXY(BladeDirection,FingerDirection).ToQuat();
            Mesh->SetRelativeRotation(Axes);
            Mesh->SetRelativeLocation(FingerDirection*.65f-Axes.RotateVector(HandleCenter*Mesh->GetRelativeScale3D()));
        }
        else if (Character==0 && UE5 && Suffix[0]==TEXT('r'))
        {
            // Closed glove: the generated UE5 hand has middle phalanges but no
            // index/pinky landmarks. Bind-pose calibration is recorded in
            // SourceArt/Characters/Conductor/UE5Rework/validation.json.
            const FQuat Axes(.8124045834,-.0487389947,-.5760366176,-.0761913282);
            Mesh->SetRelativeRotation(Axes);
            Mesh->SetRelativeLocation(FVector(2.224758,-1.595244,-3.602174)-Axes.RotateVector(HandleCenter*Mesh->GetRelativeScale3D()));
        }
    };
    const FVector SabreHandle(0,7,-3);
    Grip(HeldWeapon,TEXT("Right"),Hammer?FVector::ZeroVector:SabreHandle);
    OffhandWeapon->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Weapons/sabre.sabre")));
    OffhandWeapon->SetRelativeScale3D(FVector(.55f));OffhandWeapon->SetVisibility(Character==1&&Weapon==1);
    Grip(OffhandWeapon,TEXT("Left"),SabreHandle);

}
void ATrainPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{ Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(ATrainPawn,Character);DOREPLIFETIME(ATrainPawn,Weapon); }
float ATrainPawn::MeleeDuration(bool Heavy) const
{
    const bool Hammer = Character == 0 && Weapon == 1;
    const bool Daggers = Character == 1 && Weapon == 1;
    if (Hammer) return Heavy ? .64f : .39f;
    if (Daggers) return Heavy ? .40f : .23f;
    return Heavy ? .48f : .29f;
}
float ATrainPawn::MeleeContactTime(bool Heavy) const
{
    return MeleeDuration(Heavy) * (Heavy ? .46f : .38f);
}
bool ATrainPawn::CanDodge() const
{
    return SwingTime <= 0 && GetCharacterMovement()->IsMovingOnGround();
}
void ATrainPawn::Dash() { if (!IsMenuOpen() && CanDodge()) Abilities->TryActivateAbilityByClass(UTrainDash::StaticClass()); }
void ATrainPawn::QueueAttack(bool Heavy)
{
    if (IsMenuOpen()) return;
    BufferedAttack = Heavy ? 1 : 0;
    InputBufferTime = .20f;
    if (Abilities->TryActivateAbilityByClass(Heavy ? UTrainHeavyAttack::StaticClass() : UTrainLightAttack::StaticClass()))
        BufferedAttack = -1;
}
void ATrainPawn::Attack() { QueueAttack(false); }
void ATrainPawn::HeavyAttack() { QueueAttack(true); }
void ATrainPawn::FaceAttackTarget()
{
    FVector Forward = GetActorForwardVector();
    const FVector Acceleration = GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D();
    if (!Acceleration.IsNearlyZero()) Forward = Acceleration;
    float BestScore = 0;
    FVector Aim = Forward;
    for (TActorIterator<ATrainEnemy> It(GetWorld()); It; ++It)
    {
        const FVector Delta = It->GetActorLocation() - GetActorLocation();
        const float Distance = Delta.Size2D();
        const float Alignment = FVector::DotProduct(Delta.GetSafeNormal2D(), Forward);
        if (It->Health <= 0 || Distance > 310 || Alignment < .15f) continue;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(TrainAim),false,this);
        Query.AddIgnoredActor(*It);
        if (GetWorld()->LineTraceTestByChannel(GetActorLocation(),It->GetActorLocation(),ECC_Visibility,Query)) continue;
        const float Score = Alignment + 1.f - Distance / 310.f;
        if (Score > BestScore) { BestScore = Score; Aim = Delta.GetSafeNormal2D(); }
    }
    SetActorRotation(FRotator(0,Aim.Rotation().Yaw,0));
}
void ATrainPawn::ConfirmImpact_Implementation(bool Heavy)
{
    if (!IsLocallyControlled()) return;
    ImpactTime = Heavy ? .16f : .09f;
    if (auto* PC=Cast<APlayerController>(GetController()); PC && UTrainSettings::Get()->Vibration)
        PC->PlayDynamicForceFeedback(Heavy?.55f:.23f,Heavy?.14f:.07f,true,true,true,true);
}
void ATrainPawn::PredictMelee(bool Heavy)
{ MulticastSwing_Implementation(Heavy,GetActorRotation());PredictedMelee=true; }
void ATrainPawn::CancelPredictedMelee()
{
    if (!PredictedMelee) return;PredictedMelee=false;SwingTime=0;
    if (auto* Anim=GetMesh()->GetAnimInstance()) Anim->Montage_Stop(.08f);
}
void ATrainPawn::ServerAttack_Implementation(bool Heavy)
{
    auto* PS=GetPlayerState<ATrainPlayerState>();auto* GS=GetWorld()->GetGameState<ATrainGameState>();
    if (!PS || PS->Health<=0 || !GS || !GS->RoomReady || GS->RunFinished || AttackCooldown>0 || !GetCharacterMovement()->IsMovingOnGround()
        || GetCharacterMovement()->GetRootMotionSource(TEXT("Dodge")) || UGameplayStatics::IsGamePaused(this)) return;
    AttackCooldown = MeleeDuration(Heavy);
    FaceAttackTarget();
    const FRotator Direction=GetActorRotation();MulticastSwing(Heavy,Direction);
    // A native montage supplies the motion. Its contact time is shared by authoritative damage.
    GetWorldTimerManager().SetTimer(StrikeTimer,FTimerDelegate::CreateWeakLambda(this,[this,Heavy,Direction](){ResolveStrike(Heavy,Direction);}),MeleeContactTime(Heavy),false);
}
void ATrainPawn::ResolveStrike(bool Heavy,FRotator Direction)
{
    auto* PS=GetPlayerState<ATrainPlayerState>();if (!PS || PS->Health<=0) return;
    const bool Hammer=Character==0 && Weapon==1;
    const float Damage=(Hammer?45.f:Character&&Weapon?20.f:30.f)*(Heavy?2.f:1.f);
    const float Reach=Hammer?185.f:210.f;
    auto InArc=[&](AActor* Target)
    {
        const FVector Delta=Target->GetActorLocation()-GetActorLocation();
        FCollisionQueryParams Query(SCENE_QUERY_STAT(TrainMeleeSight),false,this);
        Query.AddIgnoredActor(Target);
        return Delta.SizeSquared2D()<FMath::Square(Reach) && FVector::DotProduct(Delta.GetSafeNormal2D(),Direction.Vector())>(Heavy?-.05f:.2f)
            && !GetWorld()->LineTraceTestByChannel(GetActorLocation(),Target->GetActorLocation(),ECC_Visibility,Query);
    };
    for (TActorIterator<ATrainDummy> It(GetWorld());It;++It) if (InArc(*It)) { It->Hit(Damage); ConfirmImpact(Heavy); }
    if (!bWagon) for (TActorIterator<ATrainEnemy> It(GetWorld());It;++It) if (InArc(*It)) { It->Hit(this,Damage); ConfirmImpact(Heavy); }
}
void ATrainPawn::MulticastSwing_Implementation(bool Heavy,FRotator Direction)
{
    if (PredictedMelee && IsLocallyControlled()) { PredictedMelee=false;return; }
    // Release facing and allow the dodge once the strike has passed contact.
    SwingTime = MeleeContactTime(Heavy) + .05f;
    SwingDirection = Direction;
    const double Now = GetWorld()->GetTimeSeconds();
    ComboIndex = !Heavy && Now-LastSwingAt < .72 ? 1-ComboIndex : 0;
    LastSwingAt = Now;
    const FName Clip = Character==1&&Weapon==1&&Heavy ? TEXT("DualHeavy") : Character==0&&Weapon==1 ? (Heavy?TEXT("HammerHeavy"):TEXT("HammerLight"))
        : Heavy?TEXT("Heavy"):ComboIndex?TEXT("LightReturn"):TEXT("Light");
    if (auto* Anim=Cast<UTrainAnimInstance>(GetMesh()->GetAnimInstance())) Anim->Action(Clip,MeleeDuration(Heavy));
    FTimerHandle TrailTimer;
    GetWorldTimerManager().SetTimer(TrailTimer, FTimerDelegate::CreateWeakLambda(this,[this,Direction,Heavy]()
    {
        ATrainCombatEffect::Spawn(GetWorld(),GetActorLocation()-FVector(0,0,40),Direction,false,Heavy);
    }),MeleeContactTime(Heavy),false);
    const TCHAR* Track=Heavy?TEXT("heavy_swing"):Character&&Weapon?TEXT("dagger_swing"):TEXT("blade_swing");
    if (auto* Sound=LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("/Game/Audio/%s.%s"),Track,Track))) UGameplayStatics::PlaySoundAtLocation(this,Sound,GetActorLocation());
}
void ATrainPawn::MulticastDashSound_Implementation()
{
    if (auto* Sound=LoadObject<USoundBase>(nullptr,TEXT("/Game/Audio/dash.dash"))) UGameplayStatics::PlaySoundAtLocation(this,Sound,GetActorLocation());
}
void ATrainPawn::MulticastDamage_Implementation(float Damage,bool Dead)
{
    if (auto* PC=Cast<ATrainPlayerController>(GetWorld()->GetFirstPlayerController())) PC->CombatText(GetActorLocation()-FVector(0,0,65),Damage,true);
    if (auto* Anim=Cast<UTrainAnimInstance>(GetMesh()->GetAnimInstance())) Anim->Action(Dead?TEXT("Death"):TEXT("Hit"),Dead?1.1f:.24f);
}
void ATrainPawn::ReceiveHit(float Damage)
{
    auto* PS=GetPlayerState<ATrainPlayerState>();auto* GS=GetWorld()->GetGameState<ATrainGameState>();
    if (!HasAuthority() || bWagon || Damage<=0 || !PS || !GS || PS->Health<=0 || GS->RunFinished || GS->GetServerWorldTimeSeconds()<PS->DashReadyAt - (UTrainDash::CooldownDuration - UTrainDash::LeapDuration)) return;
    const float Actual=FMath::Min(PS->Health,Damage);PS->Health-=Actual;PS->DamageTaken+=Actual;PS->ForceNetUpdate();
    MulticastDamage(Actual,PS->Health<=0);
    if (auto* PC=Cast<ATrainPlayerController>(GetController())) PC->ClientHurt();
    if (PS->Health<=0) { GetCharacterMovement()->DisableMovement();GetWorldTimerManager().ClearTimer(StrikeTimer); }
}

ATrainGameMode::ATrainGameMode()
{
    DefaultPawnClass = ATrainPawn::StaticClass();
    PlayerControllerClass = ATrainPlayerController::StaticClass();
    PlayerStateClass = ATrainPlayerState::StaticClass();
    GameStateClass = ATrainGameState::StaticClass();
    bUseSeamlessTravel = true;
    PrimaryActorTick.bCanEverTick = true;
}

void ATrainGameMode::StartPlay()
{
#if WITH_EDITOR
    FModuleManager::GetModuleChecked<FTrainModule>("LeDernierTrain").ConfigureEditorEscape();
#endif
    Super::StartPlay();
    auto* GS = GetGameState<ATrainGameState>();
    if (auto* Session=GetGameInstance<UTrainSession>()) GS->RoomsCleared=Session->RoomsCleared;
    GS->DeparturePoint = IsMap(this,WagonMap) ? FVector(2650,0,0) : FVector(2050,1250,0);
    GS->RoomIndex = TrainRooms::Index(UGameplayStatics::GetCurrentLevelName(this));
    if (GameSession) GameSession->MaxPlayers = 4;
    if (auto* Session = GetGameInstance<UTrainSession>()) Session->SetJoinable(IsMap(this, WagonMap));
    if (!IsMap(this,MenuMap) && !IsMap(this,WagonMap))
    {
        TArray<AActor*> Templates;
        for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
            if (It->ActorHasTag(TEXT("Automate"))) Templates.Add(*It);
        if (GS->RoomIndex>0)
        {
            for (auto* Template:Templates) Template->Destroy();
            auto* Session=GetGameInstance<UTrainSession>();
            if (!Session->ExpeditionCrew) Session->ExpeditionCrew=FMath::Clamp(GS->PlayerArray.Num(),1,4);
            if (!Session->ExpeditionSeed)
            {
                Session->ExpeditionSeed=FGuid::NewGuid().A&MAX_int32;
#if !UE_BUILD_SHIPPING
                FParse::Value(FCommandLine::Get(),TEXT("TrainSeed="),Session->ExpeditionSeed);
#endif
            }
            auto* Generator=GetWorld()->SpawnActor<ATrainRoomGenerator>();
            Generator->Initialize(GS->RoomIndex,HashCombineFast(Session->ExpeditionSeed,GS->RoomIndex)&MAX_int32);
        }
        else for (auto* Template : Templates)
        {
            FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
            if (auto* Enemy = GetWorld()->SpawnActor<ATrainEnemy>(Template->GetActorLocation(), Template->GetActorRotation(), Params))
            {
                ETrainEnemyType Type = ETrainEnemyType::Patrol;
                if (Template->ActorHasTag(TEXT("Sentry"))) Type = ETrainEnemyType::Sentry;
                else if (Template->ActorHasTag(TEXT("Runner"))) Type = ETrainEnemyType::Runner;
                else if (Template->ActorHasTag(TEXT("Gunner"))) Type = ETrainEnemyType::Gunner;
                else if (Template->ActorHasTag(TEXT("Lockkeeper"))) Type = ETrainEnemyType::Lockkeeper;
                else if (Template->ActorHasTag(TEXT("Guardian"))) Type = ETrainEnemyType::Guardian;
                Enemy->Configure(Type);
                ++GS->RemainingEnemies;
            }
            Template->Destroy();
        }
    }
    if (IsMap(this,WagonMap)) for (APlayerState* Base:GS->PlayerArray)
        if (auto* PS=Cast<ATrainPlayerState>(Base)) PS->Health=PS->MaxHealth;
    // Station meshes and placement are authored in each map; actors are replicated by Unreal.
    for (TActorIterator<ATrainStation> It(GetWorld());It;++It)
        if (It->Kind==3) GS->DeparturePoint=It->GetActorLocation();

}
void ATrainGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& Error)
{
    Super::PreLogin(Options,Address,UniqueId,Error);
    if (!Error.IsEmpty()) return;
    const auto* Session = GetGameInstance<UTrainSession>();
    const FString Remote = UGameplayStatics::ParseOption(Options,TEXT("TrainVersion"));
    if (Remote != UTrainSession::Version() && !(Remote.IsEmpty() && Session && Session->IsLocalDev())) Error = TEXT("TRAIN_VERSION");
    else if (GetNumPlayers() >= 4) Error = TEXT("SESSION_FULL");
    else if (!IsMap(this,WagonMap)) Error = TEXT("RUN_IN_PROGRESS");
}
bool ATrainGameMode::EveryoneReady() const
{
    const auto* State = GetGameState<ATrainGameState>();
    bool HasSurvivor = false;
    for (const APlayerState* Player : State->PlayerArray)
    {
        const auto* Survivor = Cast<ATrainPlayerState>(Player);
        if (!Survivor || Survivor->Health <= 0) continue;

        const APawn* Pawn = Survivor->GetPawn();
        if (!Pawn || FVector::Dist2D(Pawn->GetActorLocation(), State->DeparturePoint) > ATrainGameState::ExitRadius)
            return false;
        HasSurvivor = true;
    }
    return HasSurvivor;
}
void ATrainGameMode::RequestDeparture(ATrainPawn* Requester)
{
    auto* Controller = Cast<ATrainPlayerController>(Requester->GetController());
    auto* State = GetGameState<ATrainGameState>();
    if (!Controller || !State || !State->RoomReady || State->RunFinished || IsMap(this, MenuMap) || UGameplayStatics::IsGamePaused(this)) return;

    const auto* Host = Controller->GetPlayerState<ATrainPlayerState>();
    if (!Host || !Host->IsHost)
    {
        Controller->ClientNotice(ETrainNotice::OnlyHostCanDepart);
        return;
    }
    if (State->RemainingEnemies > 0) return;
    if (Host->Health <= 0)
    {
        if (!State->CanFallenHostDepart(Host)) return;
    }
    else if (FVector::Dist2D(Requester->GetActorLocation(), State->DeparturePoint) > ATrainGameState::ExitRadius)
    {
        return;
    }
    if (EveryoneReady()) Depart();
    else if (State->DepartureAt <= 0) State->DepartureAt = State->GetServerWorldTimeSeconds() + 10;
}
void ATrainGameMode::Depart()
{
    GetGameState<ATrainGameState>()->DepartureAt=0;
    const bool Wagon=IsMap(this,WagonMap);
    if (Wagon)
    {
        GetGameInstance<UTrainSession>()->ExpeditionSeed=0;
        GetGameInstance<UTrainSession>()->ExpeditionCrew=FMath::Clamp(GetGameState<ATrainGameState>()->PlayerArray.Num(),1,4);
    }
    if (auto* Session=GetGameInstance<UTrainSession>()) Session->RoomsCleared = Wagon ? 0 : GetGameState<ATrainGameState>()->RoomsCleared;
    if (Wagon)
        for (APlayerState* State:GetGameState<ATrainGameState>()->PlayerArray)
            if (auto* PS=Cast<ATrainPlayerState>(State)) { PS->Kills=0; PS->DamageTaken=0; PS->DamageDealt=0; PS->Health=PS->MaxHealth; }
    const int32 RoomIndex = GetGameState<ATrainGameState>()->RoomIndex;
    if ((Wagon || RoomIndex > 0) && RoomIndex < TrainRooms::Count)
        GetWorld()->ServerTravel(FString(TEXT("/Game/Maps/")) + TrainRooms::Faubourgs[RoomIndex].Map);
    else FinishRun(true);
}
void ATrainGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds); auto* GS=GetGameState<ATrainGameState>();
    if (!GS || IsMap(this,MenuMap)) return;
    if (GS->DepartureAt>0 && (EveryoneReady() || GS->GetServerWorldTimeSeconds()>=GS->DepartureAt)) { Depart(); return; }
    if (IsMap(this,WagonMap) || !GS->RoomReady || GS->RunFinished || GS->PlayerArray.IsEmpty()) return;
    bool Alive=false;
    for (APlayerState* State:GS->PlayerArray) if (auto* PS=Cast<ATrainPlayerState>(State)) Alive |= PS->Health>0;
    if (!Alive) FinishRun(false);
}
void ATrainGameMode::Logout(AController* Exiting)
{
    Super::Logout(Exiting);
    // Readiness is recalculated on the next native game tick, including disconnects.
}

void ATrainGameMode::FinishRun(bool Won)
{
    auto* GS=GetGameState<ATrainGameState>();if (!GS || GS->RunFinished) return;
    GS->RunFinished=true;GS->Victory=Won;GS->ForceNetUpdate();
    if (!Rewarded)
    {
        Rewarded=true;
        const int32 Points=GS->RoomsCleared+(Won?1:0);
        if (Points>0) for (auto It=GetWorld()->GetPlayerControllerIterator();It;++It)
            if (auto* PC=Cast<ATrainPlayerController>(It->Get())) PC->ClientReward(Points);
    }
}
