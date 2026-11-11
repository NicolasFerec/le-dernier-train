#include "TrainProfile.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
void UTrainProfile::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Slot=TEXT("TrainProfile");
    FString Label;
    if (FParse::Value(FCommandLine::Get(),TEXT("TrainProfile="),Label))
    {
        Label=Label.Replace(TEXT("/"),TEXT("_")).Replace(TEXT("\\"),TEXT("_"));
        Slot+=TEXT("_")+Label.Left(40);
    }
    if (UGameplayStatics::DoesSaveGameExist(Slot,0)) Data=Cast<UTrainSave>(UGameplayStatics::LoadGameFromSlot(Slot,0));
    if (!Data) Data=Cast<UTrainSave>(UGameplayStatics::CreateSaveGameObject(UTrainSave::StaticClass()));
    Data->Points=FMath::Max(0,Data->Points);
    Data->Character=Has(TEXT("character")) ? FMath::Clamp(Data->Character,0,1) : 0;
    Data->Weapon=Has(Data->Character ? TEXT("daggers") : TEXT("hammer")) ? FMath::Clamp(Data->Weapon,0,1) : 0;
}
bool UTrainProfile::Has(FName Item) const { return Data && Data->Unlocks.Contains(Item); }
int32 UTrainProfile::Cost(FName Item)
{
    if (Item==TEXT("health")) return 1;
    if (Item==TEXT("character") || Item==TEXT("hammer") || Item==TEXT("daggers") || Item==TEXT("speed")) return 2;
    return INDEX_NONE;
}
FName UTrainProfile::Prerequisite(FName Item)
{ return Item==TEXT("daggers") ? FName(TEXT("character")) : Item==TEXT("speed") ? FName(TEXT("health")) : NAME_None; }
bool UTrainProfile::Save() { return Data && UGameplayStatics::SaveGameToSlot(Data,Slot,0); }
bool UTrainProfile::Buy(FName Item)
{
    const int32 Price=Cost(Item); const FName Requires=Prerequisite(Item);
    if (!Data || Price<0 || Has(Item) || Data->Points<Price || (!Requires.IsNone() && !Has(Requires))) return false;
    Data->Points-=Price; Data->Unlocks.Add(Item);
    if (Save()) return true;
    Data->Points+=Price; Data->Unlocks.Remove(Item);return false;
}
bool UTrainProfile::Select(int32 Character,int32 Weapon)
{
    if (!Data || Character<0 || Character>1 || Weapon<0 || Weapon>1 || (Character && !Has(TEXT("character")))
        || (Weapon && !Has(Character ? TEXT("daggers") : TEXT("hammer")))) return false;
    const int32 OldCharacter=Data->Character,OldWeapon=Data->Weapon;
    Data->Character=Character;Data->Weapon=Weapon;
    if (Save()) return true;
    Data->Character=OldCharacter;Data->Weapon=OldWeapon;return false;
}
bool UTrainProfile::Reward(int32 Amount)
{
    if (!Data || Amount<=0 || Amount>1000) return false;
    Data->Points+=Amount;if (Save()) return true;Data->Points-=Amount;return false;
}
