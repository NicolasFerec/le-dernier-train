#include "TrainViewportClient.h"
#include "TrainSettings.h"
#include "Engine/UserInterfaceSettings.h"
#include "Misc/App.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/Text/STextBlock.h"

#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericWindow.h"
#include "UnrealClient.h"
#include "Widgets/SWindow.h"

// Use the SDL instance exported by ApplicationCore, which owns the native window.
#if PLATFORM_LINUX
struct SDL_Window;
extern "C" bool SDL_StartTextInput(SDL_Window* Window);
extern "C" bool SDL_StopTextInput(SDL_Window* Window);
extern "C" bool SDL_TextInputActive(SDL_Window* Window);
#endif

void UTrainViewportClient::ReceivedFocus(FViewport* InViewport)
{
    Super::ReceivedFocus(InViewport);
    UpdateTextInput(false);
}

void UTrainViewportClient::LostFocus(FViewport* InViewport)
{
    UpdateTextInput(true);
    Super::LostFocus(InViewport);
}

void UTrainViewportClient::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    EnsureFPSCounter();
    if (!FSlateApplication::IsInitialized())
        return;
    auto Window = GetWindow();
    if (Window && Window->IsVisible()) UTrainSettings::Get()->ApplyInitialDisplay(GetWorld());
    if (!Window || FSlateApplication::Get().GetActiveTopLevelWindow() != Window)
        return;

    auto Focused = FSlateApplication::Get().GetKeyboardFocusedWidget();
    bool bEditingText = false;
    for (auto Widget = Focused; Widget.IsValid(); Widget = Widget->GetParentWidget())
    {
        if (Widget->GetTypeAsString().Contains(TEXT("EditableText")))
        {
            bEditingText = true;
            break;
        }
    }
    UpdateTextInput(bEditingText);
}

void UTrainViewportClient::UpdateTextInput(bool bEnabled)
{
#if PLATFORM_LINUX
    auto Window = GetWindow();
    auto Native = Window ? Window->GetNativeWindow() : nullptr;
    if (!Native || !Native->GetOSWindowHandle())
        return;
    auto* Handle = static_cast<SDL_Window*>(Native->GetOSWindowHandle());
    if (SDL_TextInputActive(Handle) == bEnabled)
        return;

    const bool bChanged = bEnabled ? SDL_StartTextInput(Handle) : SDL_StopTextInput(Handle);
    if (bChanged)
    {
        UE_LOG(LogTemp, Display, TEXT("Train window text input %s"), bEnabled ? TEXT("enabled") : TEXT("disabled"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Could not change the train window text input mode"));
    }
#endif
}


void UTrainViewportClient::EnsureFPSCounter()
{
    if (!GetGameLayerManager().IsValid())
        return;
    if (!FPSCounter.IsValid())
    {
        auto Scale = [this]()
        {
            FVector2D Size;
            GetViewportSize(Size);
            return GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(FIntPoint(Size.X, Size.Y));
        };
        FPSCounter = SNew(SDPIScaler).DPIScale_Lambda(Scale)
            .Visibility_Lambda([]() { return UTrainSettings::Get()->ShowFPS ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
            [SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Top)
                [SNew(SBox).WidthOverride_Lambda([this, Scale]()
                    {
                        FVector2D Size;
                        GetViewportSize(Size);
                        return FMath::Min(Size.X, Size.Y * 21.0f / 9.0f) / Scale();
                    })
                    [SNew(SBox).HAlign(HAlign_Right).Padding(24)
                        [SNew(STextBlock).ColorAndOpacity(FLinearColor(.94f,.84f,.67f))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular",15))
                            .Text_Lambda([]()
                            {
                                return FText::FromString(FString::Printf(TEXT("%d %s"),
                                    FMath::RoundToInt(1.0 / FMath::Max(.001,FApp::GetDeltaTime())),
                                    UTrainSettings::Get()->Language==TEXT("en") ? TEXT("FPS") : TEXT("IPS")));
                            })]]]];
    }
    // A level transition may clear the viewport's overlays. Reattach the same
    // widget, above menus, instead of creating another counter in each screen.
    if (!FPSCounter->GetParentWidget().IsValid())
        AddViewportWidgetContent(FPSCounter.ToSharedRef(),1000);
}
