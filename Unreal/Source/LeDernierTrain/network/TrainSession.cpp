#include "TrainSession.h"
#include "TrainNetwork.h"
#include "../TrainSettings.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/NetworkVersion.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"

FText TrainText(const TCHAR* FR, const TCHAR* EN)
{
    return FText::FromString(UTrainSettings::Get()->Language == TEXT("en") ? EN : FR);
}
FString UTrainSession::Version()
{
    FString Value;
    GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), Value, GGameIni);
    return Value;
}
void UTrainSession::Init()
{
    Super::Init();
    NetworkHandle = GEngine->OnNetworkFailure().AddUObject(this, &UTrainSession::NetworkFailed);
    TravelHandle = GEngine->OnTravelFailure().AddUObject(this, &UTrainSession::TravelFailed);
    auto* OSS = Online::GetSubsystem(GetWorld());
    if (OSS && OSS->GetSubsystemName() == FName(TEXT("STEAM")))
    {
        Sessions = OSS->GetSessionInterface();
        if (Sessions)
        {
            CreatedHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &UTrainSession::Created));
            JoinedHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &UTrainSession::Joined));
            InviteHandle = Sessions->AddOnSessionUserInviteAcceptedDelegate_Handle(FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &UTrainSession::Invited));
            FoundHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this, &UTrainSession::Found));
        }
    }
}
void UTrainSession::Shutdown()
{
    GEngine->OnNetworkFailure().Remove(NetworkHandle);
    GEngine->OnTravelFailure().Remove(TravelHandle);
    if (Sessions)
    {
        Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreatedHandle);
        Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinedHandle);
        Sessions->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteHandle);
        Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FoundHandle);
        if (const auto* Session=Sessions->GetNamedSession(NAME_GameSession);
            Session && Session->SessionState!=EOnlineSessionState::Destroying)
            Sessions->DestroySession(NAME_GameSession);
    }
    Super::Shutdown();
}
bool UTrainSession::IsLocalDev() const
{
#if UE_BUILD_SHIPPING
    return false;
#else
    return FParse::Param(FCommandLine::Get(), TEXT("TrainLocal")) || (GetWorld() && GetWorld()->WorldType == EWorldType::PIE);
#endif
}
bool UTrainSession::SteamAvailable() const
{
    auto* OSS = Online::GetSubsystem(GetWorld());
    return Sessions && OSS && OSS->GetIdentityInterface() && OSS->GetIdentityInterface()->GetLoginStatus(0) == ELoginStatus::LoggedIn;
}
void UTrainSession::Solo()
{
    if (Busy) return;
    Intent=EIntent::None; Mode = TEXT("Solo"); RoomsCleared=0; Status = FText::GetEmpty();
    UGameplayStatics::OpenLevel(this, TEXT("/Game/Maps/Wagon"));
}
void UTrainSession::HostSteam()
{
    if (Busy || GetWorld()->GetNetMode() != NM_Standalone) return;
    if (!SteamAvailable()) { Status = TrainText(TEXT("Ouvre Steam et relance le jeu pour inviter tes amis."), TEXT("Open Steam and restart the game to invite friends.")); return; }
    if (Sessions->GetNamedSession(NAME_GameSession))
    { Status=TrainText(TEXT("La session précédente est encore en cours de fermeture. Réessaie dans un instant."),TEXT("The previous session is still closing. Try again in a moment.")); return; }
    FOnlineSessionSettings Settings;
    Settings.NumPublicConnections = 4;
    Settings.bIsLANMatch = false;
    Settings.bShouldAdvertise = true;
    Settings.bAllowJoinInProgress = true;
    Settings.bAllowInvites = true;
    Settings.bUsesPresence = true;
    Settings.bAllowJoinViaPresence = true;
    Settings.bUseLobbiesIfAvailable = true;
    // Keep FOnlineSessionSettings' native build ID: Steam validates it when reading a lobby.
    Settings.Set(FName(TEXT("TRAIN_GAME")), FString(TEXT("LeDernierTrain")), EOnlineDataAdvertisementType::ViaOnlineService);
    Settings.Set(FName(TEXT("TRAIN_VERSION")), Version(), EOnlineDataAdvertisementType::ViaOnlineService);
    Intent=EIntent::Host; Busy = true; Status = TrainText(TEXT("Création du salon Steam…"), TEXT("Creating Steam lobby…"));
    if (!Sessions->CreateSession(0, NAME_GameSession, Settings)) Created(NAME_GameSession, false);
}
void UTrainSession::Created(FName Name, bool Success)
{
    if (Name != NAME_GameSession) return;
    if (Intent!=EIntent::Host) { if (Success) Sessions->DestroySession(NAME_GameSession); return; }
    Intent=EIntent::None; Busy=false;
    if (!Success) { Status = TrainText(TEXT("Le salon Steam n’a pas pu être créé. Tu peux réessayer."), TEXT("Could not create the Steam lobby. Please try again.")); return; }
    Mode = TEXT("Steam"); Status = FText::GetEmpty();
    UGameplayStatics::OpenLevel(this, TEXT("/Game/Maps/Wagon"), true, TEXT("listen"));
}
void UTrainSession::InviteFriends()
{
    if (!SteamAvailable() || Mode != TEXT("Steam")) return;
    auto UI = Online::GetSubsystem(GetWorld())->GetExternalUIInterface();
    if (!UI || !UI->ShowInviteUI(0, NAME_GameSession))
        Status = TrainText(TEXT("L’interface Steam est indisponible. Vérifie que l’overlay Steam est activé."), TEXT("Steam overlay unavailable. Check that the Steam overlay is enabled."));
}
void UTrainSession::FindSteamGames()
{
    if (Busy || !SteamAvailable()) return;
    Search = MakeShared<FOnlineSessionSearch>(); Search->MaxSearchResults = 200;
    Search->QuerySettings.Set(FName(TEXT("LOBBYSEARCH")), true, EOnlineComparisonOp::Equals);
    Search->QuerySettings.Set(FName(TEXT("TRAIN_GAME")), FString(TEXT("LeDernierTrain")), EOnlineComparisonOp::Equals);
    Intent=EIntent::Search; Busy = true; Games.Empty(); Status = TrainText(TEXT("Recherche de salons…"), TEXT("Searching for lobbies…"));
    if (!Sessions->FindSessions(0, Search.ToSharedRef())) Found(false);
}
void UTrainSession::Found(bool Success)
{
    if (Intent!=EIntent::Search) return;
    Intent=EIntent::None; Busy = false;
    if (Success && Search)
        for (const auto& Result : Search->SearchResults)
        {
            FString Game;
            if (Result.Session.SessionSettings.Get(FName(TEXT("TRAIN_GAME")), Game) && Game == TEXT("LeDernierTrain")) Games.Add(Result);
        }
    Status = Success ? (Games.IsEmpty() ? TrainText(TEXT("Aucun salon disponible."), TEXT("No lobbies available.")) : FText::GetEmpty())
        : TrainText(TEXT("La recherche Steam a échoué. Tu peux réessayer."), TEXT("Steam search failed. Please try again."));
    Changed.Broadcast();
}
void UTrainSession::JoinGame(int32 Index) { if (Games.IsValidIndex(Index)) Join(Games[Index]); }
void UTrainSession::Invited(bool Success, int32 User, FUniqueNetIdPtr Id, const FOnlineSessionSearchResult& Result)
{
    if (Success && User == 0) Join(Result);
}
void UTrainSession::Join(const FOnlineSessionSearchResult& Result)
{
    if (Busy || !SteamAvailable()) return;
    FString Game, RemoteVersion;
    Result.Session.SessionSettings.Get(FName(TEXT("TRAIN_GAME")), Game);
    Result.Session.SessionSettings.Get(FName(TEXT("TRAIN_VERSION")), RemoteVersion);
    if (Game != TEXT("LeDernierTrain")) { Status = TrainText(TEXT("Cette invitation ne correspond pas au Dernier Train."), TEXT("This invite is not for Le Dernier Train.")); return; }
    if (RemoteVersion != Version()) { Status = TrainText(TEXT("Vos versions du jeu sont différentes. Mettez le jeu à jour avant de vous rejoindre."), TEXT("Your game versions differ. Update the game before joining.")); return; }
    if (GetWorld()->GetNetMode() != NM_Standalone || Sessions->GetNamedSession(NAME_GameSession))
    { Status = TrainText(TEXT("Quitte d’abord la session actuelle pour rejoindre cette invitation."), TEXT("Leave the current session before accepting this invitation.")); return; }
    Intent=EIntent::Join; Busy = true; Status = TrainText(TEXT("Connexion au salon…"), TEXT("Joining the lobby…"));
    if (!Sessions->JoinSession(0, NAME_GameSession, Result)) Joined(NAME_GameSession, EOnJoinSessionCompleteResult::UnknownError);
}
void UTrainSession::Joined(FName Name, EOnJoinSessionCompleteResult::Type Result)
{
    if (Name != NAME_GameSession) return;
    if (Intent!=EIntent::Join) { Sessions->DestroySession(NAME_GameSession); return; }
    Intent=EIntent::None; Busy=false;
    FString Address;
    if (Result != EOnJoinSessionCompleteResult::Success || !Sessions->GetResolvedConnectString(Name, Address))
    {
        Sessions->DestroySession(NAME_GameSession);
        Status = TrainText(TEXT("Impossible de rejoindre ce salon. Il est peut-être plein ou fermé."), TEXT("Unable to join. The lobby may be full or closed.")); return;
    }
    Mode = TEXT("Steam"); Status = FText::GetEmpty();
    if (auto* PC = GetFirstLocalPlayerController()) PC->ClientTravel(Address + TEXT("?TrainVersion=") + Version(), TRAVEL_Absolute);
}
void UTrainSession::SetJoinable(bool Allowed)
{
    if (Sessions && GetWorld()->GetNetMode() != NM_Client)
        if (auto* Session = Sessions->GetNamedSession(NAME_GameSession);
            Session && Session->SessionState!=EOnlineSessionState::Destroying)
        {
            auto Settings = Session->SessionSettings;
            Settings.bAllowJoinInProgress = Allowed; Settings.bAllowJoinViaPresence = Allowed;
            Sessions->UpdateSession(NAME_GameSession, Settings, true);
        }
}
void UTrainSession::Leave(const FText& Reason)
{
    if (Leaving) return;
    Leaving=true;
    if (Sessions && Intent==EIntent::Search) Sessions->CancelFindSessions();
    Intent=EIntent::None; Busy=false; Status=Reason;
    if (auto* World = GetWorld())
    {
        UGameplayStatics::SetGamePaused(World, false);
        if (World->GetNetMode() == NM_ListenServer)
        {
            for (auto It = World->GetPlayerControllerIterator(); It; ++It)
                if (auto* Guest = Cast<ATrainPlayerController>(It->Get()); Guest && !Guest->IsLocalController())
                    Guest->ClientNotice(ETrainNotice::HostLeft);
        }
    }
    if (Sessions) Sessions->DestroySession(NAME_GameSession);
    Mode = TEXT("Solo");
    UGameplayStatics::OpenLevel(this, TEXT("/Game/Maps/Menu"));
    Leaving = false;
}
void UTrainSession::HostLocal()
{
    if (!IsLocalDev() || Busy || GetWorld()->GetNetMode() != NM_Standalone) return;
    Mode = TEXT("Local");
    UGameplayStatics::OpenLevel(this, TEXT("/Game/Maps/Wagon"), true, TEXT("listen"));
}
void UTrainSession::JoinLocal()
{
    if (!IsLocalDev() || Busy) return;
    Mode = TEXT("Local");
    if (auto* PC = GetFirstLocalPlayerController()) PC->ClientTravel(TEXT("127.0.0.1:7777?TrainVersion=") + Version(), TRAVEL_Absolute);
}
void UTrainSession::NetworkFailed(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error)
{
    if (World != GetWorld() || Leaving) return;
    FText Reason;
    if (Type==ENetworkFailure::OutdatedClient || Type==ENetworkFailure::OutdatedServer || Error.Contains(TEXT("TRAIN_VERSION")))
        Reason=TrainText(TEXT("Vos versions du jeu sont différentes. Mettez le jeu à jour avant de vous rejoindre."),TEXT("Your game versions differ. Update the game before joining."));
    else if (Error.Contains(TEXT("SESSION_FULL")) || Error.Contains(TEXT("SERVER_FULL")) || Error.Contains(TEXT("full"),ESearchCase::IgnoreCase))
        Reason=TrainText(TEXT("Ce salon est complet. Quatre joueurs peuvent participer à une session."),TEXT("This lobby is full. A session supports four players."));
    else if (Error.Contains(TEXT("RUN_IN_PROGRESS")))
        Reason=TrainText(TEXT("L’expédition a déjà commencé. Attends le retour de l’équipage au wagon."),TEXT("The expedition has already started. Wait for the crew to return to the train."));
    else Reason=TrainText(TEXT("La connexion à la session a été perdue ou l’hôte a quitté la partie."),TEXT("The session connection was lost or the host left the game."));
    Leave(Reason);
}
void UTrainSession::TravelFailed(UWorld* World, ETravelFailure::Type Type, const FString& Error)
{
    if (World != GetWorld() || Leaving) return;
    Leave(TrainText(TEXT("La partie n’a pas pu être chargée. Vérifie que vous utilisez la même version du jeu."), TEXT("Could not load the game. Check that you are using the same game version.")));
}
