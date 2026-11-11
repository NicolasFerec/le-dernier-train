#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "TrainSettings.generated.h"

UCLASS(Config=GameUserSettings)
class UTrainSettings : public UGameUserSettings
{
    GENERATED_BODY()

public:
    static UTrainSettings* Get();
    virtual void SetToDefaults() override;
    virtual void SaveSettings() override;
    FString GetUpdateChannel() const;
    void SetUpdateChannel(const FString& Channel);
    void ApplyRuntime(UWorld* World, bool bMenu);
    float GetGameLimit() const;
    FKey Key(FName Action, bool bGamepad) const;
    bool Rebind(FName Action, bool bGamepad, FKey NewKey);
    bool HasGameplayBinding(FKey Key) const;
    void ResetBindings();

    UPROPERTY(Config) float MasterVolume = 1;
    UPROPERTY(Config) float MusicVolume = 1;
    UPROPERTY(Config) float EffectsVolume = 1;
    UPROPERTY(Config) bool ShowFPS = false;
    UPROPERTY(Config) bool Vibration = true;
    UPROPERTY(Config) FString Language = TEXT("fr");
    UPROPERTY(Config) int32 KeyboardLayout = 1;
    UPROPERTY(Config) int32 Monitor = -1;
    UPROPERTY(Config) int32 GameFPS = -1;
    UPROPERTY(Config) TMap<FString, FKey> Bindings;
private:
    // Keep the native volume buses alive across world travel and garbage collection.
    UPROPERTY(Transient) TObjectPtr<class USoundMix> VolumeMix;
    UPROPERTY(Transient) TArray<TObjectPtr<class USoundClass>> VolumeClasses;
};
