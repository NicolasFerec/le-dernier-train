#include "TrainPrototype.h"
#include "TrainSettings.h"
#include "progression/TrainProfile.h"
#include "ui/TrainInputHints.h"
#include "CommonInputSubsystem.h"
#include "network/TrainSession.h"
#include "network/TrainNetwork.h"
#include "GameFramework/GameStateBase.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericApplication.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SInputKeySelector.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    const FLinearColor Ivory(.94f, .84f, .67f);
    const FLinearColor Copper(.65f, .35f, .17f);
    const TArray<int32> Limits { -1, 30, 60, 75, 90, 120, 144, 165, 180, 240, 0 };
    FText T(const TCHAR* French, const TCHAR* English)
    {
        return FText::FromString(UTrainSettings::Get()->Language == TEXT("en") ? English : French);
    }
    TSharedRef<STextBlock> Label(FText Text, int32 Size = 16)
    {
        return SNew(STextBlock).Text(Text).ColorAndOpacity(Ivory)
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", Size)).AutoWrapText(true);
    }
    FString QualityName(int32 Level)
    {
        const TCHAR* FR[] { TEXT("Faible"), TEXT("Moyenne"), TEXT("Élevée"), TEXT("Épique"), TEXT("Cinématique") };
        const TCHAR* EN[] { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic"), TEXT("Cinematic") };
        return Level < 0 ? T(TEXT("Personnalisée"), TEXT("Custom")).ToString()
            : (UTrainSettings::Get()->Language == TEXT("en") ? EN : FR)[FMath::Clamp(Level, 0, 4)];
    }
}

TSharedRef<SWidget> UTrainMenu::RebuildWidget()
{
    SetIsFocusable(true);
    FirstFocus.Reset();
    if (auto* Session=GetGameInstance<UTrainSession>(); Session && !SessionListener.IsValid())
        SessionListener=Session->Changed.AddWeakLambda(this,[this](){ if (SessionMode) Refresh(); });
    Backdrop = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Art/depot_panoramique.depot_panoramique"));
    BackgroundBrush.SetResourceObject(Backdrop);
    BackgroundBrush.ImageSize = FVector2D(2400, 675);
    BackgroundBrush.DrawAs = ESlateBrushDrawType::Image;
    BackgroundBrush.TintColor = FSlateColor(FLinearColor(.68f, .68f, .72f));
    BorderBrush = FSlateColorBrush(Copper);
    PanelBrush = FSlateColorBrush(FLinearColor(.009f, .013f, .020f, .98f));
    DimBrush = FSlateColorBrush(FLinearColor(0, 0, 0, .48f));
    const FSlateColorBrush ButtonBrush = FSlateColorBrush(FLinearColor(.025f, .029f, .036f));
    ButtonStyle = FButtonStyle().SetNormal(ButtonBrush)
        .SetHovered(FSlateColorBrush(FLinearColor(.17f, .046f, .038f)))
        .SetPressed(FSlateColorBrush(FLinearColor(.27f, .09f, .05f)));
    BindingTextStyle = FTextBlockStyle().SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 15))
        .SetColorAndOpacity(FLinearColor::Transparent);
    ActiveTabStyle = ButtonStyle;
    ActiveTabStyle.SetNormal(PanelBrush);
    FSlateColorBrush Thumb(Ivory); Thumb.ImageSize = FVector2D(16, 16);
    FSlateColorBrush Bar(Copper); Bar.ImageSize = FVector2D(1, 4);
    SliderStyle = FSliderStyle().SetNormalBarImage(Bar).SetHoveredBarImage(Bar)
        .SetNormalThumbImage(Thumb).SetHoveredThumbImage(Thumb).SetDisabledThumbImage(Thumb).SetBarThickness(4);
    SAssignNew(RootHost, SBox)[BuildContent()];
    FocusFirst();
    return RootHost.ToSharedRef();
}

void UTrainMenu::FocusFirst()
{
    if (RootHost)
        RootHost->RegisterActiveTimer(.1f, FWidgetActiveTimerDelegate::CreateWeakLambda(this,
            [this](double, float)
            {
                if (auto Widget = FirstFocus.Pin())
                    if (FSlateApplication::Get().SetKeyboardFocus(Widget, EFocusCause::SetDirectly))
                        return EActiveTimerReturnType::Stop;
                return EActiveTimerReturnType::Continue;
            }));
}

void UTrainMenu::Refresh()
{
    FirstFocus.Reset();
    if (RootHost)
        RootHost->SetContent(BuildContent());
    FocusFirst();
}

void UTrainMenu::Resume()
{
    if (auto* Pawn = Cast<ATrainPawn>(GetOwningPlayerPawn()))
        Pawn->TogglePause();
}

