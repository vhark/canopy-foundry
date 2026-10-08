#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FacilityTarget.generated.h"
class UStaticMeshComponent;
class UPointLightComponent;
namespace canopy { struct Id; }

UCLASS()
class AFacilityTarget : public AActor
{
    GENERATED_BODY()
public:
    AFacilityTarget();
    void DisplayControl(double Value);
    canopy::Id DomainId() const;
private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Housing;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Indicator;
};
