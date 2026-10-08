#include "FacilityGameMode.h"
#include "QualificationReport.h"
#include "Player/FacilityCharacter.h"
#include "Player/FacilityCameraComponent.h"
#include "Player/FacilityTarget.h"
#include "Player/InteractionComponent.h"
#include "SimulationSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"

void AFacilityGameMode::BeginPlay()
{
    Super::BeginPlay();
    if (FParse::Param(FCommandLine::Get(), TEXT("CookedFiducialQualify")))
    {
        if (FParse::Param(FCommandLine::Get(), TEXT("FacilityQualify")))
        {
            UE_LOG(LogTemp, Error, TEXT("B02 cooked fiducial FAIL: qualification modes must run separately"));
            FPlatformMisc::RequestExitWithStatus(false, 8);
            return;
        }
        QualifyCookedFiducial();
        return;
    }
    bQualifying = FParse::Param(FCommandLine::Get(), TEXT("FacilityQualify"));
    PrimaryActorTick.bCanEverTick = bQualifying;
    if (bQualifying) UE_LOG(LogTemp, Display, TEXT("F03 native room qualification START"));
}

void AFacilityGameMode::FailQualification(const TCHAR* Reason)
{
    UE_LOG(LogTemp, Error, TEXT("F03 native room qualification FAIL: %s"), Reason);
    bQualifying = false;
    FPlatformMisc::RequestExitWithStatus(false, 7);
}

