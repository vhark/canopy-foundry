#include "Player/FacilityTarget.h"
#include "SimulationSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "UObject/ConstructorHelpers.h"

AFacilityTarget::AFacilityTarget()
{
    Housing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VentPanel"));
    RootComponent = Housing;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Cube.Succeeded()) Housing->SetStaticMesh(Cube.Object);
    Housing->SetRelativeScale3D(FVector(0.8, 0.15, 0.6));
    Housing->SetCollisionProfileName(TEXT("BlockAll"));
    Indicator = CreateDefaultSubobject<UPointLightComponent>(TEXT("ControlIndicator"));
    Indicator->SetupAttachment(Housing);
    Indicator->SetRelativeLocation(FVector(0, 25, 35));
    Indicator->SetLightColor(FLinearColor(0.2f, 0.8f, 0.3f));
    Indicator->SetIntensity(1000.f);
    Indicator->SetAttenuationRadius(200.f);
}
void AFacilityTarget::DisplayControl(double Value)
{
    Indicator->SetIntensity(200.f + 3500.f * static_cast<float>(Value));
}
canopy::Id AFacilityTarget::DomainId() const
{
    return USimulationSubsystem::TargetId;
}
