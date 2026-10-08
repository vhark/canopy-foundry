#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "FacilityCharacter.generated.h"
class UInputAction;
class UInputMappingContext;
class UInteractionComponent;
class UFacilityCameraComponent;
class UFacilityMenu;
class AFacilityTarget;
class UStaticMeshComponent;
class USceneComponent;

UCLASS()
class AFacilityCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    AFacilityCharacter();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    bool RebindPerspective(const FKey& Key);
    bool RebindManagement(const FKey& Key);
    static bool IsViewKeyAvailable(const UInputMappingContext& Context, const UInputAction& View, const FKey& Key);
    UFacilityCameraComponent* CameraModes() const { return Camera; }
    void SetHeadHidden(bool bHidden);
private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UFacilityCameraComponent> Camera;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInteractionComponent> Interaction;
    UPROPERTY() TObjectPtr<UInputMappingContext> Mappings;
    UPROPERTY() TObjectPtr<UInputAction> MoveAction;
    UPROPERTY() TObjectPtr<UInputAction> LookAction;
    UPROPERTY() TObjectPtr<UInputAction> PerspectiveAction;
    UPROPERTY() TObjectPtr<UInputAction> ManagementAction;
    UPROPERTY() TObjectPtr<UInputAction> InspectAction;
    UPROPERTY() TObjectPtr<UInputAction> ControlAction;
    UPROPERTY() TObjectPtr<UInputAction> PauseAction;
    UPROPERTY() TObjectPtr<UInputAction> MenuAction;
    UPROPERTY() TObjectPtr<UFacilityMenu> Menu;
    UPROPERTY() TObjectPtr<USceneComponent> EyeAnchor;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> WorkerHead;
    UPROPERTY() TObjectPtr<AFacilityTarget> VisibleTarget;
    double DisplayedFraction{-1.0};
    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void Perspective();
    void Management();
    void Inspect();
    void Control();
    void Pause();
    void ToggleMenu();
    void MapActions();
};
