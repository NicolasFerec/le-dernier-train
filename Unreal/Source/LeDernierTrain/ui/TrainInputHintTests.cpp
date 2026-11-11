#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "TrainInputHints.h"
#include "../TrainPrototype.h"
#include "../TrainSettings.h"
#include "../network/TrainNetwork.h"
#include "CommonInputSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerInput.h"
#include "Components/InputComponent.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainHintTest,"LeDernierTrain.Input.ActionHints",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FCheckTrainHints,FTrainHintTest*,Test);
bool FCheckTrainHints::Update()
{
    UWorld* World=nullptr;
    for (const auto& Context:GEngine->GetWorldContexts()) if (Context.WorldType==EWorldType::PIE) World=Context.World();
    auto* PC=World ? Cast<ATrainPlayerController>(World->GetFirstPlayerController()) : nullptr;
    auto* Pawn=PC ? Cast<ATrainPawn>(PC->GetPawn()) : nullptr;
    auto* Input=PC ? UCommonInputSubsystem::Get(PC->GetLocalPlayer()) : nullptr;
    if (!PC || !Pawn || !Input) { Test->AddError(TEXT("Local player and Common Input must be initialized")); return true; }
    auto* Settings=UTrainSettings::Get();
    const auto SavedBindings=Settings->Bindings; const int32 SavedLayout=Settings->KeyboardLayout;
    Settings->Bindings.Empty(); Settings->KeyboardLayout=1;
    Input->SetCurrentInputType(ECommonInputType::MouseAndKeyboard);
    Test->TestTrue(TEXT("Default interaction hint matches keyboard action"),TrainInput::ActionKey(Pawn,TEXT("Interact"))==EKeys::E);
    Test->TestTrue(TEXT("Interaction can be remapped"),Settings->Rebind(TEXT("Interact"),false,EKeys::F));
    Test->TestTrue(TEXT("Hint follows remapped keyboard key"),TrainInput::ActionKey(Pawn,TEXT("Interact"))==EKeys::F);
    Test->TestTrue(TEXT("Gamepad interaction can be remapped"),Settings->Rebind(TEXT("Interact"),true,EKeys::Gamepad_LeftShoulder));
    Pawn->RefreshControls();
    Pawn->SetActorLocation(FVector(-650,-150,95),false,nullptr,ETeleportType::TeleportPhysics);
    auto Press=[PC,Pawn](FKey Key)
    {
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(Key,IE_Pressed,1));
        TArray<UInputComponent*> Stack{Pawn->InputComponent};
        PC->PlayerInput->ProcessInputStack(Stack,1.f/60,false);
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(Key,IE_Released,0));
        PC->PlayerInput->FlushPressedKeys();
    };
    Press(EKeys::F);
    Test->TestTrue(TEXT("Displayed remapped keyboard key really opens the station"),Pawn->IsMenuOpen());
    if (Pawn->IsMenuOpen()) Pawn->TogglePause();
    PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_LeftX,IE_Axis,.1f));
    Test->TestFalse(TEXT("Stick drift does not change the input type"),TrainInput::UsingGamepad(Pawn));
    PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_LeftX,IE_Axis,.65f));
    Test->TestTrue(TEXT("Meaningful analog input selects gamepad hints"),TrainInput::UsingGamepad(Pawn));
    Test->TestTrue(TEXT("Gamepad hint follows remapped LB button"),TrainInput::ActionKey(Pawn,TEXT("Interact"))==EKeys::Gamepad_LeftShoulder);
    Press(EKeys::Gamepad_LeftShoulder);
    Test->TestTrue(TEXT("Displayed remapped gamepad button really opens the station"),Pawn->IsMenuOpen());
    if (Pawn->IsMenuOpen()) Pawn->TogglePause();
    PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::MouseX,IE_Axis,3));
    Test->TestFalse(TEXT("Mouse motion returns to keyboard hints"),TrainInput::UsingGamepad(Pawn));
    Test->TestTrue(TEXT("Keyboard mapping remains F after changing devices"),TrainInput::ActionKey(Pawn,TEXT("Interact"))==EKeys::F);
    for (FKey Key:{EKeys::Gamepad_FaceButton_Bottom,EKeys::Gamepad_FaceButton_Right,EKeys::Gamepad_FaceButton_Left,EKeys::Gamepad_FaceButton_Top,
        EKeys::Gamepad_LeftShoulder,EKeys::Gamepad_RightShoulder,EKeys::Gamepad_LeftTrigger,EKeys::Gamepad_RightTrigger,
        EKeys::Gamepad_DPad_Up,EKeys::Gamepad_DPad_Down,EKeys::Gamepad_DPad_Left,EKeys::Gamepad_DPad_Right,EKeys::Gamepad_Special_Right})
    {
        FSlateBrush Brush; GetDefault<UTrainXboxGlyphs>()->TryGetInputBrush(Brush,Key);
        Test->TestNotNull(FString::Printf(TEXT("Xbox drawing exists for %s"),*Key.ToString()),Brush.GetResourceObject());
    }
    Test->TestTrue(TEXT("Menu and gameplay bindings may share a key"),Settings->Rebind(TEXT("PreviousTab"),false,EKeys::F));
    Test->TestTrue(TEXT("Hint provider returns the remapped tab shortcut"),Settings->Key(TEXT("PreviousTab"),false)==EKeys::F);
    Test->TestFalse(TEXT("Two tab actions cannot share a key"),Settings->Rebind(TEXT("NextTab"),false,EKeys::F));
    Test->TestTrue(TEXT("Interaction can use a mouse button"),Settings->Rebind(TEXT("Interact"),false,EKeys::LeftMouseButton));
    Pawn->RefreshControls();
    int32 BindingsForMouse=0;
    for (const auto& Binding:Pawn->InputComponent->KeyBindings) if (Binding.Chord.Key==EKeys::LeftMouseButton) ++BindingsForMouse;
    Test->TestEqual(TEXT("Remapping mouse input does not also trigger the attack fallback"),BindingsForMouse,1);
    Settings->Bindings=SavedBindings; Settings->KeyboardLayout=SavedLayout; Settings->SaveSettings(); Pawn->RefreshControls();
    return true;
}
bool FTrainHintTest::RunTest(const FString& Parameters)
{
    FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Maps/Wagon"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(.5));
    ADD_LATENT_AUTOMATION_COMMAND(FCheckTrainHints(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
