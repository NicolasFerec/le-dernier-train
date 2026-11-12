#pragma once

#include "CoreMinimal.h"

namespace TrainRooms
{
    struct FRoom
    {
        const TCHAR* Map;
        const TCHAR* French;
        const TCHAR* English;
    };

    inline constexpr FRoom Faubourgs[] = {
        {TEXT("Pompage"), TEXT("Quai des évacués"), TEXT("Evacuation Platform")},
        {TEXT("Faubourgs02"), TEXT("Rue des vannes"), TEXT("Valve Street")},
        {TEXT("Faubourgs03"), TEXT("Entrepôt des messageries"), TEXT("Freight Warehouse")},
        {TEXT("Faubourgs04"), TEXT("Passerelles du canal"), TEXT("Canal Walkways")},
        {TEXT("Faubourgs05"), TEXT("Écluse du faubourg"), TEXT("District Lock")},
        {TEXT("Faubourgs06"), TEXT("Marché submergé"), TEXT("Sunken Market")},
        {TEXT("Faubourgs07"), TEXT("Collecteurs"), TEXT("Water Collectors")},
        {TEXT("Faubourgs08"), TEXT("Salle des chaudières"), TEXT("Boiler Hall")},
        {TEXT("Faubourgs09"), TEXT("Poste de répartition"), TEXT("Switchboard")},
        {TEXT("Faubourgs10"), TEXT("Station de pompage"), TEXT("Pumping Station")},
    };

    inline constexpr int32 Count = UE_ARRAY_COUNT(Faubourgs);

    inline int32 Index(const FString& Map)
    {
        for (int32 Index = 0; Index < Count; ++Index)
            if (Map == Faubourgs[Index].Map)
                return Index + 1;
        return 0;
    }
}