TSharedRef<SWidget> UTrainMenu::BuildContent()
{
    auto Overlay = SNew(SOverlay);
    if (!PauseMode && !SessionMode && !SummaryMode && Station<0)
        Overlay->AddSlot()[SNew(SScaleBox).Stretch(EStretch::ScaleToFill)
            .Clipping(EWidgetClipping::ClipToBounds)[SNew(SImage).Image(&BackgroundBrush)]];
    if (PauseMode || SessionMode || SummaryMode || Station>=0 || Page == 1)
        Overlay->AddSlot()[SNew(SImage).Image(&DimBrush).Visibility(EVisibility::HitTestInvisible)];

    if ((SessionMode || SummaryMode || Station>=0) && Page == 0)
    {
        Overlay->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Center)
            [SNew(SBox).WidthOverride(SummaryMode ? 900 : 760).MaxDesiredHeight(700)
                [SNew(SBorder).BorderImage(&BorderBrush).Padding(2)
                    [SNew(SBorder).BorderImage(&PanelBrush).Padding(26)[SummaryMode ? BuildSummary() : Station>=0 ? BuildStation() : BuildSession()]]]];
        return Overlay;
    }
    if (Page == 1)
    {
        Overlay->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Center)
            [SNew(SBox).WidthOverride(860).MaxDesiredHeight(670)
                [SNew(SBorder).BorderImage(&BorderBrush).Padding(2)
                    [SNew(SBorder).BorderImage(&PanelBrush).Padding(28)[BuildSettings()]]]];
        return Overlay;
    }
    auto Column = SNew(SVerticalBox);
    const FString Font = FPaths::ProjectContentDir() / TEXT("Fonts/Cinzel.ttf");
    Column->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 14)
        [SNew(STextBlock).Text(PauseMode ? T(TEXT("Pause"), TEXT("Paused")) : FText::FromString(TEXT("Le Dernier Train")))
            .Font(FSlateFontInfo(MakeShared<FCompositeFont>(TEXT("TrainTitle"), Font, EFontHinting::Default, EFontLoadingPolicy::LazyLoad), PauseMode ? 38 : 53)).ColorAndOpacity(Ivory)
            .ShadowOffset(FVector2D(2, 3)).ShadowColorAndOpacity(FLinearColor::Black)];
    Column->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 22)
        [SNew(STextBlock).ColorAndOpacity(Ivory).Font(FCoreStyle::GetDefaultFontStyle("Regular",13))
            .Text_Lambda([this]()
            {
                if (!PauseMode) return T(TEXT("◆   LES FAUBOURGS NOYÉS   ◆"),TEXT("◆   THE DROWNED OUTSKIRTS   ◆"));
                const auto* GI=GetGameInstance<UTrainSession>();
                FString Value=GetWorld()->GetNetMode()==NM_Standalone ? T(TEXT("Solo"),TEXT("Solo")).ToString()
                    : (GI && GI->Mode==TEXT("Steam") ? T(TEXT("En ligne · Steam"),TEXT("Online · Steam")).ToString()
                        : T(TEXT("Coop locale · Développement"),TEXT("Local co-op · Development")).ToString());
                if (const auto* GS=GetWorld()->GetGameState()) for (APlayerState* PS:GS->PlayerArray)
                    Value+=FString::Printf(TEXT("\n%s · %.0f ms"),*PS->GetPlayerName(),PS->GetPingInMilliseconds());
                return FText::FromString(Value);
            })];
    Column->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0,0,0,12)
        [SNew(STextBlock).AutoWrapText(true).WrapTextAt(620).ColorAndOpacity(Ivory).Text_Lambda([this]()
            { const auto* Session=GetGameInstance<UTrainSession>(); return Session ? Session->Status : FText::GetEmpty(); })];
    auto Button = [this, Column](FText Text, TFunction<FReply()> Click)
    {
        auto Item = SNew(SButton).ButtonStyle(&ButtonStyle).HAlign(HAlign_Center).VAlign(VAlign_Center)
            .OnClicked_Lambda(MoveTemp(Click))[Label(Text, 18)];
        if (!FirstFocus.IsValid())
            FirstFocus = Item;
        Column->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 12)
            [SNew(SBox).WidthOverride(320).HeightOverride(50)
                [SNew(SBorder).BorderImage(&BorderBrush).Padding(2)[Item]]];
    };
    Button(PauseMode ? T(TEXT("Reprendre"), TEXT("Resume")) : T(TEXT("Monter à bord"), TEXT("Board the train")),
        [this]()
        {
            if (PauseMode) Resume();
            else if (auto* Session=GetGameInstance<UTrainSession>()) Session->Solo();
            return FReply::Handled();
        });
    Button(T(TEXT("Réglages"), TEXT("Settings")), [this]() { Page = 1; Refresh(); return FReply::Handled(); });
    if (PauseMode)
        Button(T(TEXT("Menu principal"), TEXT("Main menu")), [this]()
        {
            if (auto* Session=GetGameInstance<UTrainSession>()) Session->Leave();
            return FReply::Handled();
        });
    Button(T(TEXT("Quitter le jeu"), TEXT("Quit game")), [this]()
    {
        UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
        return FReply::Handled();
    });
    Overlay->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Center)[Column];
    return Overlay;
}

