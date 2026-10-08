#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Framework/Commands/InputChord.h"
#include "FacilityMenu.generated.h"
class UInputKeySelector;
class UVerticalBox;
class UButton;

UCLASS()
class UFacilityMenu : public UUserWidget
{
    GENERATED_BODY()
public:
    UFacilityMenu(const FObjectInitializer& Initializer);
    void Open(class AFacilityCharacter* Character);
    void Close();
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
private:
    UPROPERTY() TObjectPtr<AFacilityCharacter> Player;
    UPROPERTY() TObjectPtr<UInputKeySelector> PerspectiveSelector;
    UPROPERTY() TObjectPtr<UInputKeySelector> ManagementSelector;
    UPROPERTY() TObjectPtr<UInputKeySelector> PerspectivePadSelector;
    UPROPERTY() TObjectPtr<UInputKeySelector> ManagementPadSelector;
    UPROPERTY() TObjectPtr<class UTextBlock> StatusText;
    UPROPERTY() TObjectPtr<UButton> FirstButton;
    UFUNCTION() void OnPerspective(FInputChord Chord);
    UFUNCTION() void OnManagement(FInputChord Chord);
    UFUNCTION() void OnPerspectivePad(FInputChord Chord);
    UFUNCTION() void OnManagementPad(FInputChord Chord);
    UFUNCTION() void SpeedOne();
    UFUNCTION() void SpeedFive();
    UFUNCTION() void SpeedSixty();
    UFUNCTION() void SpeedThreeSixty();
    UFUNCTION() void SpeedDay();
    UFUNCTION() void Resolve();
    UFUNCTION() void Skip();
    UFUNCTION() void Pause();
    UFUNCTION() void FovIncrease();
    UFUNCTION() void FovDecrease();
    UFUNCTION() void DistanceIncrease();
    UFUNCTION() void DistanceDecrease();
    UFUNCTION() void CloseClicked();
    UButton* AddButton(UVerticalBox* Box, const FString& Label);
};
