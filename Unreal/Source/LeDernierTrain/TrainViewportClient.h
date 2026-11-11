#pragma once

#include "CoreMinimal.h"
#include "CommonGameViewportClient.h"
#include "TrainViewportClient.generated.h"

UCLASS()
class UTrainViewportClient : public UCommonGameViewportClient
{
    GENERATED_BODY()

public:
    virtual void ReceivedFocus(FViewport* InViewport) override;
    virtual void LostFocus(FViewport* InViewport) override;
    virtual void Tick(float DeltaTime) override;

private:
    void UpdateTextInput(bool bEnabled);
    void EnsureFPSCounter();
    TSharedPtr<class SWidget> FPSCounter;
};
