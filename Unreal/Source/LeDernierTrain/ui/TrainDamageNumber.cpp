#include "TrainDamageNumber.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/Text/STextBlock.h"
TSharedRef<SWidget> UTrainDamageNumber::RebuildWidget()
{
    SetVisibility(ESlateVisibility::HitTestInvisible);
    return SNew(STextBlock).Text(FText::AsNumber(FMath::RoundToInt(Damage)))
        .Font(FCoreStyle::GetDefaultFontStyle("Bold",22)).ShadowOffset(FVector2D(1,2))
        .ShadowColorAndOpacity(FLinearColor(0,0,0,.85f))
        .ColorAndOpacity(Hurt ? FLinearColor(1,.18f,.14f) : FLinearColor(.96f,.95f,.87f));
}
void UTrainDamageNumber::NativeTick(const FGeometry& Geometry,float DeltaSeconds)
{
    Super::NativeTick(Geometry,DeltaSeconds);Age+=DeltaSeconds;
    if (Age>1.f) { RemoveFromParent();return; }
    FVector2D Screen;
    if (auto* PC=GetOwningPlayer();PC && PC->ProjectWorldLocationToScreen(WorldPoint,Screen,true))
    {
        Screen.Y+=(Hurt ? 22.f:-16.f)+(Hurt ? 34.f:-48.f)*Age;
        SetPositionInViewport(Screen,true);SetAlignmentInViewport(FVector2D(.5,.5));
    }
    SetRenderOpacity(FMath::Clamp((1.f-Age)*3,0.f,1.f));
}
