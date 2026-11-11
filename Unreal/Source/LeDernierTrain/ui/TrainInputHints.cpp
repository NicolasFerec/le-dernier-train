#include "TrainInputHints.h"
#include "../TrainSettings.h"
#include "CommonInputSubsystem.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    FText Text(const TCHAR* FR,const TCHAR* EN)
    { return FText::FromString(UTrainSettings::Get()->Language==TEXT("en") ? EN : FR); }
    FCommonInputKeyBrushConfiguration Glyph(FKey Key,const TCHAR* Name)
    {
        FCommonInputKeyBrushConfiguration Entry;
        Entry.Key=Key;
        Entry.KeyBrush.SetResourceObject(LoadObject<UTexture2D>(nullptr,*FString::Printf(TEXT("/Game/UI/InputPrompts/%s.%s"),Name,Name)));
        Entry.KeyBrush.DrawAs=ESlateBrushDrawType::Image;
        Entry.KeyBrush.ImageSize=FVector2D(32,32);
        return Entry;
    }
}
UTrainXboxGlyphs::UTrainXboxGlyphs()
{
    InputType=ECommonInputType::Gamepad; GamepadName=TEXT("Generic");
    InputBrushDataMap={
        Glyph(EKeys::Gamepad_FaceButton_Bottom,TEXT("xbox_button_color_a")),
        Glyph(EKeys::Gamepad_FaceButton_Right,TEXT("xbox_button_color_b")),
        Glyph(EKeys::Gamepad_FaceButton_Left,TEXT("xbox_button_color_x")),
        Glyph(EKeys::Gamepad_FaceButton_Top,TEXT("xbox_button_color_y")),
        Glyph(EKeys::Gamepad_LeftShoulder,TEXT("xbox_lb")),Glyph(EKeys::Gamepad_RightShoulder,TEXT("xbox_rb")),
        Glyph(EKeys::Gamepad_LeftTrigger,TEXT("xbox_lt")),Glyph(EKeys::Gamepad_RightTrigger,TEXT("xbox_rt")),
        Glyph(EKeys::Gamepad_LeftThumbstick,TEXT("xbox_stick_l_press")),Glyph(EKeys::Gamepad_RightThumbstick,TEXT("xbox_stick_r_press")),
        Glyph(EKeys::Gamepad_LeftStick_Up,TEXT("xbox_stick_l_up")),
        Glyph(EKeys::Gamepad_LeftStick_Down,TEXT("xbox_stick_l_down")),
        Glyph(EKeys::Gamepad_LeftStick_Left,TEXT("xbox_stick_l_left")),
        Glyph(EKeys::Gamepad_LeftStick_Right,TEXT("xbox_stick_l_right")),
        Glyph(EKeys::Gamepad_RightStick_Up,TEXT("xbox_stick_r_up")),
        Glyph(EKeys::Gamepad_RightStick_Down,TEXT("xbox_stick_r_down")),
        Glyph(EKeys::Gamepad_RightStick_Left,TEXT("xbox_stick_r_left")),
        Glyph(EKeys::Gamepad_RightStick_Right,TEXT("xbox_stick_r_right")),
        Glyph(EKeys::Gamepad_DPad_Up,TEXT("xbox_dpad_up")),Glyph(EKeys::Gamepad_DPad_Down,TEXT("xbox_dpad_down")),
        Glyph(EKeys::Gamepad_DPad_Left,TEXT("xbox_dpad_left")),Glyph(EKeys::Gamepad_DPad_Right,TEXT("xbox_dpad_right")),
        Glyph(EKeys::Gamepad_Special_Right,TEXT("xbox_button_menu")),Glyph(EKeys::Gamepad_Special_Left,TEXT("xbox_button_view"))};
}
UTrainMouseGlyphs::UTrainMouseGlyphs()
{
    InputType=ECommonInputType::MouseAndKeyboard;
    InputBrushDataMap={Glyph(EKeys::LeftMouseButton,TEXT("mouse_left")),Glyph(EKeys::RightMouseButton,TEXT("mouse_right")),
        Glyph(EKeys::MiddleMouseButton,TEXT("mouse_scroll")),Glyph(EKeys::MouseScrollUp,TEXT("mouse_scroll_up")),
        Glyph(EKeys::MouseScrollDown,TEXT("mouse_scroll_down")),Glyph(EKeys::ThumbMouseButton,TEXT("mouse_side_back")),Glyph(EKeys::ThumbMouseButton2,TEXT("mouse_side_forward"))};
}
bool TrainInput::UsingGamepad(const UObject* Context)
{
    const auto* PC=UGameplayStatics::GetPlayerController(Context,0);
    const auto* Input=PC ? UCommonInputSubsystem::Get(PC->GetLocalPlayer()) : nullptr;
    return Input && Input->GetCurrentInputType()==ECommonInputType::Gamepad;
}
FKey TrainInput::ActionKey(const UObject* Context,FName Action)
{ return UTrainSettings::Get()->Key(Action,UsingGamepad(Context)); }
FText TrainInput::KeyName(FKey Key)
{
    if (Key==EKeys::SpaceBar) return Text(TEXT("Espace"),TEXT("Space"));
    if (Key==EKeys::Escape) return Text(TEXT("Échap"),TEXT("Esc"));
    if (Key==EKeys::Enter) return Text(TEXT("Entrée"),TEXT("Enter"));
    if (Key==EKeys::BackSpace) return Text(TEXT("Retour"),TEXT("Backspace"));
    if (Key==EKeys::LeftShift || Key==EKeys::RightShift) return Text(TEXT("Maj"),TEXT("Shift"));
    if (Key==EKeys::LeftControl || Key==EKeys::RightControl) return FText::FromString(TEXT("Ctrl"));
    if (Key==EKeys::Gamepad_FaceButton_Bottom) return FText::FromString(TEXT("A"));
    if (Key==EKeys::Gamepad_FaceButton_Right) return FText::FromString(TEXT("B"));
    if (Key==EKeys::Gamepad_FaceButton_Left) return FText::FromString(TEXT("X"));
    if (Key==EKeys::Gamepad_FaceButton_Top) return FText::FromString(TEXT("Y"));
    if (Key==EKeys::Gamepad_LeftShoulder) return FText::FromString(TEXT("LB"));
    if (Key==EKeys::Gamepad_RightShoulder) return FText::FromString(TEXT("RB"));
    if (Key==EKeys::Gamepad_Special_Right) return FText::FromString(TEXT("Menu"));
    if (Key==EKeys::Gamepad_Special_Left) return Text(TEXT("Affichage"),TEXT("View"));
    if (Key==EKeys::Gamepad_DPad_Up || Key==EKeys::Up) return FText::FromString(TEXT("↑"));
    if (Key==EKeys::Gamepad_DPad_Down || Key==EKeys::Down) return FText::FromString(TEXT("↓"));
    if (Key==EKeys::Gamepad_DPad_Left || Key==EKeys::Left) return FText::FromString(TEXT("←"));
    if (Key==EKeys::Gamepad_DPad_Right || Key==EKeys::Right) return FText::FromString(TEXT("→"));
    return Key.GetDisplayName();
}
void STrainInputGlyph::RefreshBrush() const
{
    const FKey Key=BoundKey.Get();
    if (Key==CachedKey) return;
    CachedKey=Key; Icon=FSlateBrush();
    const UCommonInputBaseControllerData* Data=Key.IsGamepadKey()
        ? static_cast<const UCommonInputBaseControllerData*>(GetDefault<UTrainXboxGlyphs>())
        : static_cast<const UCommonInputBaseControllerData*>(GetDefault<UTrainMouseGlyphs>());
    Data->TryGetInputBrush(Icon,Key);
}
void STrainInputGlyph::Construct(const FArguments& Args)
{
    BoundKey=Args._Key; Selecting=Args._Selecting;
    Keycap=FSlateRoundedBoxBrush(FLinearColor(.035f,.045f,.055f),5.f,FLinearColor(.65f,.35f,.17f),1.2f);
    SetVisibility(EVisibility::HitTestInvisible);
    SetToolTipText(TAttribute<FText>::CreateLambda([this](){return TrainInput::KeyName(BoundKey.Get());}));
    ChildSlot[SNew(SBox).MinDesiredWidth(Args._Size).HeightOverride(Args._Size)
        [SNew(SOverlay)
            +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
                [SNew(SBox).WidthOverride(Args._Size).HeightOverride(Args._Size)
                    [SNew(SImage).Image_Lambda([this](){RefreshBrush();return &Icon;})
                        .Visibility_Lambda([this](){RefreshBrush();return !Selecting.Get() && Icon.GetResourceObject() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;})]]
            +SOverlay::Slot()
                [SNew(SBorder).BorderImage(&Keycap).Padding(FMargin(8,2)).HAlign(HAlign_Center).VAlign(VAlign_Center)
                    .Visibility_Lambda([this](){RefreshBrush();return Selecting.Get() || !Icon.GetResourceObject() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;})
                    [SNew(STextBlock).ColorAndOpacity(FLinearColor(.96f,.86f,.7f))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold",14))
                        .Text_Lambda([this](){return Selecting.Get() ? FText::FromString(TEXT("…")) : TrainInput::KeyName(BoundKey.Get());})]]]];
}