TSharedRef<SWidget> UTrainMenu::BuildSettings()
{
    auto* Settings = UTrainSettings::Get();
    auto Apply = [this, Settings]() { Settings->ApplyRuntime(GetWorld(), true); };
    auto Main = SNew(SVerticalBox);
    Main->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 16)
        [Label(T(TEXT("Réglages"), TEXT("Settings")), 29)];
    auto Tabs = SNew(SHorizontalBox);
    const FText Names[] { T(TEXT("Audio"), TEXT("Audio")), T(TEXT("Graphismes"), TEXT("Graphics")),
        T(TEXT("Général"), TEXT("General")), T(TEXT("Commandes"), TEXT("Controls")) };
    for (int32 Index = 0; Index < 4; ++Index)
        Tabs->AddSlot().FillWidth(1).Padding(0, 0, 3, 0)
            [SNew(SBorder).BorderImage(&BorderBrush).Padding(FMargin(1, 1, 1, Index == Tab ? 0 : 1))
                [SNew(SButton).ButtonStyle(Index == Tab ? &ActiveTabStyle : &ButtonStyle).HAlign(HAlign_Center)
                    .OnClicked_Lambda([this, Index]() { Tab = Index; Refresh(); return FReply::Handled(); })
                    [Label(Names[Index], 17)]]];
    Main->AddSlot().AutoHeight()[Tabs];
    Main->AddSlot().AutoHeight().HAlign(HAlign_Right).Padding(0,7)
        [SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [SNew(STrainInputGlyph).Size(27).Key_Lambda([this](){return TrainInput::ActionKey(this,TEXT("PreviousTab"));})]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(7,0,18,0)
                [SNew(STextBlock).Text(T(TEXT("Onglet précédent"),TEXT("Previous tab"))).ColorAndOpacity(Ivory).Font(FCoreStyle::GetDefaultFontStyle("Regular",12))]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [SNew(STrainInputGlyph).Size(27).Key_Lambda([this](){return TrainInput::ActionKey(this,TEXT("NextTab"));})]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(7,0,0,0)
                [SNew(STextBlock).Text(T(TEXT("Onglet suivant"),TEXT("Next tab"))).ColorAndOpacity(Ivory).Font(FCoreStyle::GetDefaultFontStyle("Regular",12))]];
    auto Rows = SNew(SVerticalBox);
    auto Row = [Rows](FText Text, TSharedRef<SWidget> Control)
    {
        Rows->AddSlot().AutoHeight().Padding(0, 6)
            [SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(.48f).VAlign(VAlign_Center).Padding(0, 0, 20, 0)[Label(Text)]
                + SHorizontalBox::Slot().FillWidth(.52f).VAlign(VAlign_Center)[Control]];
    };
    auto Focus = [this](TSharedRef<SWidget> Widget) -> TSharedRef<SWidget>
    {
        if (!FirstFocus.IsValid() && Widget->IsEnabled())
            FirstFocus = Widget;
        return Widget;
    };
    auto Toggle = [Focus](TAttribute<ECheckBoxState> Checked, TFunction<void(ECheckBoxState)> Changed)
    {
        return Focus(SNew(SCheckBox).IsChecked(Checked).OnCheckStateChanged_Lambda(MoveTemp(Changed)));
    };
    using FChoice = TPair<FText, TFunction<void()>>;
    auto Select = [this, Focus](TAttribute<FText> Current, TArray<FChoice> Choices, bool Enabled = true)
    {
        auto Control = SNew(SComboButton).IsEnabled(Enabled).ContentPadding(FMargin(9, 5))
            .OnGetMenuContent_Lambda([Choices]()
            {
                auto Menu = SNew(SVerticalBox);
                for (const auto& Choice : Choices)
                    Menu->AddSlot().AutoHeight()[SNew(SButton)
                        .OnClicked_Lambda([Choice]()
                        {
                            FSlateApplication::Get().DismissAllMenus();
                            Choice.Value();
                            return FReply::Handled();
                        })[Label(Choice.Key)]];
                return SNew(SBox).MinDesiredWidth(200).MaxDesiredHeight(350)[SNew(SScrollBox)
                    + SScrollBox::Slot()[Menu]];
            }).ButtonContent()[SNew(STextBlock).Text(Current).ColorAndOpacity(Ivory).Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))];
        return Focus(Control);
    };
    auto Slider = [this, Focus](TAttribute<float> Value, TFunction<void(float)> Change, float Step, TAttribute<FText> Display)
    {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                [Focus(SNew(SSlider).Style(&SliderStyle).Value(Value).StepSize(Step).OnValueChanged_Lambda(MoveTemp(Change)))]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12, 0, 0, 0)
                [SNew(SBox).WidthOverride(100)[SNew(STextBlock).Text(Display).ColorAndOpacity(Ivory)]];
    };
    if (Tab == 0)
    {
        struct FVolume { FText Name; float UTrainSettings::* Member; };
        for (const auto& Volume : { FVolume{T(TEXT("Volume général"), TEXT("Master volume")), &UTrainSettings::MasterVolume},
            FVolume{T(TEXT("Musique"), TEXT("Music")), &UTrainSettings::MusicVolume},
            FVolume{T(TEXT("Effets sonores"), TEXT("Sound effects")), &UTrainSettings::EffectsVolume} })
        {
            auto Member = Volume.Member;
            Row(Volume.Name, Slider(TAttribute<float>::CreateLambda([Settings, Member]() { return Settings->*Member; }),
                [Settings, Member, Apply](float Value) { Settings->*Member = Value; Apply(); }, .01f,
                TAttribute<FText>::CreateLambda([Settings, Member]() { return FText::AsPercent(Settings->*Member); })));
        }
    }
    else if (Tab == 1)
    {
        FDisplayMetrics Metrics;
        FDisplayMetrics::RebuildDisplayMetrics(Metrics);
        const bool CanMove = FPlatformMisc::GetEnvironmentVariable(TEXT("XDG_SESSION_TYPE")) != TEXT("wayland");
        TArray<FChoice> Monitors;
        for (int32 Index = 0; Index < Metrics.MonitorInfo.Num(); ++Index)
        {
            auto Info = Metrics.MonitorInfo[Index];
            Monitors.Add({ FText::FromString(FString::Printf(TEXT("%d · %s"), Index + 1, *Info.Name)),
                [this, Settings, Info, Index]()
                {
                    Settings->Monitor = Index;
                    Settings->SetDisplayProperties(Info.ID, Index);
                    Settings->ApplyResolutionSettings(false);
                    Settings->ConfirmVideoMode();
                    if (auto Window = GetWorld()->GetGameViewport()->GetWindow())
                        Window->MoveWindowTo(FVector2D(Info.DisplayRect.Left, Info.DisplayRect.Top));
                    Settings->SaveSettings();
                    Refresh();
                } });
        }
        Row(T(TEXT("Écran"), TEXT("Display")), Select(
            TAttribute<FText>::CreateLambda([Settings, CanMove]()
            {
                if (CanMove && Settings->Monitor < 0) return T(TEXT("Automatique"),TEXT("Automatic"));
                return CanMove ? FText::Format(T(TEXT("Écran {0}"), TEXT("Display {0}")), FText::AsNumber(Settings->Monitor + 1))
                    : T(TEXT("Géré par le système"), TEXT("Managed by the system"));
            }), Monitors, CanMove && Monitors.Num() > 1));
        Row(T(TEXT("Plein écran"), TEXT("Fullscreen")), Toggle(
            TAttribute<ECheckBoxState>::CreateLambda([Settings]()
            { return Settings->GetFullscreenMode() == EWindowMode::Windowed ? ECheckBoxState::Unchecked : ECheckBoxState::Checked; }),
            [this, Settings, Apply](ECheckBoxState Value)
            {
                Settings->SetFullscreenMode(Value == ECheckBoxState::Checked ? EWindowMode::Fullscreen : EWindowMode::Windowed);
                Settings->ApplyResolutionSettings(false); Settings->ConfirmVideoMode(); Apply(); Refresh();
            }));
        auto Native = Settings->GetDesktopResolution();
        if (Metrics.MonitorInfo.IsValidIndex(Settings->Monitor))
            Native = FIntPoint(Metrics.MonitorInfo[Settings->Monitor].NativeWidth, Metrics.MonitorInfo[Settings->Monitor].NativeHeight);
        TArray<FChoice> Resolutions;
        const FIntPoint Common[] { {1280,720},{1280,800},{1600,900},{1600,1000},{1920,1080},{1920,1200},
            {2560,1440},{2560,1600},{2560,1080},{3440,1440},{3840,1080},{3840,1600},{3840,2160},{5120,1440},{7680,2160} };
        TArray<FIntPoint> Sizes { Native };
        for (auto Size : Common)
            if (Size.X <= Native.X && Size.Y <= Native.Y && Native.Y > 0
                && FMath::Abs((double(Size.X) / Size.Y) / (double(Native.X) / Native.Y) - 1) < .02)
                Sizes.AddUnique(Size);
        for (auto Size : Sizes)
            Resolutions.Add({ FText::FromString(FString::Printf(TEXT("%d × %d"), Size.X, Size.Y)), [Settings, Size, Apply]()
            {
                Settings->SetScreenResolution(Size); Settings->ApplyResolutionSettings(false);
                Settings->ConfirmVideoMode(); Apply();
            } });
        Row(T(TEXT("Résolution plein écran"), TEXT("Fullscreen resolution")), Select(
            TAttribute<FText>::CreateLambda([Settings]()
            { auto Size = Settings->GetScreenResolution(); return FText::FromString(FString::Printf(TEXT("%d × %d"), Size.X, Size.Y)); }),
            Resolutions, Settings->GetFullscreenMode() != EWindowMode::Windowed));
        Row(T(TEXT("VSync"), TEXT("VSync")), Toggle(
            TAttribute<ECheckBoxState>::CreateLambda([Settings]() { return Settings->IsVSyncEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }),
            [Settings, Apply](ECheckBoxState Value) { Settings->SetVSyncEnabled(Value == ECheckBoxState::Checked); Apply(); }));
        Row(T(TEXT("Compteur d’IPS"), TEXT("FPS counter")), Toggle(
            TAttribute<ECheckBoxState>::CreateLambda([Settings]() { return Settings->ShowFPS ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }),
            [Settings, Apply](ECheckBoxState Value) { Settings->ShowFPS = Value == ECheckBoxState::Checked; Apply(); }));
        Row(T(TEXT("Limite d’IPS en jeu"), TEXT("In-game FPS limit")), Slider(
            TAttribute<float>::CreateLambda([Settings]() { return FMath::Max(0, Limits.Find(Settings->GameFPS)) / float(Limits.Num() - 1); }),
            [Settings, Apply](float Value) { Settings->GameFPS = Limits[FMath::RoundToInt(Value * (Limits.Num() - 1))]; Apply(); },
            1.0f / (Limits.Num() - 1), TAttribute<FText>::CreateLambda([Settings]()
            {
                if (Settings->GameFPS == 0) return T(TEXT("Sans limite"), TEXT("Unlimited"));
                if (Settings->GameFPS == -1) return T(TEXT("Automatique"), TEXT("Automatic"));
                return FText::AsNumber(Settings->GameFPS);
            })));
        Rows->AddSlot().AutoHeight().Padding(0, 7)[Label(T(
            TEXT("La VSync peut limiter les IPS à la fréquence de l’écran, même avec « Sans limite »."),
            TEXT("VSync can limit FPS to the display refresh rate, even with Unlimited.")), 13)];
        TArray<FChoice> Quality;
        for (int32 Level = 0; Level <= 3; ++Level)
            Quality.Add({FText::FromString(QualityName(Level)), [Settings, Apply, Level]() { Settings->SetOverallScalabilityLevel(Level); Apply(); }});
        Row(T(TEXT("Qualité globale"), TEXT("Overall quality")), Select(
            TAttribute<FText>::CreateLambda([Settings]() { return FText::FromString(QualityName(Settings->GetOverallScalabilityLevel())); }), Quality));
        struct FGroup { const TCHAR* FR; const TCHAR* EN; int32 (UGameUserSettings::*Get)() const; void (UGameUserSettings::*Set)(int32); };
        const FGroup Groups[] {
            {TEXT("Ombres"),TEXT("Shadows"),&UGameUserSettings::GetShadowQuality,&UGameUserSettings::SetShadowQuality},
            {TEXT("Textures"),TEXT("Textures"),&UGameUserSettings::GetTextureQuality,&UGameUserSettings::SetTextureQuality},
            {TEXT("Effets"),TEXT("Effects"),&UGameUserSettings::GetVisualEffectQuality,&UGameUserSettings::SetVisualEffectQuality},
            {TEXT("Anticrénelage"),TEXT("Anti-aliasing"),&UGameUserSettings::GetAntiAliasingQuality,&UGameUserSettings::SetAntiAliasingQuality},
            {TEXT("Post-traitement"),TEXT("Post processing"),&UGameUserSettings::GetPostProcessingQuality,&UGameUserSettings::SetPostProcessingQuality},
            {TEXT("Distance d’affichage"),TEXT("View distance"),&UGameUserSettings::GetViewDistanceQuality,&UGameUserSettings::SetViewDistanceQuality} };
        for (const auto& Group : Groups)
        {
            TArray<FChoice> Choices;
            for (int32 Level = 0; Level <= 3; ++Level)
                Choices.Add({FText::FromString(QualityName(Level)), [Settings, Apply, Group, Level]() { (Settings->*Group.Set)(Level); Apply(); }});
            Row(T(Group.FR, Group.EN), Select(TAttribute<FText>::CreateLambda(
                [Settings, Group]() { return FText::FromString(QualityName((Settings->*Group.Get)())); }), Choices));
        }
    }
    else if (Tab == 2)
    {
        Row(T(TEXT("Langue"), TEXT("Language")), Select(FText::FromString(Settings->Language == TEXT("en") ? TEXT("English") : TEXT("Français")),
            { { FText::FromString(TEXT("Français")), [this, Settings]() { Settings->SetLanguage(TEXT("fr"), GetWorld()); Refresh(); } },
              { FText::FromString(TEXT("English")), [this, Settings]() { Settings->SetLanguage(TEXT("en"), GetWorld()); Refresh(); } } }));
        Row(T(TEXT("Clavier"), TEXT("Keyboard")), Select(
            FText::FromString(Settings->KeyboardLayout == 2 ? TEXT("QWERTY · WASD") : TEXT("AZERTY · ZQSD")),
            { { FText::FromString(TEXT("AZERTY · ZQSD")), [this, Settings]() { Settings->KeyboardLayout = 1; Settings->SaveSettings(); Refresh(); } },
              { FText::FromString(TEXT("QWERTY · WASD")), [this, Settings]() { Settings->KeyboardLayout = 2; Settings->SaveSettings(); Refresh(); } } }));
        const FString Channel = Settings->GetUpdateChannel();
        if (!Channel.IsEmpty())
        {
            Row(T(TEXT("Canal de mise à jour"), TEXT("Update channel")),
                Select(FText::FromString(Channel == TEXT("dev") ? TEXT("Dev") : TEXT("Stable")),
                    { { FText::FromString(TEXT("Stable")), [this, Settings]() { Settings->SetUpdateChannel(TEXT("stable")); Refresh(); } },
                      { FText::FromString(TEXT("Dev")), [this, Settings]() { Settings->SetUpdateChannel(TEXT("dev")); Refresh(); } } }));
            Rows->AddSlot().AutoHeight().Padding(0, 15)[Label(T(
                TEXT("Ce choix s’applique au prochain lancement."),
                TEXT("This choice applies the next time you start the game.")), 13)];
        }
    }
    else
    {
        Row(T(TEXT("Vibrations de la manette"), TEXT("Controller vibration")), Toggle(
            TAttribute<ECheckBoxState>::CreateLambda([Settings]() { return Settings->Vibration ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }),
            [this, Settings](ECheckBoxState Value)
            {
                Settings->Vibration = Value == ECheckBoxState::Checked; Settings->SaveSettings();
                GetOwningPlayer()->SetDisableHaptics(!Settings->Vibration);
                if (!Settings->Vibration) GetOwningPlayer()->ClientStopForceFeedback(nullptr, NAME_None);
            }));
        auto Header = SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(.44f)[Label(T(TEXT("Action"), TEXT("Action")))]
            + SHorizontalBox::Slot().FillWidth(.28f)[Label(T(TEXT("Clavier"), TEXT("Keyboard")))]
            + SHorizontalBox::Slot().FillWidth(.28f)[Label(T(TEXT("Manette"), TEXT("Controller")))];
        Rows->AddSlot().AutoHeight().Padding(0, 10)[Header];
        const FName Actions[] { TEXT("MoveUp"),TEXT("MoveDown"),TEXT("MoveLeft"),TEXT("MoveRight"),TEXT("Attack"),TEXT("Interact"),TEXT("Dash"),TEXT("HeavyAttack"),TEXT("PreviousTab"),TEXT("NextTab") };
        const FText ActionNames[] {T(TEXT("Monter"),TEXT("Move up")),T(TEXT("Descendre"),TEXT("Move down")),
            T(TEXT("Aller à gauche"),TEXT("Move left")),T(TEXT("Aller à droite"),TEXT("Move right")),
            T(TEXT("Frapper"),TEXT("Attack")),T(TEXT("Interagir"),TEXT("Interact")),T(TEXT("Esquiver"),TEXT("Dodge")),T(TEXT("Frappe lourde"),TEXT("Heavy attack")),T(TEXT("Onglet précédent"),TEXT("Previous tab")),T(TEXT("Onglet suivant"),TEXT("Next tab"))};
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Actions); ++Index)
        {
            auto Line = SNew(SHorizontalBox);
            Line->AddSlot().FillWidth(.44f).VAlign(VAlign_Center)[Label(ActionNames[Index])];
            FName Action = Actions[Index];
            for (bool Pad : { false, true })
            {
                auto Selector = SNew(SInputKeySelector).TextStyle(&BindingTextStyle).ButtonStyle(&ButtonStyle)
                    .SelectedKey_Lambda([Settings, Action, Pad]() { return FInputChord(Settings->Key(Action, Pad)); })
                    .AllowModifierKeys(false).AllowGamepadKeys(true)
                    .KeySelectionText(T(TEXT("Appuie sur une touche…"),TEXT("Press a key…")))
                    .EscapeKeys({EKeys::Escape,EKeys::Gamepad_Special_Left})
                    .OnIsSelectingKeyChanged_Lambda([this]() { bSelectingKey = !bSelectingKey; })
                    .OnKeySelected_Lambda([this, Settings, Action, Pad](FInputChord Chord)
                    {
                        BindingNotice = Settings->Rebind(Action, Pad, Chord.Key) ? FString()
                            : T(TEXT("Touche réservée, déjà utilisée ou périphérique incorrect."),
                                TEXT("Reserved key, existing binding or wrong device.")).ToString();
                    });
                Line->AddSlot().FillWidth(.28f).Padding(4, 0)
                    [SNew(SBorder).BorderImage(&BorderBrush).Padding(1)
                        [SNew(SOverlay)
                            + SOverlay::Slot()[Focus(Selector)]
                            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
                                [SNew(STrainInputGlyph).Size(30)
                                    .Key_Lambda([Settings,Action,Pad](){return Settings->Key(Action,Pad);})
                                    .Selecting_Lambda([Selector](){return Selector->GetIsSelectingKey();})]]];
            }
            Rows->AddSlot().AutoHeight().Padding(0, 5)[Line];
        }
        Rows->AddSlot().AutoHeight().Padding(0,5)
            [SNew(SHorizontalBox)
                +SHorizontalBox::Slot().FillWidth(.44f).VAlign(VAlign_Center)[Label(T(TEXT("Pause"),TEXT("Pause")))]
                +SHorizontalBox::Slot().FillWidth(.28f).HAlign(HAlign_Center)[SNew(STrainInputGlyph).Key(Settings->Key(TEXT("Pause"),false))]
                +SHorizontalBox::Slot().FillWidth(.28f).HAlign(HAlign_Center)[SNew(STrainInputGlyph).Key(Settings->Key(TEXT("Pause"),true))]];
        Rows->AddSlot().AutoHeight().Padding(0, 10)[SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(BindingNotice); }).ColorAndOpacity(Ivory).AutoWrapText(true)];
        Rows->AddSlot().AutoHeight()[SNew(SButton).ButtonStyle(&ButtonStyle)
            .OnClicked_Lambda([Settings]() { Settings->ResetBindings(); return FReply::Handled(); })
            [Label(T(TEXT("Rétablir les touches"),TEXT("Reset bindings")))]];
    }
    Main->AddSlot().FillHeight(1).Padding(0, 6)[SNew(SBox).MinDesiredHeight(280).MaxDesiredHeight(400)
        [SNew(SScrollBox) + SScrollBox::Slot()[Rows]]];
    Main->AddSlot().AutoHeight().Padding(0, 15, 0, 0)
        [SNew(SBorder).BorderImage(&BorderBrush).Padding(2)
            [SNew(SButton).ButtonStyle(&ButtonStyle).HAlign(HAlign_Center).ContentPadding(FMargin(15, 10))
                .OnClicked_Lambda([this]() { Page = 0; Refresh(); return FReply::Handled(); })
                [Label(T(TEXT("Retour"),TEXT("Back")), 18)]]];
    return Main;
}

