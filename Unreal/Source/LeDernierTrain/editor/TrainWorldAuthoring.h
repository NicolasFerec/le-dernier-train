#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TrainWorldAuthoring.generated.h"

UCLASS()
class UTrainWorldAuthoring : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Small editor bridge to Epic's brush builder / Recast build; never used by gameplay.
    UFUNCTION(BlueprintCallable,meta=(WorldContext="WorldContext"))
    static void BuildNavigation(UObject* WorldContext,FVector Extent);
    UFUNCTION(BlueprintCallable)
    static void AssignSurface(class USkeletalMesh* Mesh,class UMaterialInterface* Material);
    UFUNCTION(BlueprintCallable,meta=(WorldContext="WorldContext"))
    static AActor* AddRepeatedMesh(UObject* WorldContext,class UStaticMesh* Mesh,class UMaterialInterface* Material,const TArray<FTransform>& Transforms);
};
