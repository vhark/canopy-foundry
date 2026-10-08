#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "CookedFiducialImportCommandlet.generated.h"

UCLASS()
class UCookedFiducialImportCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UCookedFiducialImportCommandlet();
    virtual int32 Main(const FString& Params) override;
};
