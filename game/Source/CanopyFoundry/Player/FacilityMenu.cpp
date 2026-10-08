#include "Player/FacilityMenu.h"
#include "Player/FacilityCharacter.h"
#include "Player/FacilityCameraComponent.h"
#include "Player/FacilityUserSettings.h"
#include "SimulationSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/InputKeySelector.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"

UFacilityMenu::UFacilityMenu(const FObjectInitializer& Initializer) : Super(Initializer)
{
    SetIsFocusable(true);
}

UButton* UFacilityMenu::AddButton(UVerticalBox* Box, const FString& Label)
{
    UButton* Button = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
    Text->SetText(FText::FromString(Label));
    Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    Button->AddChild(Text);
    Box->AddChildToVerticalBox(Button)->SetPadding(FMargin(4));
    return Button;
}

TSharedRef<SWidget> UFacilityMenu::RebuildWidget()
{
    if (!WidgetTree) Initialize();
    if (WidgetTree->RootWidget) return Super::RebuildWidget();
    UBorder* Border = WidgetTree->ConstructWidget<UBorder>();
    Border->SetBrushColor(FLinearColor(0.015f, 0.045f, 0.055f, 0.97f));
    UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
    Border->AddChild(Box);
    WidgetTree->RootWidget = Border;
    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
    Title->SetText(FText::FromString(TEXT("CANOPY FOUNDRY  |  Operations / Preferences")));
    Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    Box->AddChildToVerticalBox(Title)->SetPadding(FMargin(8));
    StatusText = WidgetTree->ConstructWidget<UTextBlock>();
    StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.4f, 1.f, 0.5f)));
    Box->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(8));
    FirstButton = AddButton(Box, TEXT("1x hands-on"));
    FirstButton->OnClicked.AddDynamic(this, &UFacilityMenu::SpeedOne);
    AddButton(Box, TEXT("5x hands-on"))->OnClicked.AddDynamic(this, &UFacilityMenu::SpeedFive);
    AddButton(Box, TEXT("60x management"))->OnClicked.AddDynamic(this, &UFacilityMenu::SpeedSixty);
    AddButton(Box, TEXT("360x management"))->OnClicked.AddDynamic(this, &UFacilityMenu::SpeedThreeSixty);
    AddButton(Box, TEXT("1440x management"))->OnClicked.AddDynamic(this, &UFacilityMenu::SpeedDay);
    AddButton(Box, TEXT("Pause / resume"))->OnClicked.AddDynamic(this, &UFacilityMenu::Pause);
    AddButton(Box, TEXT("Skip to next authored decision"))->OnClicked.AddDynamic(this, &UFacilityMenu::Skip);
    AddButton(Box, TEXT("Resolve pending decision"))->OnClicked.AddDynamic(this, &UFacilityMenu::Resolve);
    AddButton(Box, TEXT("FOV +"))->OnClicked.AddDynamic(this, &UFacilityMenu::FovIncrease);
    AddButton(Box, TEXT("FOV -"))->OnClicked.AddDynamic(this, &UFacilityMenu::FovDecrease);
    AddButton(Box, TEXT("Chase distance +"))->OnClicked.AddDynamic(this, &UFacilityMenu::DistanceIncrease);
    AddButton(Box, TEXT("Chase distance -"))->OnClicked.AddDynamic(this, &UFacilityMenu::DistanceDecrease);
    UTextBlock* PerspectiveText = WidgetTree->ConstructWidget<UTextBlock>();
    PerspectiveText->SetText(FText::FromString(TEXT("Perspective keyboard:")));
    Box->AddChildToVerticalBox(PerspectiveText);
    PerspectiveSelector = WidgetTree->ConstructWidget<UInputKeySelector>();
    PerspectiveSelector->SetAllowGamepadKeys(false);
    PerspectiveSelector->SetAllowModifierKeys(false);
    PerspectiveSelector->SetSelectedKey(FInputChord(GetDefault<UFacilityUserSettings>()->PerspectiveKey));
    PerspectiveSelector->OnKeySelected.AddDynamic(this, &UFacilityMenu::OnPerspective);
    Box->AddChildToVerticalBox(PerspectiveSelector);
    UTextBlock* ManagementText = WidgetTree->ConstructWidget<UTextBlock>();
    ManagementText->SetText(FText::FromString(TEXT("Management keyboard:")));
    Box->AddChildToVerticalBox(ManagementText);
    ManagementSelector = WidgetTree->ConstructWidget<UInputKeySelector>();
    ManagementSelector->SetAllowGamepadKeys(false);
    ManagementSelector->SetAllowModifierKeys(false);
    ManagementSelector->SetSelectedKey(FInputChord(GetDefault<UFacilityUserSettings>()->ManagementKey));
    ManagementSelector->OnKeySelected.AddDynamic(this, &UFacilityMenu::OnManagement);
    Box->AddChildToVerticalBox(ManagementSelector);
    UTextBlock* PadText = WidgetTree->ConstructWidget<UTextBlock>();
    PadText->SetText(FText::FromString(TEXT("Perspective controller:")));
    Box->AddChildToVerticalBox(PadText);
    PerspectivePadSelector = WidgetTree->ConstructWidget<UInputKeySelector>();
    PerspectivePadSelector->SetAllowGamepadKeys(true);
    PerspectivePadSelector->SetAllowModifierKeys(false);
    PerspectivePadSelector->SetSelectedKey(FInputChord(GetDefault<UFacilityUserSettings>()->PerspectiveGamepadKey));
    PerspectivePadSelector->OnKeySelected.AddDynamic(this, &UFacilityMenu::OnPerspectivePad);
    Box->AddChildToVerticalBox(PerspectivePadSelector);
    UTextBlock* PadManagementText = WidgetTree->ConstructWidget<UTextBlock>();
    PadManagementText->SetText(FText::FromString(TEXT("Management controller:")));
    Box->AddChildToVerticalBox(PadManagementText);
    ManagementPadSelector = WidgetTree->ConstructWidget<UInputKeySelector>();
    ManagementPadSelector->SetAllowGamepadKeys(true);
    ManagementPadSelector->SetAllowModifierKeys(false);
    ManagementPadSelector->SetSelectedKey(FInputChord(GetDefault<UFacilityUserSettings>()->ManagementGamepadKey));
    ManagementPadSelector->OnKeySelected.AddDynamic(this, &UFacilityMenu::OnManagementPad);
    Box->AddChildToVerticalBox(ManagementPadSelector);
    AddButton(Box, TEXT("Return to facility"))->OnClicked.AddDynamic(this, &UFacilityMenu::CloseClicked);
    return Super::RebuildWidget();
}

