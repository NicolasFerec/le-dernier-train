#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "TrainNetwork.h"
#include "TrainSession.h"
#include "TrainDash.h"
#include "TrainMelee.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "../TrainPrototype.h"
#include "../TrainSettings.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerInput.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainCoopTest,"LeDernierTrain.Network.FourPlayers",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
namespace
{
    int32 PreviousClients=1;
    bool PreviousSingleProcess=true;
    EPlayNetMode PreviousMode=PIE_Standalone;
    class FCheckTrainCoop : public IAutomationLatentCommand
    {
        FAutomationTestBase* Test;
        int32 Phase=0;
        double Since=FPlatformTime::Seconds();
        TWeakObjectPtr<UWorld> Server;
        TWeakObjectPtr<UWorld> Client;
        TWeakObjectPtr<ATrainEnemy> Target;
        FVector Before;
        int32 PlayerID=0;
        int32 EnemyCount=0;
        double PausedTime=0;
        void Next() { ++Phase; Since=FPlatformTime::Seconds(); }
        ATrainPawn* ClientPawn() const
        {
            const auto* PC=Client.IsValid() ? Client->GetFirstPlayerController() : nullptr;
            return PC ? Cast<ATrainPawn>(PC->GetPawn()) : nullptr;
        }
        ATrainPawn* ServerPawn() const
        {
            if (Server.IsValid()) for (TActorIterator<ATrainPawn> It(Server.Get());It;++It)
                if (It->GetPlayerState() && It->GetPlayerState()->GetPlayerId()==PlayerID) return *It;
            return nullptr;
        }
    public:
        explicit FCheckTrainCoop(FAutomationTestBase* InTest):Test(InTest){}
        virtual bool Update() override
        {
            const double Elapsed=FPlatformTime::Seconds()-Since;
            if (Elapsed>20) { Test->AddError(FString::Printf(TEXT("Network phase %d timed out"),Phase)); return true; }
            for (const auto& Context:GEngine->GetWorldContexts()) if (Context.WorldType==EWorldType::PIE && Context.World())
            {
                if (Context.World()->GetNetMode()==NM_ListenServer) Server=Context.World();
                if (Phase>0 && Context.World()->GetNetMode()==NM_Client && Context.World()->GetFirstPlayerController()
                    && Context.World()->GetFirstPlayerController()->PlayerState
                    && Context.World()->GetFirstPlayerController()->PlayerState->GetPlayerId()==PlayerID) Client=Context.World();
            }
            if (Phase==0)
            {
                int32 Clients=0;
                for (const auto& Context:GEngine->GetWorldContexts()) if (Context.WorldType==EWorldType::PIE)
                {
                    auto* World=Context.World();
                    if (World->GetNetMode()==NM_ListenServer) Server=World;
                    if (World->GetNetMode()==NM_Client) { ++Clients; Client=World; }
                }
                if (Clients!=3 || !Server.IsValid() || !ClientPawn() || !ClientPawn()->GetPlayerState()) return false;
                auto* GS=Server->GetGameState<ATrainGameState>();
                if (!GS || GS->PlayerArray.Num()!=4) return false;
                for (const auto& Context:GEngine->GetWorldContexts()) if (Context.WorldType==EWorldType::PIE)
                    if (!Context.World()->GetGameState() || Context.World()->GetGameState()->PlayerArray.Num()!=4) return false;
                Test->TestEqual(TEXT("Four registered players"),GS->PlayerArray.Num(),4);
                for (const auto& Context:GEngine->GetWorldContexts()) if (Context.WorldType==EWorldType::PIE)
                    Test->TestEqual(TEXT("Roster replicated to every world"),Context.World()->GetGameState()->PlayerArray.Num(),4);
                PlayerID=ClientPawn()->GetPlayerState()->GetPlayerId();
                if (!ServerPawn()) return false;
                ServerPawn()->SetActorLocation(FVector(-200,-160,95),false,nullptr,ETeleportType::TeleportPhysics);
                Next(); return false;
            }
            if (Phase==4 || Phase==10)
                for (const auto& Context:GEngine->GetWorldContexts()) if (Context.WorldType==EWorldType::PIE)
                {
                    if (Context.World()->GetNetMode()==NM_ListenServer) Server=Context.World();
                    if (Context.World()->GetNetMode()==NM_Client && Context.World()->GetFirstPlayerController() && Context.World()->GetFirstPlayerController()->PlayerState
                        && Context.World()->GetFirstPlayerController()->PlayerState->GetPlayerId()==PlayerID) Client=Context.World();
                }
            if (!Server.IsValid() || !Client.IsValid()) return false;
            auto* HostPC=Server->GetFirstPlayerController();
            auto* Host=HostPC ? Cast<ATrainPawn>(HostPC->GetPawn()) : nullptr;
            auto* GS=Server->GetGameState<ATrainGameState>();
            if (Phase!=4 && Phase!=10 && (!Host || !GS)) return false;
            if (Phase==1)
            {
                if (Elapsed<.8 || !ClientPawn()) return false;
                Before=ServerPawn()->GetActorLocation();
                Client->GetFirstPlayerController()->InputKey(FInputKeyEventArgs::CreateSimulated(UTrainSettings::Get()->Key(TEXT("MoveUp"),false),IE_Pressed,1));
                Next(); return false;
            }
            if (Phase==2)
            {
                if (Elapsed<.8) return false;
                auto* Move=FindObject<UInputAction>(ClientPawn(),TEXT("Move"));
                auto* Input=Cast<UEnhancedInputComponent>(ClientPawn()->InputComponent);
                const FVector2D Value=Move&&Input?Input->GetBoundActionValue(Move).Get<FVector2D>():FVector2D::ZeroVector;
                UE_LOG(LogTemp,Display,TEXT("Train movement QA: before %s, server %s, client %s, velocity %s, input %s, key %.2f, mode %d, ignored %d"),
                    *Before.ToString(),*ServerPawn()->GetActorLocation().ToString(),*ClientPawn()->GetActorLocation().ToString(),*ClientPawn()->GetVelocity().ToString(),*Value.ToString(),
                    Client->GetFirstPlayerController()->PlayerInput->GetKeyValue(UTrainSettings::Get()->Key(TEXT("MoveUp"),false)),int32(ClientPawn()->GetCharacterMovement()->MovementMode),Client->GetFirstPlayerController()->IsMoveInputIgnored());
                Client->GetFirstPlayerController()->InputKey(FInputKeyEventArgs::CreateSimulated(UTrainSettings::Get()->Key(TEXT("MoveUp"),false),IE_Released,0));
                Test->TestTrue(TEXT("Client movement reaches authority through native CharacterMovement"),FVector::Dist2D(Before,ServerPawn()->GetActorLocation())>30);
                Test->TestTrue(TEXT("Client and server converge"),FVector::Dist2D(ClientPawn()->GetActorLocation(),ServerPawn()->GetActorLocation())<90);
                ClientPawn()->GetAbilitySystemComponent()->TryActivateAbilityByClass(UTrainDash::StaticClass());
                Next(); return false;
            }
            if (Phase==3)
            {
                if (Elapsed<.12) return false;
                Test->TestTrue(TEXT("Predicted GAS dash is accepted by server"),ServerPawn()->GetPlayerState<ATrainPlayerState>()->DashReadyAt>GS->GetServerWorldTimeSeconds());
                Host->SetActorLocation(GS->DeparturePoint+FVector(0,0,95),false,nullptr,ETeleportType::TeleportPhysics);
                Server->GetAuthGameMode<ATrainGameMode>()->RequestDeparture(Host);
                Test->TestTrue(TEXT("Missing players start a ten-second countdown"),GS->DepartureAt>GS->GetServerWorldTimeSeconds()+9);
                for (TActorIterator<ATrainPawn> It(Server.Get());It;++It)
                    It->SetActorLocation(GS->DeparturePoint+FVector(-80,0,95),false,nullptr,ETeleportType::TeleportPhysics);
                Next(); return false;
            }
            if (Phase==4)
            {
                if (UGameplayStatics::GetCurrentLevelName(Server.Get())!=TEXT("Pompage") || UGameplayStatics::GetCurrentLevelName(Client.Get())!=TEXT("Pompage"))
                {
                    for (const auto& Context:GEngine->GetWorldContexts()) if (Context.WorldType==EWorldType::PIE)
                    {
                        if (Context.World()->GetNetMode()==NM_ListenServer) Server=Context.World();
                        if (Context.World()->GetNetMode()==NM_Client && Context.World()->GetFirstPlayerController() && Context.World()->GetFirstPlayerController()->PlayerState
                            && Context.World()->GetFirstPlayerController()->PlayerState->GetPlayerId()==PlayerID) Client=Context.World();
                    }
                    return false;
                }
                if (!ClientPawn() || !ServerPawn() || Server->GetGameState()->PlayerArray.Num()!=4 || Elapsed<1) return false;
                GS=Server->GetGameState<ATrainGameState>();
                Test->TestTrue(TEXT("All ready skips the remainder of the countdown"),Elapsed<9);
                EnemyCount=GS->RemainingEnemies;Test->TestTrue(TEXT("Room has authoritative enemies"),EnemyCount>=3);
                for (TActorIterator<ATrainEnemy> It(Server.Get());It;++It) { It->SetActorTickEnabled(false);if (auto* AI=Cast<AAIController>(It->GetController())) AI->StopMovement();It->GetCharacterMovement()->StopMovementImmediately(); Target=*It; }
                Test->TestNotNull(TEXT("Enemy exists"),Target.Get());
                if (!Target.IsValid()) return true;
                ServerPawn()->SetActorLocation(Target->GetActorLocation()-FVector(100,0,0),false,nullptr,ETeleportType::TeleportPhysics);
                ServerPawn()->SetActorRotation(FRotator::ZeroRotator);
                Next(); return false;
            }
            if (Phase==5)
            {
                if (Elapsed<.5) return false;
                ClientPawn()->GetAbilitySystemComponent()->TryActivateAbilityByClass(UTrainLightAttack::StaticClass());
                Test->TestTrue(TEXT("Client light animation starts locally through GAS prediction"),ClientPawn()->GetMesh()->GetAnimInstance()->IsAnyMontagePlaying());
                ClientPawn()->ServerAttack(false);
                Next(); return false;
            }
            if (Phase==6)
            {
                if (Elapsed<.55) return false;
                Test->TestEqual(TEXT("Server applies one hit and rejects attack spam"),Target->Health,60.f);
                ClientPawn()->GetAbilitySystemComponent()->TryActivateAbilityByClass(UTrainHeavyAttack::StaticClass());
                Next(); return false;
            }
            if (Phase==7)
            {
                if (Elapsed<.6) return false;
                Test->TestFalse(TEXT("Killed enemy is removed on the host"),Target.IsValid());
                for (const auto& Context:GEngine->GetWorldContexts()) if (Context.WorldType==EWorldType::PIE)
                {
                    Test->TestEqual(TEXT("Enemy death counter agrees everywhere"),Context.World()->GetGameState<ATrainGameState>()->RemainingEnemies,EnemyCount-1);
                    int32 Count=0; for (TActorIterator<ATrainEnemy> It(Context.World());It;++It) ++Count;
                    Test->TestEqual(TEXT("Enemy removal replicates everywhere"),Count,EnemyCount-1);
                }
                auto* PS=ServerPawn()->GetPlayerState<ATrainPlayerState>();
                Test->TestEqual(TEXT("Damage credited to the attacking player"),PS->DamageDealt,90.f);
                Test->TestEqual(TEXT("Kill credited to the attacking player"),PS->Kills,1);
                ServerPawn()->ReceiveHit(12);
                ClientPawn()->ReceiveHit(99);
                Test->TestEqual(TEXT("Clients cannot apply damage"),PS->Health,88.f);
                ClientPawn()->TogglePause();
                Test->TestFalse(TEXT("A guest cannot pause the world"),UGameplayStatics::IsGamePaused(Server.Get()));
                Host->TogglePause(); PausedTime=GS->GetServerWorldTimeSeconds();
                Next(); return false;
            }
            if (Phase==8)
            {
                if (Elapsed<.4) return false;
                Test->TestTrue(TEXT("Host pauses authoritative world"),UGameplayStatics::IsGamePaused(Server.Get()));
                Test->TestTrue(TEXT("Pause replicates to client"),UGameplayStatics::IsGamePaused(Client.Get()));
                Test->TestTrue(TEXT("Simulation time freezes"),FMath::Abs(GS->GetServerWorldTimeSeconds()-PausedTime)<.05);
                Test->TestEqual(TEXT("Damage and health replicate to guest"),ClientPawn()->GetPlayerState<ATrainPlayerState>()->Health,88.f);
                Host->TogglePause(); Next(); return false;
            }
            if (Phase==9)
            {
                if (Elapsed<.3) return false;
                Test->TestFalse(TEXT("Host resumes the session"),UGameplayStatics::IsGamePaused(Client.Get()));
                Host->SetActorLocation(GS->DeparturePoint+FVector(-150,0,95),false,nullptr,ETeleportType::TeleportPhysics);
                Server->GetAuthGameMode<ATrainGameMode>()->RequestDeparture(Host);
                Test->TestEqual(TEXT("Exit stays locked while enemies remain"),GS->DepartureAt,0.0);
                TArray<ATrainEnemy*> Enemies;for (TActorIterator<ATrainEnemy> It(Server.Get());It;++It) Enemies.Add(*It);
                for (auto* Enemy:Enemies) Enemy->Hit(Host,10000);
                for (TActorIterator<ATrainPawn> It(Server.Get()); It; ++It)
                    It->SetActorLocation(FVector(-1900,-1100,95),false,nullptr,ETeleportType::TeleportPhysics);
                Host->ReceiveHit(10000);
                Host->ServerInteract();
                Test->TestEqual(TEXT("A fallen host needs a survivor at the exit"), GS->DepartureAt, 0.0);
                ServerPawn()->SetActorLocation(GS->DeparturePoint+FVector(-150,0,95),false,nullptr,ETeleportType::TeleportPhysics);
                ServerPawn()->ServerInteract();
                Test->TestEqual(TEXT("A guest cannot start departure for the fallen host"), GS->DepartureAt, 0.0);
                Host->ServerInteract();
                Test->TestTrue(TEXT("A fallen host can start the countdown remotely"),GS->DepartureAt>GS->GetServerWorldTimeSeconds()+9);
                Next();return false;
            }
            if (Phase==10)
            {
                if (UGameplayStatics::GetCurrentLevelName(Server.Get())!=TEXT("Fonderie") || UGameplayStatics::GetCurrentLevelName(Client.Get())!=TEXT("Fonderie") || !ClientPawn() || !ClientPawn()->GetPlayerState<ATrainPlayerState>() || !GS || GS->PlayerArray.Num()!=4) return false;
                const ATrainPlayerState* FallenHost = nullptr;
                for (const APlayerState* Player : GS->PlayerArray)
                    if (const auto* State = Cast<ATrainPlayerState>(Player); State && State->IsHost) FallenHost = State;
                if (!FallenHost) return false;
                Test->TestTrue(TEXT("Missing guests travel after ten seconds"),Elapsed>=9.5);
                Test->TestEqual(TEXT("Four players remain together in the next room"),Server->GetGameState()->PlayerArray.Num(),4);
                Test->TestEqual(TEXT("Cleared-room count survives multiplayer travel"),Server->GetGameState<ATrainGameState>()->RoomsCleared,1);
                Test->TestEqual(TEXT("Guest health survives room travel"),ClientPawn()->GetPlayerState<ATrainPlayerState>()->Health,88.f);
                Test->TestEqual(TEXT("The fallen host stays down after room travel"), FallenHost->Health, 0.f);
                return true;
            }
            return false;
        }
    };
}
DEFINE_LATENT_AUTOMATION_COMMAND(FRestoreTrainPlaySettings);
bool FRestoreTrainPlaySettings::Update()
{
    auto* Settings=GetMutableDefault<ULevelEditorPlaySettings>();
    Settings->SetPlayNumberOfClients(PreviousClients);Settings->SetPlayNetMode(PreviousMode);Settings->SetRunUnderOneProcess(PreviousSingleProcess);
    return true;
}
bool FTrainCoopTest::RunTest(const FString& Parameters)
{
    FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Maps/Wagon"));
    auto* Settings=GetMutableDefault<ULevelEditorPlaySettings>();
    Settings->GetPlayNumberOfClients(PreviousClients);Settings->GetPlayNetMode(PreviousMode);Settings->GetRunUnderOneProcess(PreviousSingleProcess);
    Settings->SetPlayNumberOfClients(4);Settings->SetPlayNetMode(PIE_ListenServer);Settings->SetRunUnderOneProcess(true);
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCheckTrainCoop>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    ADD_LATENT_AUTOMATION_COMMAND(FRestoreTrainPlaySettings());
    return true;
}
#endif
