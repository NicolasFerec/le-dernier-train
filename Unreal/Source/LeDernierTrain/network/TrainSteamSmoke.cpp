#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
// Single-account live check. Uses the game's real Steam session code, not mocks.
#include "TrainSession.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineExternalUIInterface.h"

namespace
{
class FTrainSteamSmoke : public TSharedFromThis<FTrainSteamSmoke>
{
    TWeakObjectPtr<UTrainSession> Game;
    IOnlineSessionPtr Sessions;
    IOnlineExternalUIPtr ExternalUI;
    FDelegateHandle SearchHandle, CreateHandle, DestroyHandle, OverlayHandle;
    bool SearchDone=false, SearchOK=false, CreateDone=false, CreateOK=false, DestroyDone=false, DestroyOK=false, OverlayOpened=false;
    bool IdentityOK=false, LobbyOK=false, DriverOK=false, InviteAccepted=false, Failed=false;
    int32 Phase=0;
    double Since=FPlatformTime::Seconds();
    void Next() { ++Phase; Since=FPlatformTime::Seconds(); }
    void SearchFinished(bool OK) { SearchDone=true; SearchOK=OK; }
    void Created(FName Name,bool OK) { if(Name==NAME_GameSession) { CreateDone=true; CreateOK=OK; } }
    void Destroyed(FName Name,bool OK) { if(Name==NAME_GameSession) { DestroyDone=true; DestroyOK=OK; } }
    void Overlay(bool Open) { OverlayOpened|=Open; }
    bool Finish()
    {
        if (Sessions)
        {
            Sessions->ClearOnFindSessionsCompleteDelegate_Handle(SearchHandle);
            Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
            Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
        }
        if (ExternalUI) ExternalUI->ClearOnExternalUIChangeDelegate_Handle(OverlayHandle);
        const FString Report=FString::Printf(TEXT("{\n  \"app_id\": 480,\n  \"identity\": %s,\n  \"search\": %s,\n  \"lobby\": %s,\n  \"steam_sockets\": %s,\n  \"invite_api_accepted\": %s,\n  \"overlay_open_event\": %s,\n  \"lobby_destroyed\": %s,\n  \"peer_join_tested\": false,\n  \"failed\": %s\n}\n"),
            IdentityOK?TEXT("true"):TEXT("false"),SearchOK?TEXT("true"):TEXT("false"),LobbyOK?TEXT("true"):TEXT("false"),DriverOK?TEXT("true"):TEXT("false"),
            InviteAccepted?TEXT("true"):TEXT("false"),OverlayOpened?TEXT("true"):TEXT("false"),DestroyOK?TEXT("true"):TEXT("false"),Failed?TEXT("true"):TEXT("false"));
        FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("SteamSmoke.json")));
        UE_LOG(LogTemp,Display,TEXT("TRAIN_STEAM_SMOKE finished: %s"),*Report);
        if (FParse::Param(FCommandLine::Get(),TEXT("TrainSteamSmokeExit"))) FPlatformMisc::RequestExit(false);
        return false;
    }