void UTrainMenu::ChangeTab(int32 Delta)
{
    Tab = (Tab + Delta + 4) % 4;
    Refresh();
}

FReply UTrainMenu::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    const FKey Key = Event.GetKey();
    if (auto* Input=UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
        Input->SetCurrentInputType(Key.IsGamepadKey() ? ECommonInputType::Gamepad : ECommonInputType::MouseAndKeyboard);
    const auto* Settings=UTrainSettings::Get();
    const bool Pad=Key.IsGamepadKey();
    if (!bSelectingKey && (Key == Settings->Key(TEXT("Pause"),Pad) || Key == Settings->Key(TEXT("MenuBack"),Pad)))
    {
        if (Page == 1) { Page = 0; Refresh(); }
        else if (PauseMode || SessionMode || Station>=0) Resume();
        return FReply::Handled();
    }
    if (!bSelectingKey && Page == 1 && !Event.IsRepeat())
    {
        if (Key == Settings->Key(TEXT("PreviousTab"),Pad)) { ChangeTab(-1); return FReply::Handled(); }
        if (Key == Settings->Key(TEXT("NextTab"),Pad)) { ChangeTab(1); return FReply::Handled(); }
    }
    return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

FReply UTrainMenu::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    return Super::NativeOnKeyDown(Geometry, Event);
}

