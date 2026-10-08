#include "Player/FacilityCameraComponent.h"
#include "Player/FacilityUserSettings.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SceneComponent.h"
#include "Player/FacilityCharacter.h"

UFacilityCameraComponent::UFacilityCameraComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UFacilityCameraComponent::Setup(USceneComponent* Body, USceneComponent* EyeAnchor)
{
    Eye = EyeAnchor;
    AActor* Owner = GetOwner();
    Boom = NewObject<USpringArmComponent>(Owner, TEXT("FacilityBoom"));
    Owner->AddInstanceComponent(Boom);
    Boom->SetupAttachment(Body);
    Boom->SetRelativeLocation(FVector(0, 0, 68));
    Boom->bUsePawnControlRotation = true;
    Boom->bDoCollisionTest = true;
    Boom->ProbeChannel = ECC_Camera;
    Boom->ProbeSize = 16.f;
    Boom->bEnableCameraLag = true;
    Boom->CameraLagSpeed = 12.f;
    // UE blends DesiredRot before its collision sweep, including the overhead ceiling.
    Boom->bEnableCameraRotationLag = true;
    Boom->CameraRotationLagSpeed = 8.f;
    Boom->RegisterComponent();
    Camera = NewObject<UCameraComponent>(Owner, TEXT("FacilityCamera"));
    Owner->AddInstanceComponent(Camera);
    Camera->SetupAttachment(Boom, USpringArmComponent::SocketName);
    Camera->bUsePawnControlRotation = false;
    Camera->RegisterComponent();
    const UFacilityUserSettings* Settings = GetDefault<UFacilityUserSettings>();
    State.HandsOn = State.Current = Settings->bFirstPerson ? EFacilityView::FirstPerson : EFacilityView::ThirdPerson;
    State.Fov = FMath::Clamp(Settings->FieldOfView, 70.f, 110.f);
    State.Distance = FMath::Clamp(Settings->ChaseDistance, 200.f, 500.f);
    Apply();
    Boom->TargetArmLength = DesiredArmLength;
    Boom->SocketOffset = FVector(0, 0, DesiredSocketHeight);
}

void UFacilityCameraComponent::Apply()
{
    if (!Boom || !Camera) return;
    DesiredArmLength = State.Current == EFacilityView::FirstPerson ? 0.f : State.Current == EFacilityView::Overhead ? 950.f : State.Distance;
    DesiredSocketHeight = State.Current == EFacilityView::Overhead ? 400.f : 0.f;
    Boom->bDoCollisionTest = true;
    Boom->bInheritPitch = State.Current != EFacilityView::Overhead;
    Boom->SetRelativeRotation(State.Current == EFacilityView::Overhead ? FRotator(-70, 0, 0) : FRotator::ZeroRotator);
    Boom->SetRelativeLocation(State.Current == EFacilityView::FirstPerson && Eye ? Eye->GetRelativeLocation() : FVector(0, 0, 68));
    Camera->SetFieldOfView(State.Fov);
    if (AFacilityCharacter* Character = Cast<AFacilityCharacter>(GetOwner()))
        Character->SetHeadHidden(State.Current == EFacilityView::FirstPerson);
}

void UFacilityCameraComponent::TogglePerspective()
{
    State = State.TogglePerspective();
    if (State.Current == EFacilityView::Overhead) return;
    UFacilityUserSettings* Settings = GetMutableDefault<UFacilityUserSettings>();
    Settings->bFirstPerson = State.HandsOn == EFacilityView::FirstPerson;
    Settings->Store();
    Apply();
}

void UFacilityCameraComponent::ToggleManagement() { State = State.ToggleManagement(); Apply(); }
void UFacilityCameraComponent::SetFov(float Fov)
{
    State.Fov = FMath::Clamp(Fov, 70.f, 110.f);
    UFacilityUserSettings* Settings = GetMutableDefault<UFacilityUserSettings>();
    Settings->FieldOfView = State.Fov;
    Settings->Store();
    Apply();
}
void UFacilityCameraComponent::SetDistance(float Distance)
{
    State.Distance = FMath::Clamp(Distance, 200.f, 500.f);
    UFacilityUserSettings* Settings = GetMutableDefault<UFacilityUserSettings>();
    Settings->ChaseDistance = State.Distance;
    Settings->Store();
    Apply();
}

void UFacilityCameraComponent::TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta, TickType, Function);
    if (!Boom) return;
    const float SafeDelta = FMath::Clamp(Delta, 0.f, 0.1f);
    Boom->TargetArmLength = FMath::FInterpTo(Boom->TargetArmLength, DesiredArmLength, SafeDelta, 9.f);
    Boom->SocketOffset.Z = FMath::FInterpTo(Boom->SocketOffset.Z, DesiredSocketHeight, SafeDelta, 9.f);
    if (FMath::Abs(Boom->TargetArmLength - DesiredArmLength) < 0.25f) Boom->TargetArmLength = DesiredArmLength;
}
