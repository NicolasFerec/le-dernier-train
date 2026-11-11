#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TrainWorldPrompt.generated.h"
class ATrainStation;
UCLASS()
class UTrainWorldPrompt : public UUserWidget
{
    GENERATED_BODY()
public:
    TWeakObjectPtr<ATrainStation> Station;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    FSlateBrush Background;
};