FReply UTrainMenu::NativeOnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event)
{
    if (Event.GetKey().IsGamepadKey() && FMath::Abs(Event.GetAnalogValue())>.24f)
        if (auto* Input=UCommonInputSubsystem::Get(GetOwningLocalPlayer())) Input->SetCurrentInputType(ECommonInputType::Gamepad);
    return Super::NativeOnAnalogValueChanged(Geometry, Event);
}

void UTrainMenu::NativeDestruct()
{
    if (auto* Session=GetGameInstance<UTrainSession>()) Session->Changed.Remove(SessionListener);
    SessionListener.Reset();
    Super::NativeDestruct();
}

TSharedRef<SWidget> UTrainMenu::BuildSession()
{
    auto Column=SNew(SVerticalBox); auto* Session=GetGameInstance<UTrainSession>();
    Column->AddSlot().AutoHeight().Padding(0,0,0,18)[Label(T(TEXT("L’équipage"),TEXT("The crew")),29)];
    Column->AddSlot().AutoHeight().Padding(0,0,0,12)[SNew(STextBlock).ColorAndOpacity(Ivory).Text_Lambda([this]()
    {
        auto* GI=GetGameInstance<UTrainSession>(); const auto* GS=GetWorld()->GetGameState<ATrainGameState>();
        FString Value=GI && GI->Mode==TEXT("Steam") ? T(TEXT("En ligne · Steam"),TEXT("Online · Steam")).ToString()
            : (GetWorld()->GetNetMode()==NM_Standalone ? T(TEXT("Solo"),TEXT("Solo")).ToString() : T(TEXT("Coop locale · Développement"),TEXT("Local co-op · Development")).ToString());
        if (GS) for (APlayerState* State:GS->PlayerArray)
            if (const auto* PS=Cast<ATrainPlayerState>(State)) Value+=FString::Printf(TEXT("\n%s%s   ·   %.0f ms"),*PS->GetPlayerName(),
                PS->IsHost ? *T(TEXT(" (hôte)"),TEXT(" (host)")).ToString() : TEXT(""),PS->GetPingInMilliseconds());
        return FText::FromString(Value);
    })];
    auto Button=[this,Column](FText Text,TFunction<void()> Action,bool Enabled=true)
    {
        auto Item=SNew(SButton).ButtonStyle(&ButtonStyle).HAlign(HAlign_Center).ContentPadding(FMargin(12,8)).IsEnabled(Enabled)
            .OnClicked_Lambda([Action](){Action();return FReply::Handled();})[Label(Text,17)];
        if (!FirstFocus.IsValid() && Enabled) FirstFocus=Item;
        Column->AddSlot().AutoHeight().Padding(0,5)[SNew(SBorder).BorderImage(&BorderBrush).Padding(1)[Item]];
    };
    if (Session)
    {
        const bool Available=Session->SteamAvailable();
        Button(Session->Mode==TEXT("Steam") ? T(TEXT("Inviter des amis Steam"),TEXT("Invite Steam friends"))
            : T(TEXT("Ouvrir un salon Steam"),TEXT("Open a Steam lobby")), [Session]()
                { if (Session->Mode==TEXT("Steam")) Session->InviteFriends(); else Session->HostSteam(); },
                Available && !Session->IsBusy() && (Session->Mode==TEXT("Steam") || GetWorld()->GetNetMode()==NM_Standalone));
        if (!Available) Column->AddSlot().AutoHeight().Padding(0,5)[Label(T(TEXT("Steam n’est pas disponible. Ouvre Steam et relance le jeu pour inviter tes amis."),
            TEXT("Steam is unavailable. Open Steam and restart the game to invite friends.")),14)];
        if (GetWorld()->GetNetMode()==NM_Standalone)
        {
            Button(T(TEXT("Rechercher des salons Steam"),TEXT("Find Steam lobbies")),[Session](){Session->FindSteamGames();},Available&&!Session->IsBusy());
            for (int32 Index=0;Index<Session->Games.Num();++Index)
                Button(FText::FromString(Session->Games[Index].Session.OwningUserName),[Session,Index](){Session->JoinGame(Index);});
            if (Session->IsLocalDev())
            {
                Button(T(TEXT("Héberger en local · Dev"),TEXT("Host locally · Dev")),[Session](){Session->HostLocal();});
                Button(T(TEXT("Rejoindre en local · Dev"),TEXT("Join locally · Dev")),[Session](){Session->JoinLocal();});
            }
        }
        if (GetWorld()->GetNetMode()!=NM_Standalone)
            Button(T(TEXT("Quitter la session"),TEXT("Leave session")),[Session](){Session->Leave();});
        Column->AddSlot().AutoHeight().Padding(0,8)[SNew(STextBlock).Text_Lambda([Session](){return Session->Status;}).ColorAndOpacity(Ivory).AutoWrapText(true)];
    }
    Button(T(TEXT("Retour au wagon"),TEXT("Back to the train")),[this](){Resume();});
    return SNew(SScrollBox)+SScrollBox::Slot()[Column];
}
TSharedRef<SWidget> UTrainMenu::BuildSummary()
{
    auto Column=SNew(SVerticalBox); const auto* GS=GetWorld()->GetGameState<ATrainGameState>();
    Column->AddSlot().AutoHeight().Padding(0,0,0,20)[Label(T(TEXT("Bilan de l’expédition"),TEXT("Expedition summary")),29)];
    Column->AddSlot().AutoHeight().Padding(0,0,0,15)[Label(FText::Format(T(TEXT("{0} salle(s) sécurisée(s)"),TEXT("{0} room(s) cleared")),FText::AsNumber(GS ? GS->RoomsCleared : 0)),20)];
    if (GS) for (const APlayerState* State:GS->PlayerArray)
        if (const auto* PS=Cast<ATrainPlayerState>(State))
        {
            const TWeakObjectPtr<const ATrainPlayerState> WeakState(PS);
            Column->AddSlot().AutoHeight().Padding(0,8)
                [SNew(STextBlock).ColorAndOpacity(Ivory).AutoWrapText(true)
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular",GS->PlayerArray.Num()==1 ? 23 : 18))
                    .Text_Lambda([WeakState]()
                    {
                        if (!WeakState.IsValid()) return FText::GetEmpty();
                        const auto* Entry=WeakState.Get();
                        return FText::Format(T(TEXT("{0}\nEnnemis éliminés : {1}   ·   Dégâts infligés : {2}   ·   Dégâts reçus : {3}"),
                            TEXT("{0}\nEnemies defeated: {1}   ·   Damage dealt: {2}   ·   Damage taken: {3}")),FText::FromString(Entry->GetPlayerName()),
                            FText::AsNumber(Entry->Kills),FText::AsNumber(Entry->DamageDealt),FText::AsNumber(Entry->DamageTaken));
                    })];
        }
    const bool Host=GetWorld()->GetNetMode()!=NM_Client;
    auto Back=SNew(SButton).ButtonStyle(&ButtonStyle).HAlign(HAlign_Center).ContentPadding(FMargin(14,12)).IsEnabled(Host)
        .OnClicked_Lambda([this](){GetWorld()->ServerTravel(TEXT("/Game/Maps/Wagon"));return FReply::Handled();})
        [Label(Host ? T(TEXT("Retour au wagon"),TEXT("Back to the train")) : T(TEXT("En attente de l’hôte…"),TEXT("Waiting for the host…")),20)];
    FirstFocus=Back; Column->AddSlot().AutoHeight().Padding(0,20,0,0)[Back];
    return Column;
}

