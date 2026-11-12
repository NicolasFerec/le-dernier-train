#include "TrainBlockoutGameMode.h"
#include "../network/TrainNetwork.h"

void ATrainBlockoutGameMode::StartPlay()
{
    // QuaiBlockout is deliberately outside TrainRooms' procedural map registry.
    // Super instantiates its authored Automate markers through native replication.
    Super::StartPlay();
    auto* State = GetGameState<ATrainGameState>();
    State->RoomIndex = 1;
    State->ForceNetUpdate();
}
