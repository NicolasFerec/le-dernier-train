#pragma once
#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "TrainSession.generated.h"

// UI glue around Epic's Online Subsystem. Steam owns lobbies, invites and transport.
UCLASS()
class UTrainSession : public UGameInstance
{
    GENERATED_BODY()
public:
    virtual void Init() override;
    virtual void Shutdown() override;
    void Solo();
    void HostSteam();
    void InviteFriends();
    void FindSteamGames();
    void JoinGame(int32 Index);
    void Leave(const FText& Reason = FText::GetEmpty());
    void HostLocal();
    void JoinLocal();
    void SetJoinable(bool Allowed);
    bool SteamAvailable() const;
    bool IsBusy() const { return Busy; }
    bool IsLocalDev() const;
    FSimpleMulticastDelegate Changed;
    int32 RoomsCleared = 0;
    int32 ExpeditionSeed = 0;
    int32 ExpeditionCrew = 0;
    FText Status;
    FString Mode = TEXT("Solo");
    TArray<FOnlineSessionSearchResult> Games;
    static FString Version();
private:
    IOnlineSessionPtr Sessions;
    TSharedPtr<FOnlineSessionSearch> Search;
    FDelegateHandle CreatedHandle, JoinedHandle, InviteHandle, FoundHandle, NetworkHandle, TravelHandle;
    bool Busy = false;
    bool Leaving = false;
    enum class EIntent { None, Host, Join, Search };
    EIntent Intent=EIntent::None;
    void Created(FName Name, bool Success);
    void Joined(FName Name, EOnJoinSessionCompleteResult::Type Result);
    void Invited(bool Success, int32 User, FUniqueNetIdPtr Id, const FOnlineSessionSearchResult& Result);
    void Join(const FOnlineSessionSearchResult& Result);
    void Found(bool Success);
    void NetworkFailed(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error);
    void TravelFailed(UWorld* World, ETravelFailure::Type Type, const FString& Error);
};