TSharedRef<SWidget> UTrainMenu::BuildStation()
{
    auto* Profile=GetGameInstance()->GetSubsystem<UTrainProfile>();
    auto Column=SNew(SVerticalBox);
    Column->AddSlot().AutoHeight().Padding(0,0,0,10)[Label(ATrainStation::Title(Station),29)];
    Column->AddSlot().AutoHeight().Padding(0,0,0,20)[SNew(STextBlock).ColorAndOpacity(Ivory)
        .Font(FCoreStyle::GetDefaultFontStyle("Regular",17))
        .Text_Lambda([Profile](){return FText::Format(T(TEXT("Pièces d’atelier : {0}"),TEXT("Workshop tokens: {0}")),FText::AsNumber(Profile->Data->Points));})];
    auto Button=[this,Column](FText Text,TFunction<void()> Action,bool Enabled=true)
    {
        auto Item=SNew(SButton).ButtonStyle(&ButtonStyle).HAlign(HAlign_Left).ContentPadding(FMargin(18,12)).IsEnabled(Enabled)
            .OnClicked_Lambda([Action](){Action();return FReply::Handled();})[Label(Text,18)];
        if (!FirstFocus.IsValid() && Enabled) FirstFocus=Item;
        Column->AddSlot().AutoHeight().Padding(0,5)[SNew(SBorder).BorderImage(&BorderBrush).Padding(1)[Item]];
    };
    auto Update=[this]() { if (auto* Pawn=Cast<ATrainPawn>(GetOwningPlayerPawn())) Pawn->SendLoadout();Refresh(); };
    auto Select=[Profile,Update](int32 C,int32 W) { if (Profile->Select(C,W)) Update(); };
    auto Buy=[this,Profile,Button,Update](FName Item,FText Name)
    {
        const FName Requires=UTrainProfile::Prerequisite(Item);const bool Owned=Profile->Has(Item);
        FText Text=Owned ? FText::Format(T(TEXT("{0} · Débloqué"),TEXT("{0} · Unlocked")),Name)
            : FText::Format(T(TEXT("{0} · {1} pièces"),TEXT("{0} · {1} tokens")),Name,FText::AsNumber(UTrainProfile::Cost(Item)));
        Button(Text,[Profile,Item,Update](){if (Profile->Buy(Item)) Update();},!Owned&&Profile->Data->Points>=UTrainProfile::Cost(Item)&&(Requires.IsNone()||Profile->Has(Requires)));
    };
    const int32 C=Profile->Data->Character,W=Profile->Data->Weapon;
    if (Station==0)
    {
        Column->AddSlot().AutoHeight().Padding(0,0,0,10)[Label(T(TEXT("Chaque voyageur possède son équipement et son style de combat."),TEXT("Each traveller has their own equipment and combat style.")),15)];
        Button(C==0 ? T(TEXT("Le Conducteur · Sélectionné"),TEXT("The Conductor · Selected")):T(TEXT("Le Conducteur · Robuste, armes lourdes"),TEXT("The Conductor · Sturdy, heavy weapons")),[Select](){Select(0,0);});
        if (Profile->Has(TEXT("character")))
            Button(C==1 ? T(TEXT("L’Éclaireur · Sélectionné"),TEXT("The Scout · Selected")):T(TEXT("L’Éclaireur · Rapide, plus fragile"),TEXT("The Scout · Faster, more fragile")),[Select](){Select(1,0);});
        else Buy(TEXT("character"),T(TEXT("Recruter l’Éclaireur"),TEXT("Recruit the Scout")));
    }
    else if (Station==1)
    {
        Button(FText::Format(T(TEXT("{0}{1}"),TEXT("{0}{1}")),C?T(TEXT("Rapière"),TEXT("Rapier")):T(TEXT("Sabre à vapeur"),TEXT("Steam sabre")),W==0?T(TEXT(" · Équipé"),TEXT(" · Equipped")):FText::GetEmpty()),[Select,C](){Select(C,0);});
        const FName Unlock=C?TEXT("daggers"):TEXT("hammer");
        if (Profile->Has(Unlock))
            Button(FText::Format(T(TEXT("{0}{1}"),TEXT("{0}{1}")),C?T(TEXT("Doubles lames"),TEXT("Twin blades")):T(TEXT("Marteau à piston"),TEXT("Piston hammer")),W==1?T(TEXT(" · Équipé"),TEXT(" · Equipped")):FText::GetEmpty()),[Select,C](){Select(C,1);});
        else Buy(Unlock,C?T(TEXT("Débloquer les doubles lames"),TEXT("Unlock twin blades")):T(TEXT("Débloquer le marteau à piston"),TEXT("Unlock the piston hammer")));
        Column->AddSlot().AutoHeight().Padding(0,12)[Label(T(TEXT("La zone d’entraînement est située dans le wagon suivant. Les coups y sont sans danger."),TEXT("The practice area is in the next car. Attacks there are safe.")),15)];
    }
    else if (Station==2)
    {
        Buy(TEXT("health"),T(TEXT("I · Constitution : +20 points de vie"),TEXT("I · Constitution: +20 health")));
        Column->AddSlot().AutoHeight().Padding(18,6)[Label(T(TEXT("↓ Constitution requise"),TEXT("↓ Requires Constitution")),14)];
        Buy(TEXT("speed"),T(TEXT("II · Mobilité : +15 % de vitesse"),TEXT("II · Mobility: +15% movement speed")));
    }
    Button(T(TEXT("Retour au wagon"),TEXT("Back to the train")),[this](){Resume();});
    return Column;
}
