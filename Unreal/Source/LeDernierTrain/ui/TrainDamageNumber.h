#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TrainDamageNumber.generated.h"
UCLASS()
class UTrainDamageNumber : public UUserWidget
{
    GENERATED_BODY()
public:
    FVector WorldPoint;
    float Damage=0;
    bool Hurt=false;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeTick(const FGeometry& Geometry,float DeltaSeconds) override;
private:
    float Age=0;
};
