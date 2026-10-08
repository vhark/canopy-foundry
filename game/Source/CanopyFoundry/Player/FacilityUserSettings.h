#pragma once
#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UObject/Object.h"
#include "FacilityUserSettings.generated.h"

UCLASS(Config=GameUserSettings)
class UFacilityUserSettings : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(Config) bool bFirstPerson = false;
    UPROPERTY(Config) float FieldOfView = 90.f;
    UPROPERTY(Config) float ChaseDistance = 350.f;
    UPROPERTY(Config) FKey PerspectiveKey = EKeys::V;
    UPROPERTY(Config) FKey ManagementKey = EKeys::Tab;
    UPROPERTY(Config) FKey PerspectiveGamepadKey = EKeys::Gamepad_FaceButton_Top;
    UPROPERTY(Config) FKey ManagementGamepadKey = EKeys::Gamepad_Special_Right;
    void Store() { SaveConfig(); }
};
