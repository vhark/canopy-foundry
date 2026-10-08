#include "Player/FacilityCharacter.h"
#include "Player/FacilityCameraComponent.h"
#include "Player/FacilityUserSettings.h"
#include "Player/FacilityMenu.h"
#include "Player/InteractionComponent.h"
#include "Player/FacilityTarget.h"
#include "SimulationSubsystem.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputModifiers.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "UObject/ConstructorHelpers.h"

AFacilityCharacter::AFacilityCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(34.f, 92.f);
    bUseControllerRotationYaw = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->MaxWalkSpeed = 360.f;
    EyeAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("EyeAnchor"));
    EyeAnchor->SetupAttachment(GetCapsuleComponent());
    EyeAnchor->SetRelativeLocation(FVector(0, 0, 68));
    Camera = CreateDefaultSubobject<UFacilityCameraComponent>(TEXT("FacilityCameraModes"));
    Interaction = CreateDefaultSubobject<UInteractionComponent>(TEXT("Interaction"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMeshComponent* Torso = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WorkerJacket"));
    Torso->SetupAttachment(GetCapsuleComponent());
    if (Cube.Succeeded()) Torso->SetStaticMesh(Cube.Object);
    Torso->SetRelativeLocation(FVector(0, 0, -10));
    Torso->SetRelativeScale3D(FVector(.38, .25, .55));
    Torso->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WorkerHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WorkerHead"));
    WorkerHead->SetupAttachment(GetCapsuleComponent());
    if (Sphere.Succeeded()) WorkerHead->SetStaticMesh(Sphere.Object);
    WorkerHead->SetRelativeLocation(FVector(0, 0, 64));
    WorkerHead->SetRelativeScale3D(FVector(.25));
    WorkerHead->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    for (int32 Index = 0; Index < 2; ++Index)
    {
        UStaticMeshComponent* Leg = CreateDefaultSubobject<UStaticMeshComponent>(Index ? TEXT("WorkerLeftLeg") : TEXT("WorkerRightLeg"));
        Leg->SetupAttachment(GetCapsuleComponent());
        if (Cube.Succeeded()) Leg->SetStaticMesh(Cube.Object);
        Leg->SetRelativeLocation(FVector(0, Index ? -13.f : 13.f, -60));
        Leg->SetRelativeScale3D(FVector(.16, .16, .65));
        Leg->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        UStaticMeshComponent* Arm = CreateDefaultSubobject<UStaticMeshComponent>(Index ? TEXT("WorkerLeftArm") : TEXT("WorkerRightArm"));
        Arm->SetupAttachment(GetCapsuleComponent());
        if (Cube.Succeeded()) Arm->SetStaticMesh(Cube.Object);
        Arm->SetRelativeLocation(FVector(0, Index ? -33.f : 33.f, 3));
        Arm->SetRelativeScale3D(FVector(.13, .13, .45));
        Arm->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}

void AFacilityCharacter::BeginPlay()
{
    Super::BeginPlay();
    Camera->Setup(GetCapsuleComponent(), EyeAnchor);
    if (!Mappings) MapActions();
    for (TActorIterator<AFacilityTarget> It(GetWorld()); It; ++It)
        if (It->DomainId() == USimulationSubsystem::TargetId) { VisibleTarget = *It; break; }
}

void AFacilityCharacter::SetHeadHidden(bool bHidden)
{
    WorkerHead->SetOwnerNoSee(bHidden);
}

void AFacilityCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const USimulationSubsystem* Sim = GetGameInstance()->GetSubsystem<USimulationSubsystem>();
    if (!Sim) return;
    if (Sim->IsManagementTimeLapse()) GetCharacterMovement()->StopMovementImmediately();
    if (VisibleTarget && DisplayedFraction != Sim->AppliedControl())
    {
        DisplayedFraction = Sim->AppliedControl();
        VisibleTarget->DisplayControl(DisplayedFraction);
    }
}

void AFacilityCharacter::MapActions()
{
    Mappings = NewObject<UInputMappingContext>(this);
    auto MakeAction = [this](EInputActionValueType Type)
    {
        UInputAction* Action = NewObject<UInputAction>(this);
        Action->ValueType = Type;
        return Action;
    };
    MoveAction = MakeAction(EInputActionValueType::Axis2D);
    LookAction = MakeAction(EInputActionValueType::Axis2D);
    PerspectiveAction = MakeAction(EInputActionValueType::Boolean);
    ManagementAction = MakeAction(EInputActionValueType::Boolean);
    InspectAction = MakeAction(EInputActionValueType::Boolean);
    ControlAction = MakeAction(EInputActionValueType::Boolean);
    PauseAction = MakeAction(EInputActionValueType::Boolean);
    MenuAction = MakeAction(EInputActionValueType::Boolean);
    auto Axis = [this](const UInputAction* Action, FKey Key, bool bY, bool bNegate)
    {
        auto& Binding = Mappings->MapKey(Action, Key);
        if (bY) Binding.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(Mappings));
        if (bNegate) Binding.Modifiers.Add(NewObject<UInputModifierNegate>(Mappings));
    };
    Axis(MoveAction, EKeys::W, true, false);
    Axis(MoveAction, EKeys::S, true, true);
    Axis(MoveAction, EKeys::D, false, false);
    Axis(MoveAction, EKeys::A, false, true);
    Mappings->MapKey(MoveAction, EKeys::Gamepad_Left2D);
    Axis(LookAction, EKeys::MouseX, false, false);
    Axis(LookAction, EKeys::MouseY, true, true);
    Mappings->MapKey(LookAction, EKeys::Gamepad_Right2D);
    Mappings->MapKey(PerspectiveAction, GetDefault<UFacilityUserSettings>()->PerspectiveKey);
    Mappings->MapKey(PerspectiveAction, GetDefault<UFacilityUserSettings>()->PerspectiveGamepadKey);
    Mappings->MapKey(ManagementAction, GetDefault<UFacilityUserSettings>()->ManagementKey);
    Mappings->MapKey(ManagementAction, GetDefault<UFacilityUserSettings>()->ManagementGamepadKey);
    Mappings->MapKey(InspectAction, EKeys::E);
    Mappings->MapKey(InspectAction, EKeys::Gamepad_FaceButton_Left);
    Mappings->MapKey(ControlAction, EKeys::F);
    Mappings->MapKey(ControlAction, EKeys::Gamepad_FaceButton_Bottom);
    Mappings->MapKey(PauseAction, EKeys::P);
    Mappings->MapKey(PauseAction, EKeys::Gamepad_FaceButton_Right);
    Mappings->MapKey(MenuAction, EKeys::Escape);
    Mappings->MapKey(MenuAction, EKeys::Gamepad_Special_Left);
    if (const auto* Controller = Cast<APlayerController>(GetController()))
        if (auto* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer()))
            Input->AddMappingContext(Mappings, 0);
}

void AFacilityCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    // Actions are constructed before possession in BeginPlay for the qualification map.
    if (auto* Enhanced = Cast<UEnhancedInputComponent>(Input))
    {
        if (!MoveAction) MapActions();
        Enhanced->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AFacilityCharacter::Move);
        Enhanced->BindAction(LookAction, ETriggerEvent::Triggered, this, &AFacilityCharacter::Look);
        Enhanced->BindAction(PerspectiveAction, ETriggerEvent::Started, this, &AFacilityCharacter::Perspective);
        Enhanced->BindAction(ManagementAction, ETriggerEvent::Started, this, &AFacilityCharacter::Management);
        Enhanced->BindAction(InspectAction, ETriggerEvent::Started, this, &AFacilityCharacter::Inspect);
        Enhanced->BindAction(ControlAction, ETriggerEvent::Started, this, &AFacilityCharacter::Control);
        Enhanced->BindAction(PauseAction, ETriggerEvent::Started, this, &AFacilityCharacter::Pause);
        Enhanced->BindAction(MenuAction, ETriggerEvent::Started, this, &AFacilityCharacter::ToggleMenu);
        if (const auto* Controller = Cast<APlayerController>(GetController()))
            if (auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer()))
                Subsystem->AddMappingContext(Mappings, 0);
    }
}
void AFacilityCharacter::Move(const FInputActionValue& Value)
{
    const auto* Sim = GetGameInstance()->GetSubsystem<USimulationSubsystem>();
    if (Menu && Menu->IsInViewport()) return;
    if (Camera->IsOverhead() || (Sim && Sim->IsManagementTimeLapse())) return;
    FVector2D Axis = Value.Get<FVector2D>();
    const FRotator Yaw(0, GetControlRotation().Yaw, 0);
    AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Axis.Y);
    AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Axis.X);
}
void AFacilityCharacter::Look(const FInputActionValue& Value)
{
    if (Menu && Menu->IsInViewport()) return;
    const FVector2D Axis = Value.Get<FVector2D>();
    AddControllerYawInput(Axis.X);
    AddControllerPitchInput(Axis.Y);
}
void AFacilityCharacter::Perspective() { if (!Menu || !Menu->IsInViewport()) Camera->TogglePerspective(); }
void AFacilityCharacter::Management() { if (!Menu || !Menu->IsInViewport()) Camera->ToggleManagement(); }
void AFacilityCharacter::Inspect()
{
    if (Menu && Menu->IsInViewport()) return;
    if (!Camera->IsOverhead() && !GetGameInstance()->GetSubsystem<USimulationSubsystem>()->IsManagementTimeLapse())
        Interaction->Inspect();
}
void AFacilityCharacter::Control()
{
    if (Menu && Menu->IsInViewport()) return;
    if (!Camera->IsOverhead() && !GetGameInstance()->GetSubsystem<USimulationSubsystem>()->IsManagementTimeLapse())
        Interaction->SetControl(0.75);
}
void AFacilityCharacter::Pause()
{
    if (!Menu || !Menu->IsInViewport()) GetGameInstance()->GetSubsystem<USimulationSubsystem>()->TogglePause();
}
void AFacilityCharacter::ToggleMenu()
{
    if (Menu && Menu->IsInViewport()) { Menu->Close(); return; }
    if (!Menu) Menu = CreateWidget<UFacilityMenu>(Cast<APlayerController>(GetController()), UFacilityMenu::StaticClass());
    if (Menu) Menu->Open(this);
}
bool AFacilityCharacter::IsViewKeyAvailable(const UInputMappingContext& Context, const UInputAction& View, const FKey& Key)
{
    if (!Key.IsValid()) return false;
    for (const FEnhancedActionKeyMapping& Mapping : Context.GetMappings())
        if (Mapping.Key == Key && Mapping.Action != &View) return false;
    return true;
}