public:
    void Begin(UWorld* World)
    {
        Game=World ? World->GetGameInstance<UTrainSession>() : nullptr;
        auto* OSS=World ? Online::GetSubsystem(World) : nullptr;
        auto Identity=OSS ? OSS->GetIdentityInterface() : nullptr;
        IdentityOK=OSS && OSS->GetSubsystemName()==FName(TEXT("STEAM")) && OSS->GetAppId()==TEXT("480")
            && Identity && Identity->GetLoginStatus(0)==ELoginStatus::LoggedIn && !Identity->GetPlayerNickname(0).IsEmpty()
            && Game.IsValid() && Game->SteamAvailable();
        if (!IdentityOK) { Failed=true; Finish(); return; }
        UE_LOG(LogTemp,Display,TEXT("TRAIN_STEAM_SMOKE identity=OK nickname=%s app_id=%s"),*Identity->GetPlayerNickname(0),*OSS->GetAppId());
        Sessions=OSS->GetSessionInterface(); ExternalUI=OSS->GetExternalUIInterface();
        SearchHandle=Sessions->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateSP(AsShared(),&FTrainSteamSmoke::SearchFinished));
        CreateHandle=Sessions->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateSP(AsShared(),&FTrainSteamSmoke::Created));
        DestroyHandle=Sessions->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateSP(AsShared(),&FTrainSteamSmoke::Destroyed));
        if (ExternalUI) OverlayHandle=ExternalUI->AddOnExternalUIChangeDelegate_Handle(FOnExternalUIChangeDelegate::CreateSP(AsShared(),&FTrainSteamSmoke::Overlay));
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(AsShared(),&FTrainSteamSmoke::Tick),.25f);
        // The ticker delegate is weak; retain this test until completion.
        KeepAlive=AsShared();
        Game->FindSteamGames();
    }
    TSharedPtr<FTrainSteamSmoke> KeepAlive;
    bool Tick(float)
    {
        const double Elapsed=FPlatformTime::Seconds()-Since;
        if (!Game.IsValid()) { Failed=true; Finish(); KeepAlive.Reset(); return false; }
        if (Elapsed>30)
        {
            UE_LOG(LogTemp,Warning,TEXT("TRAIN_STEAM_SMOKE phase %d timed out"),Phase);
            Failed=true;
            if (Phase!=3) { Game->Leave(); Phase=3; Since=FPlatformTime::Seconds(); return true; }
            Finish(); KeepAlive.Reset(); return false;
        }
        if (Phase==0 && SearchDone)
        {
            UE_LOG(LogTemp,Display,TEXT("TRAIN_STEAM_SMOKE search=%s matching_lobbies=%d"),SearchOK?TEXT("OK"):TEXT("FAIL"),Game->Games.Num());
            if (!SearchOK) Failed=true;
            Game->HostSteam(); Next();
        }
        else if (Phase==1 && CreateDone && !CreateOK)
        { Failed=true; Game->Leave(); Phase=3; Since=FPlatformTime::Seconds(); }
        else if (Phase==1 && CreateDone && CreateOK && Game->GetWorld()->GetNetMode()==NM_ListenServer)
        {
            auto* Session=Sessions->GetNamedSession(NAME_GameSession);
            auto* OSS=Online::GetSubsystem(Game->GetWorld());
            const auto* PC=Game->GetFirstLocalPlayerController();
            const bool PlayerNameOK=PC && PC->PlayerState && PC->PlayerState->GetPlayerName()==OSS->GetIdentityInterface()->GetPlayerNickname(0).Left(20);
            UE_LOG(LogTemp,Display,TEXT("TRAIN_STEAM_SMOKE player_name_matches_steam=%s"),PlayerNameOK?TEXT("true"):TEXT("false"));
            Failed|=!PlayerNameOK;
            FString GameKey,Version;
            LobbyOK=Session && Session->SessionInfo.IsValid() && Session->SessionInfo->IsValid()
                && Session->SessionSettings.NumPublicConnections==4 && Session->SessionSettings.bUseLobbiesIfAvailable
                && Session->SessionSettings.BuildUniqueId==GetBuildUniqueId()
                && Session->SessionSettings.Get(FName(TEXT("TRAIN_GAME")),GameKey) && GameKey==TEXT("LeDernierTrain")
                && Session->SessionSettings.Get(FName(TEXT("TRAIN_VERSION")),Version) && Version==UTrainSession::Version();
            auto* Driver=Game->GetWorld()->GetNetDriver();
            DriverOK=Driver && Driver->GetClass()->GetPathName()==TEXT("/Script/SteamSockets.SteamSocketsNetDriver");
            UE_LOG(LogTemp,Display,TEXT("TRAIN_STEAM_SMOKE lobby=%s steam_sockets=%s four_slots=%s"),LobbyOK?TEXT("OK"):TEXT("FAIL"),DriverOK?TEXT("OK"):TEXT("FAIL"),LobbyOK?TEXT("yes"):TEXT("no"));
            Failed|=!LobbyOK || !DriverOK;
            Game->InviteFriends(); InviteAccepted=Game->Status.IsEmpty(); Next();
        }
        else if (Phase==2 && Elapsed>5)
        {
            UE_LOG(LogTemp,Display,TEXT("TRAIN_STEAM_SMOKE invite_api_accepted=%s overlay_open_event=%s (no invitation sent)"),InviteAccepted?TEXT("true"):TEXT("false"),OverlayOpened?TEXT("true"):TEXT("false"));
            Game->Leave(); Next();
        }
        else if (Phase==3 && DestroyDone)
        {
            Failed|=!DestroyOK;
            Finish(); KeepAlive.Reset(); return false;
        }
        return true;
    }
};
FAutoConsoleCommandWithWorld SteamSmokeCommand(TEXT("Train.SteamSmoke"),TEXT("Development-only Steam identity/lobby/invite-UI check; sends no invitations."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) { MakeShared<FTrainSteamSmoke>()->Begin(World); }));
}
#endif