void AFacilityGameMode::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!bQualifying) return;
    QualificationElapsed += DeltaTime;
    if (QualificationElapsed > 30.f) { FailQualification(TEXT("Timed out waiting for worker/scene")); return; }
    USimulationSubsystem* Sim = GetGameInstance()->GetSubsystem<USimulationSubsystem>();
    AFacilityCharacter* Worker = Cast<AFacilityCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Sim || !Worker || !Worker->CameraModes()) return;
    switch (QualificationStage)
    {
    case 0:
    {
        if (Sim->Status() == TEXT("Loading simulation")) return;
        if (Sim->Status() == TEXT("Simulation model failed to load")) { FailQualification(TEXT("World load failed")); return; }
        FHitResult Hit;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(FacilityRoomQualify));
        const bool DoorPass = !GetWorld()->LineTraceSingleByChannel(Hit, {-130, 0, 110}, {130, 0, 110}, ECC_Visibility, Query);
        const bool WallBlocks = GetWorld()->LineTraceSingleByChannel(Hit, {-130, 220, 110}, {130, 220, 110}, ECC_Visibility, Query);
        const bool BenchBlocks = GetWorld()->LineTraceSingleByChannel(Hit, {350, 180, 210}, {350, 180, 45}, ECC_Visibility, Query);
        AFacilityTarget* Target = nullptr;
        for (TActorIterator<AFacilityTarget> It(GetWorld()); It; ++It) { Target = *It; break; }
        UInteractionComponent* Reach = Worker->FindComponentByClass<UInteractionComponent>();
        if (!Target || !Reach) { FailQualification(TEXT("Inspectable target or reach policy missing")); return; }
        const FVector StartPosition = Worker->GetActorLocation();
        Worker->SetActorLocation({480, 40, 92}, false, nullptr, ETeleportType::TeleportPhysics);
        const bool AvatarCornerDenied = !Reach->CanReach(*Target);
        Worker->SetActorLocation({620, 300, 92}, false, nullptr, ETeleportType::TeleportPhysics);
        const bool AvatarDirectAllowed = Reach->CanReach(*Target);
        Worker->SetActorLocation(StartPosition, false, nullptr, ETeleportType::TeleportPhysics);
        const bool CameraCornerClear = !GetWorld()->LineTraceSingleByChannel(Hit, {620, 310, 145}, {620, 240, 145}, ECC_Visibility, Query);
        if (!AvatarCornerDenied || !AvatarDirectAllowed || !CameraCornerClear)
        { FailQualification(TEXT("Around-corner avatar reach/visibility policy failed")); return; }
        if (!DoorPass || !WallBlocks || !BenchBlocks) { FailQualification(TEXT("Narrow doorway/wall/bench geometry or collision missing")); return; }
        InitialSecond = Sim->Second(); InitialRevision = Sim->Revision();
        AFacilityCharacter* SameActor = Worker;
        UFacilityCameraComponent* Camera = Worker->CameraModes();
        const EFacilityView Previous = Camera->CurrentView();
        Camera->TogglePerspective();
        if (Camera->CurrentView() == Previous) { FailQualification(TEXT("Perspective toggle failed")); return; }
        Camera->ToggleManagement();
        if (!Camera->IsOverhead()) { FailQualification(TEXT("Overhead toggle failed")); return; }
        Camera->ToggleManagement();
        if (Camera->CurrentView() == EFacilityView::Overhead || SameActor != UGameplayStatics::GetPlayerPawn(this, 0) ||
            InitialSecond != Sim->Second() || InitialRevision != Sim->Revision())
        { FailQualification(TEXT("View-only switch changed pawn or domain authority")); return; }
        if (!Sim->SubmitControl(0.75)) { FailQualification(TEXT("SetControl queue rejected")); return; }
        QualificationStage = 1;
        break;
    }
    case 1:
        if (Sim->Revision() < InitialRevision + 1) return;
        if (Sim->Second() != InitialSecond || Sim->AppliedControl() != .25)
        { FailQualification(TEXT("Control mutated applied state without tick")); return; }
        if (!Sim->SkipToDecision()) { FailQualification(TEXT("Decision skip rejected")); return; }
        QualificationStage = 2;
        break;
    case 2:
        if (Sim->Second() < 60) return;
        if (Sim->Second() != 60 || Sim->Speed() != 0 || Sim->AppliedControl() != .75)
        { FailQualification(TEXT("Decision stop, applied control or achieved speed incorrect")); return; }
        if (!Sim->ResolvePendingDecision()) { FailQualification(TEXT("Decision resolution queue rejected")); return; }
        QualificationStage = 3;
        break;
    case 3:
        if (Sim->Revision() < InitialRevision + 2) return;
        Sim->TogglePause(); // queue resume after the decision resolution
        Sim->TogglePause(); // queue pause in order; no fractional sim time is granted
        if (!Sim->SetSpeed(1) || !Sim->SubmitControl(.5)) { FailQualification(TEXT("Pending-work shutdown setup rejected")); return; }
        QualificationStage = 4;
        break;
    case 4:
        if (Sim->Revision() < InitialRevision + 3) return;
        if (Sim->Second() != 60) { FailQualification(TEXT("Pause/resume advanced simulation unexpectedly")); return; }
        // Subsystem deinitialization joins the worker after every queued intent is delivered.
        if (!Sim->SubmitControl(.25)) { FailQualification(TEXT("Shutdown command not queued")); return; }
        {
            const TSharedRef<FJsonObject> Measurements = MakeShared<FJsonObject>();
            Measurements->SetNumberField(TEXT("second"), Sim->Second());
            Measurements->SetNumberField(TEXT("revision"), Sim->Revision());
            Measurements->SetNumberField(TEXT("appliedControl"), Sim->AppliedControl());
            Measurements->SetNumberField(TEXT("completedStage"), QualificationStage);
            if (!WriteQualificationReport(TEXT("f03-native-room"), Measurements))
            { FailQualification(TEXT("Could not publish fresh qualification report")); return; }
        }
        UE_LOG(LogTemp, Display, TEXT("F03 native room qualification PASS: room collision, view-only invariance, control, authored decision, pause/resume, queued shutdown; interactive camera obstruction and preferences still require external input/relaunch observations"));
        bQualifying = false;
        FPlatformMisc::RequestExitWithStatus(false, 0);
        break;
    }
}