void UFacilityMenu::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    if (const USimulationSubsystem* Sim = GetGameInstance()->GetSubsystem<USimulationSubsystem>())
        StatusText->SetText(FText::FromString(FString::Printf(TEXT("Second %lld | requested %dx / achieved %.1fx | %s"),
            static_cast<long long>(Sim->Second()), Sim->Speed(), Sim->AchievedSpeed(), *Sim->Status())));
}

void UFacilityMenu::Open(AFacilityCharacter* Character)
{
    Player = Character;
    AddToViewport(100);
    APlayerController* Controller = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
    if (Controller)
    {
        FInputModeGameAndUI Mode;
        Mode.SetWidgetToFocus(TakeWidget());
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        Controller->SetInputMode(Mode);
        Controller->bShowMouseCursor = true;
    }
    if (FirstButton) FirstButton->SetKeyboardFocus();
}
void UFacilityMenu::Close()
{
    if (Player)
        if (APlayerController* Controller = Cast<APlayerController>(Player->GetController()))
        {
            Controller->SetInputMode(FInputModeGameOnly());
            Controller->bShowMouseCursor = false;
        }
    RemoveFromParent();
}
void UFacilityMenu::OnPerspective(FInputChord Chord)
{
    if (!Player || Chord.Key.IsGamepadKey() || !Player->RebindPerspective(Chord.Key))
        PerspectiveSelector->SetSelectedKey(FInputChord(GetDefault<UFacilityUserSettings>()->PerspectiveKey));
}
void UFacilityMenu::OnManagement(FInputChord Chord)
{
    if (!Player || Chord.Key.IsGamepadKey() || !Player->RebindManagement(Chord.Key))
        ManagementSelector->SetSelectedKey(FInputChord(GetDefault<UFacilityUserSettings>()->ManagementKey));
}
void UFacilityMenu::OnPerspectivePad(FInputChord Chord)
{
    if (!Player || !Chord.Key.IsGamepadKey() || !Player->RebindPerspective(Chord.Key))
        PerspectivePadSelector->SetSelectedKey(FInputChord(GetDefault<UFacilityUserSettings>()->PerspectiveGamepadKey));
}
void UFacilityMenu::OnManagementPad(FInputChord Chord)
{
    if (!Player || !Chord.Key.IsGamepadKey() || !Player->RebindManagement(Chord.Key))
        ManagementPadSelector->SetSelectedKey(FInputChord(GetDefault<UFacilityUserSettings>()->ManagementGamepadKey));
}
#define FACILITY_SIM_ACTION(Name, Method) void UFacilityMenu::Name() { if (auto* Sim = GetGameInstance()->GetSubsystem<USimulationSubsystem>()) Sim->Method; }
FACILITY_SIM_ACTION(SpeedOne, SetSpeed(1))
FACILITY_SIM_ACTION(SpeedFive, SetSpeed(5))
FACILITY_SIM_ACTION(SpeedSixty, SetSpeed(60))
FACILITY_SIM_ACTION(SpeedThreeSixty, SetSpeed(360))
FACILITY_SIM_ACTION(SpeedDay, SetSpeed(1440))
FACILITY_SIM_ACTION(Skip, SkipToDecision())
FACILITY_SIM_ACTION(Resolve, ResolvePendingDecision())
FACILITY_SIM_ACTION(Pause, TogglePause())
#undef FACILITY_SIM_ACTION
void UFacilityMenu::FovIncrease() { if (Player) Player->CameraModes()->SetFov(GetDefault<UFacilityUserSettings>()->FieldOfView + 5.f); }
void UFacilityMenu::FovDecrease() { if (Player) Player->CameraModes()->SetFov(GetDefault<UFacilityUserSettings>()->FieldOfView - 5.f); }
void UFacilityMenu::DistanceIncrease() { if (Player) Player->CameraModes()->SetDistance(GetDefault<UFacilityUserSettings>()->ChaseDistance + 25.f); }
void UFacilityMenu::DistanceDecrease() { if (Player) Player->CameraModes()->SetDistance(GetDefault<UFacilityUserSettings>()->ChaseDistance - 25.f); }
void UFacilityMenu::CloseClicked() { Close(); }
