#include "TrainSettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/TextRenderActor.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RHI.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

namespace
{
    const TArray<FName> Actions { TEXT("MoveUp"), TEXT("MoveDown"), TEXT("MoveLeft"), TEXT("MoveRight"),
        TEXT("Attack"), TEXT("Interact"), TEXT("Dash"), TEXT("HeavyAttack"), TEXT("PreviousTab"), TEXT("NextTab") };

    FString BindingName(FName Action, bool bGamepad, int32 Layout)
    {
        return FString::Printf(TEXT("%s_%d_%s"), bGamepad ? TEXT("Pad") : TEXT("Key"),
            bGamepad ? 0 : Layout, *Action.ToString());
    }
}

UTrainSettings* UTrainSettings::Get()
{
    return CastChecked<UTrainSettings>(GEngine->GetGameUserSettings());
}

void UTrainSettings::SaveSettings()
{
    Super::SaveSettings();
    const FString Directory = FPlatformMisc::GetEnvironmentVariable(TEXT("TRAIN_LAUNCHER_SETTINGS"));
    if (!Directory.IsEmpty())
        FFileHelper::SaveStringToFile(Language == TEXT("en") ? TEXT("en\n") : TEXT("fr\n"),
            *FPaths::Combine(Directory, TEXT("language.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

FString UTrainSettings::GetUpdateChannel() const
{
    const FString Directory = FPlatformMisc::GetEnvironmentVariable(TEXT("TRAIN_LAUNCHER_SETTINGS"));
    if (Directory.IsEmpty()) return {};
    FString Channel;
    FFileHelper::LoadFileToString(Channel, *FPaths::Combine(Directory, TEXT("channel.txt")));
    return Channel.TrimStartAndEnd() == TEXT("dev") ? TEXT("dev") : TEXT("stable");
}

void UTrainSettings::SetUpdateChannel(const FString& Channel)
{
    const FString Directory = FPlatformMisc::GetEnvironmentVariable(TEXT("TRAIN_LAUNCHER_SETTINGS"));
    if (Directory.IsEmpty() || (Channel != TEXT("stable") && Channel != TEXT("dev"))) return;
    FFileHelper::SaveStringToFile(Channel + TEXT("\n"), *FPaths::Combine(Directory, TEXT("channel.txt")),
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void UTrainSettings::SetLanguage(const FString& NewLanguage, UWorld* World)
{
    Language = NewLanguage;
    LocalizeSigns(World);
    SaveSettings();
}

void UTrainSettings::LocalizeSigns(UWorld* World) const
{
    if (!World) return;

    struct FSignText { FName Tag; const TCHAR* French; const TCHAR* English; };
    static const FSignText Signs[] = {
        {TEXT("TrainSign.Wardrobe"), TEXT("VESTIAIRE"), TEXT("WARDROBE")},
        {TEXT("TrainSign.Arsenal"), TEXT("ARSENAL"), TEXT("ARMORY")},
        {TEXT("TrainSign.Passives"), TEXT("ATELIER DES PASSIFS"), TEXT("PASSIVE SKILLS")},
        {TEXT("TrainSign.Practice"), TEXT("ENTRAÎNEMENT"), TEXT("TRAINING")},
        {TEXT("TrainSign.Pumping"), TEXT("STATION DE POMPAGE"), TEXT("PUMP STATION")},
        {TEXT("TrainSign.Turbines"), TEXT("HALLE DES TURBINES"), TEXT("TURBINE HALL")},
    };
    for (TActorIterator<ATextRenderActor> Actor(World); Actor; ++Actor)
    {
        for (const FSignText& Sign : Signs)
        {
            if (!Actor->ActorHasTag(Sign.Tag)) continue;
            Actor->GetTextRender()->SetText(FText::FromString(Language == TEXT("en") ? Sign.English : Sign.French));
            break;
        }
    }
}

void UTrainSettings::SetToDefaults()
{
    Super::SetToDefaults();
    MasterVolume = MusicVolume = EffectsVolume = 1;
    ShowFPS = false;
    Vibration = true;
    Language = TEXT("fr");
    KeyboardLayout = 1;
    Monitor = -1;
    GameFPS = -1;
    Bindings.Empty();
}

float UTrainSettings::GetGameLimit() const
{
    if (GameFPS >= 0)
        return GameFPS;
    FScreenResolutionArray Modes;
    float Refresh = 60;
    if (RHIGetAvailableResolutions(Modes, false))
    {
        FIntPoint Native = GetDesktopResolution();
        for (const auto& Mode : Modes)
            if (Mode.Width == Native.X && Mode.Height == Native.Y)
                Refresh = FMath::Max(Refresh, static_cast<float>(Mode.RefreshRate));
    }
    return Refresh;
}

void UTrainSettings::ApplyRuntime(UWorld* World, bool bMenu)
{
    ApplyNonResolutionSettings();
    GEngine->SetMaxFPS(bMenu ? 30 : GetGameLimit());
    if (World)
    {
        LocalizeSigns(World);
        if (auto* PC = World->GetFirstPlayerController())
            PC->SetDisableHaptics(!Vibration);
        VolumeMix = LoadObject<USoundMix>(nullptr, TEXT("/Game/Audio/TrainMix.TrainMix"));
        auto* Mix=VolumeMix.Get();
        if (Mix)
        {
            for (auto Item : { TPair<const TCHAR*, float>(TEXT("Master"), MasterVolume),
                TPair<const TCHAR*, float>(TEXT("Music"), MusicVolume),
                TPair<const TCHAR*, float>(TEXT("Effects"), EffectsVolume) })
            {
                auto* Class = LoadObject<USoundClass>(nullptr,
                    *FString::Printf(TEXT("/Game/Audio/%s.%s"), Item.Key, Item.Key));
                if (Class)
                {
                    VolumeClasses.AddUnique(Class);
                    UGameplayStatics::SetSoundMixClassOverride(World, Mix, Class, Item.Value, 1, .05f, true);
                }
            }
            UGameplayStatics::SetBaseSoundMix(World, Mix);
        }
    }
    SaveSettings();
}

FKey UTrainSettings::Key(FName Action, bool bGamepad) const
{
    if (Action==TEXT("Pause")) return bGamepad ? EKeys::Gamepad_Special_Right : EKeys::Escape;
    if (Action==TEXT("MenuBack")) return bGamepad ? EKeys::Gamepad_FaceButton_Right : EKeys::Escape;
    if (Action==TEXT("MenuAccept")) return bGamepad ? EKeys::Gamepad_FaceButton_Bottom : EKeys::Enter;
    if (const FKey* Saved = Bindings.Find(BindingName(Action, bGamepad, KeyboardLayout)))
        return *Saved;
    const int32 Index = Actions.IndexOfByKey(Action);
    if (Index == INDEX_NONE)
        return EKeys::Invalid;
    if (bGamepad)
    {
        const FKey Defaults[] { EKeys::Gamepad_DPad_Up, EKeys::Gamepad_DPad_Down,
            EKeys::Gamepad_DPad_Left, EKeys::Gamepad_DPad_Right, EKeys::Gamepad_FaceButton_Left,
            EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_FaceButton_Right, EKeys::Gamepad_FaceButton_Top, EKeys::Gamepad_LeftShoulder, EKeys::Gamepad_RightShoulder };
        return Defaults[Index];
    }
    const FKey Defaults[] { KeyboardLayout == 2 ? EKeys::W : EKeys::Z, EKeys::S,
        KeyboardLayout == 2 ? EKeys::A : EKeys::Q, EKeys::D, EKeys::J, EKeys::E, EKeys::SpaceBar, EKeys::K, EKeys::A, EKeys::E };
    return Defaults[Index];
}

bool UTrainSettings::Rebind(FName Action, bool bGamepad, FKey NewKey)
{
    const bool TabAction=Action==TEXT("PreviousTab") || Action==TEXT("NextTab");
    if (!Actions.Contains(Action) || (TabAction && NewKey.IsMouseButton())) return false;
    if (!NewKey.IsValid() || NewKey.IsGamepadKey() != bGamepad || NewKey == EKeys::Escape
        || NewKey == EKeys::Gamepad_Special_Right || NewKey == EKeys::Gamepad_Special_Left
        || NewKey.IsAxis1D() || NewKey.IsAxis2D() || NewKey.IsAxis3D())
        return false;
    for (FName Other : Actions)
        if ((Other==TEXT("PreviousTab") || Other==TEXT("NextTab"))==TabAction
            && Other != Action && Key(Other, bGamepad) == NewKey)
            return false;
    Bindings.Add(BindingName(Action, bGamepad, KeyboardLayout), NewKey);
    SaveSettings();
    return true;
}

bool UTrainSettings::HasGameplayBinding(FKey InputKey) const
{
    for (FName Action:Actions)
        if (Action!=TEXT("PreviousTab") && Action!=TEXT("NextTab") && Key(Action,InputKey.IsGamepadKey())==InputKey) return true;
    return false;
}
void UTrainSettings::ResetBindings()
{
    Bindings.Empty();
    SaveSettings();
}
