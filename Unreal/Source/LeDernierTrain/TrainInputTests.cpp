#if WITH_DEV_AUTOMATION_TESTS

#include "TrainPrototype.h"
#include "TrainSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "InputAction.h"
#include "InputKeyEventArgs.h"
#if WITH_EDITOR
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainKeyboardLayouts, "LeDernierTrain.Input.KeyboardLayouts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FCheckTrainKeyboardLayouts, FTrainKeyboardLayouts*, Test);

bool FCheckTrainKeyboardLayouts::Update()
{
    auto TestTrue = [this](const FString& What, bool Value) { return Test->TestTrue(What, Value); };
    auto TestNotNull = [this](const FString& What, const void* Value) { return Test->TestNotNull(What, Value); };
    auto AddError = [this](const FString& What) { Test->AddError(What); };

    UWorld* World = nullptr;
    for (const auto& Context : GEngine->GetWorldContexts())
        if (Context.WorldType == EWorldType::PIE)
            World = Context.World();
    if (!World || !World->GetFirstPlayerController())
    {
        AddError(TEXT("Start PIE in the Wagon map before running this test."));
        return true;
    }

    auto* PC = World->GetFirstPlayerController();
    auto* Subsystem = PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    const int32 SavedLayout = UTrainSettings::Get()->KeyboardLayout;
    const auto SavedBindings = UTrainSettings::Get()->Bindings;
    UTrainSettings::Get()->Bindings.Empty();
    APawn* OriginalPawn = PC->GetPawn();
    PC->UnPossess();

    for (int32 Layout = 1; Layout <= 2; ++Layout)
    {
        UTrainSettings::Get()->KeyboardLayout = Layout;
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Pawn = World->SpawnActor<ATrainPawn>(FVector(-100, -50, 95), FRotator::ZeroRotator, Spawn);
        PC->Possess(Pawn);
        Pawn->InputComponent = NewObject<UEnhancedInputComponent>(Pawn);
        Pawn->SetupPlayerInputComponent(Pawn->InputComponent);
        auto* Component = CastChecked<UEnhancedInputComponent>(Pawn->InputComponent);
        const auto* Action = FindObject<UInputAction>(Pawn, TEXT("Move"));
        TestNotNull(TEXT("Movement action exists"), Action);
        FModifyContextOptions Options;
        Options.bForceImmediately = true;
        Options.bIgnoreAllPressedKeysUntilRelease = false;
        Subsystem->RequestRebuildControlMappings(Options);

        auto Check = [&](const FKey& Key, FVector2D Expected)
        {
            PC->PlayerInput->FlushPressedKeys();
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Pressed, 1.0f));
            TArray<UInputComponent*> Stack { Component };
            PC->PlayerInput->ProcessInputStack(Stack, 1.0f / 60.0f, false);
            const FVector2D Actual = Component->GetBoundActionValue(Action).Get<FVector2D>().GetClampedToMaxSize(1.0);
            TestTrue(FString::Printf(TEXT("Layout %d key %s gives %s (actual %s)"),
                Layout, *Key.ToString(), *Expected.ToString(), *Actual.ToString()), Actual.Equals(Expected, .01));
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Released, 0.0f));
            PC->PlayerInput->ProcessInputStack(Stack, 1.0f / 60.0f, false);
        };
        Check(EKeys::Z, Layout != 2 ? FVector2D(0, 1) : FVector2D::ZeroVector);
        Check(EKeys::Q, Layout != 2 ? FVector2D(-1, 0) : FVector2D::ZeroVector);
        Check(EKeys::W, Layout != 1 ? FVector2D(0, 1) : FVector2D::ZeroVector);
        Check(EKeys::A, Layout != 1 ? FVector2D(-1, 0) : FVector2D::ZeroVector);
        Check(EKeys::S, FVector2D(0, -1));
        Check(EKeys::D, FVector2D(1, 0));
        Check(EKeys::Up, FVector2D(0, 1));
        Check(EKeys::Left, FVector2D(-1, 0));
        PC->UnPossess();
        Pawn->Destroy();
    }
    auto* Settings = UTrainSettings::Get();
    TestTrue(TEXT("A new keyboard attack binding is accepted"), Settings->Rebind(TEXT("Attack"), false, EKeys::L));
    TestTrue(TEXT("Attack uses the new binding"), Settings->Key(TEXT("Attack"), false) == EKeys::L);
    TestTrue(TEXT("Duplicate action binding is rejected"), !Settings->Rebind(TEXT("Interact"), false, EKeys::L));
    TestTrue(TEXT("Pause key is reserved"), !Settings->Rebind(TEXT("Attack"), false, EKeys::Escape));
    TestTrue(TEXT("Gamepad slot rejects keyboard input"), !Settings->Rebind(TEXT("Attack"), true, EKeys::M));
    Settings->KeyboardLayout = SavedLayout;
    Settings->Bindings = SavedBindings;
    Settings->SaveSettings();
    if (OriginalPawn)
    {
        PC->Possess(OriginalPawn);
        if (auto* Pawn = Cast<ATrainPawn>(OriginalPawn))
        {
            Pawn->TogglePause();
            TestTrue(TEXT("Pause actually pauses the world"), UGameplayStatics::IsGamePaused(World));
            Pawn->TogglePause();
            TestTrue(TEXT("Resume actually unpauses the world"), !UGameplayStatics::IsGamePaused(World));
        }
    }
    return true;
}

bool FTrainKeyboardLayouts::RunTest(const FString& Parameters)
{
#if WITH_EDITOR
    FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Maps/Wagon"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
    ADD_LATENT_AUTOMATION_COMMAND(FCheckTrainKeyboardLayouts(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
#endif
    return true;
}
#endif
