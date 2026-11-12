#include "TrainWorldPrompt.h"
#include "TrainInputHints.h"
#include "../network/TrainNetwork.h"
#include "../TrainPrototype.h"
#include "../TrainRooms.h"
#include "../TrainSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
TSharedRef<SWidget> UTrainWorldPrompt::RebuildWidget()
{
    Background=FSlateRoundedBoxBrush(FLinearColor(.012f,.018f,.024f,.96f),7.f,FLinearColor(.65f,.35f,.17f),1.f);
    return SNew(SBorder).BorderImage(&Background).Padding(FMargin(10,7)).Visibility_Lambda([this]()
        {
            const auto* PC=UGameplayStatics::GetPlayerController(this,0);
            const auto* Pawn=PC ? Cast<ATrainPawn>(PC->GetPawn()) : nullptr;
            return Pawn && !Pawn->IsMenuOpen() && !UGameplayStatics::IsGamePaused(this) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
        })
        [SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [SNew(STrainInputGlyph).Key_Lambda([this](){return TrainInput::ActionKey(this,TEXT("Interact"));})]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10,0,2,0)
                [SNew(STextBlock).ColorAndOpacity(FLinearColor(.96f,.86f,.7f)).Font(FCoreStyle::GetDefaultFontStyle("Regular",16))
                    .Text_Lambda([this]()
                    {
                        const int32 Kind = Station.IsValid() ? Station->Kind : 3;
                        const auto* GS = GetWorld()->GetGameState<ATrainGameState>();
                        if (Kind != 3 || !GS || GS->RoomIndex == 0)
                            return ATrainStation::Title(Kind);
                        const bool English = UTrainSettings::Get()->Language == TEXT("en");
                        if (GS->RoomIndex == TrainRooms::Count)
                            return FText::FromString(English ? TEXT("Restart the pumps") : TEXT("Remettre les pompes en marche"));
                        return FText::FromString(English ? TEXT("Continue") : TEXT("Continuer"));
                    })]];
}
