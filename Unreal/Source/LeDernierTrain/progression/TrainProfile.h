#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TrainProfile.generated.h"

UCLASS()
class UTrainSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() int32 Points=0;
    UPROPERTY() TArray<FName> Unlocks;
    UPROPERTY() int32 Character=0;
    UPROPERTY() int32 Weapon=0;
};

UCLASS()
class UTrainProfile : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    UPROPERTY() TObjectPtr<UTrainSave> Data;
    bool Buy(FName Item);
    bool Select(int32 Character,int32 Weapon);
    bool Reward(int32 Amount);
    bool Has(FName Item) const;
    static int32 Cost(FName Item);
    static FName Prerequisite(FName Item);
    bool Save();
private:
    FString Slot;
};
