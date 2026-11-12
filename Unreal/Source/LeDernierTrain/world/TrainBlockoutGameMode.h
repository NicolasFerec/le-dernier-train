#pragma once
#include "../TrainPrototype.h"
#include "TrainBlockoutGameMode.generated.h"

// Dedicated map override: authored encounters while reviewing the first room.
// Existing expedition maps keep their seeded PCG generation.
UCLASS()
class ATrainBlockoutGameMode : public ATrainGameMode
{
    GENERATED_BODY()
public:
    virtual void StartPlay() override;
};