bool AFacilityCharacter::RebindPerspective(const FKey& Key)
{
    auto* Settings = GetMutableDefault<UFacilityUserSettings>();
    const bool bGamepad = Key.IsGamepadKey();
    FKey& Current = bGamepad ? Settings->PerspectiveGamepadKey : Settings->PerspectiveKey;
    if (Key == Current) return true;
    if (!IsViewKeyAvailable(*Mappings, *PerspectiveAction, Key)) return false;
    Mappings->UnmapKey(PerspectiveAction, Current);
    Mappings->MapKey(PerspectiveAction, Key);
    Current = Key; Settings->Store();
    if (auto* Controller = Cast<APlayerController>(GetController()))
        if (auto* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer())) Input->RequestRebuildControlMappings();
    return true;
}
bool AFacilityCharacter::RebindManagement(const FKey& Key)
{
    auto* Settings = GetMutableDefault<UFacilityUserSettings>();
    const bool bGamepad = Key.IsGamepadKey();
    FKey& Current = bGamepad ? Settings->ManagementGamepadKey : Settings->ManagementKey;
    if (Key == Current) return true;
    if (!IsViewKeyAvailable(*Mappings, *ManagementAction, Key)) return false;
    Mappings->UnmapKey(ManagementAction, Current);
    Mappings->MapKey(ManagementAction, Key);
    Current = Key; Settings->Store();
    if (auto* Controller = Cast<APlayerController>(GetController()))
        if (auto* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer())) Input->RequestRebuildControlMappings();
    return true;
}
