#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "FacilityRoomCommandlet.generated.h"

UCLASS()
class UFacilityRoomCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UFacilityRoomCommandlet();
    virtual int32 Main(const FString& Params) override;
};
