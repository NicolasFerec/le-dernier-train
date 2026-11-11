#pragma once
#include "CoreMinimal.h"
#include "CommonInputBaseTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "TrainInputHints.generated.h"

// Common Input owns device tracking. This data supplies the Xbox/mouse artwork.
UCLASS()
class UTrainXboxGlyphs : public UCommonInputBaseControllerData
{
    GENERATED_BODY()
public:
    UTrainXboxGlyphs();
};
UCLASS()
class UTrainMouseGlyphs : public UCommonInputBaseControllerData
{
    GENERATED_BODY()
public:
    UTrainMouseGlyphs();
};
namespace TrainInput
{
    bool UsingGamepad(const UObject* Context);
    FKey ActionKey(const UObject* Context,FName Action);
    FText KeyName(FKey Key);
}
class STrainInputGlyph : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(STrainInputGlyph) : _Size(32), _Selecting(false) {}
        SLATE_ATTRIBUTE(FKey,Key)
        SLATE_ARGUMENT(float,Size)
        SLATE_ATTRIBUTE(bool,Selecting)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args);
private:
    void RefreshBrush() const;
    TAttribute<FKey> BoundKey;
    TAttribute<bool> Selecting;
    mutable FKey CachedKey;
    mutable FSlateBrush Icon;
    FSlateBrush Keycap;
};
