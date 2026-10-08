#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FacilityCameraComponent.generated.h"

class USpringArmComponent;
class UCameraComponent;
class USceneComponent;

enum class EFacilityView : uint8 { FirstPerson, ThirdPerson, Overhead };
struct FFacilityViewState
{
    EFacilityView Current{EFacilityView::ThirdPerson};
    EFacilityView HandsOn{EFacilityView::ThirdPerson};
    float Fov{90.f};
    float Distance{350.f};
    FFacilityViewState TogglePerspective() const
    {
        FFacilityViewState Result = *this;
        if (Current != EFacilityView::Overhead) Result.Current = Result.HandsOn = Current == EFacilityView::FirstPerson ? EFacilityView::ThirdPerson : EFacilityView::FirstPerson;
        return Result;
    }
    FFacilityViewState ToggleManagement() const
    {
        FFacilityViewState Result = *this;
        Result.Current = Current == EFacilityView::Overhead ? HandsOn : EFacilityView::Overhead;
        return Result;
    }
};

UCLASS(ClassGroup=(Canopy), meta=(BlueprintSpawnableComponent))
class UFacilityCameraComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UFacilityCameraComponent();
    void Setup(USceneComponent* Body, USceneComponent* EyeAnchor);
    void TogglePerspective();
    void ToggleManagement();
    void SetFov(float Fov);
    void SetDistance(float Distance);
    bool IsOverhead() const { return State.Current == EFacilityView::Overhead; }
    EFacilityView CurrentView() const { return State.Current; }
    virtual void TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Function) override;
private:
    UPROPERTY() TObjectPtr<USpringArmComponent> Boom;
    UPROPERTY() TObjectPtr<UCameraComponent> Camera;
    UPROPERTY() TObjectPtr<USceneComponent> Eye;
    FFacilityViewState State;
    float DesiredArmLength{350.f};
    float DesiredSocketHeight{};
    void Apply();
};
