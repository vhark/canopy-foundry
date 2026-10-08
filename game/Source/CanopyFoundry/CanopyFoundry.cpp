#include "Modules/ModuleManager.h"
#include "FacilityGameMode.h"
#include "Player/FacilityCharacter.h"
#include "Player/InteractionComponent.h"
#include "Player/FacilityCameraComponent.h"
#include "Player/FacilityUserSettings.h"
#include "SimulationSubsystem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"

void AFacilityHUD::DrawHUD()
{
        Super::DrawHUD();
        const USimulationSubsystem* Sim = GetGameInstance()->GetSubsystem<USimulationSubsystem>();
        const AFacilityCharacter* Character = Cast<AFacilityCharacter>(GetOwningPawn());
        if (!Sim || !Character || !Canvas) return;
        const TCHAR* Mode = Character->CameraModes()->IsOverhead() ? TEXT("Overhead") : Character->CameraModes()->CurrentView() == EFacilityView::FirstPerson ? TEXT("First person") : TEXT("Third person");
        const FString Clock = FString::Printf(TEXT("%s | Second %lld | requested %dx / achieved %.1fx | Vent %.0f%% | Revision %llu"),
            Mode, static_cast<long long>(Sim->Second()), Sim->Speed(), Sim->AchievedSpeed(),
            Sim->AppliedControl() * 100., static_cast<unsigned long long>(Sim->Revision()));
        DrawText(Clock, FLinearColor::White, 24, 24, GEngine->GetSmallFont(), 1.25f);
        DrawText(Sim->Status(), FLinearColor(0.4f, 1.f, 0.5f), 24, 54, GEngine->GetSmallFont(), 1.2f);
        const UFacilityUserSettings* Keys = GetDefault<UFacilityUserSettings>();
        if (CachedHints.IsEmpty() || CachedPerspective != Keys->PerspectiveKey.GetFName() ||
            CachedManagement != Keys->ManagementKey.GetFName() ||
            CachedPadPerspective != Keys->PerspectiveGamepadKey.GetFName() ||
            CachedPadManagement != Keys->ManagementGamepadKey.GetFName())
        {
            CachedPerspective = Keys->PerspectiveKey.GetFName();
            CachedManagement = Keys->ManagementKey.GetFName();
            CachedPadPerspective = Keys->PerspectiveGamepadKey.GetFName();
            CachedPadManagement = Keys->ManagementGamepadKey.GetFName();
            CachedHints = FString::Printf(TEXT("WASD / stick move • Mouse / right stick look • %s / %s perspective • %s / %s management"),
                *Keys->PerspectiveKey.ToString(), *Keys->PerspectiveGamepadKey.ToString(),
                *Keys->ManagementKey.ToString(), *Keys->ManagementGamepadKey.ToString());
        }
        DrawText(CachedHints, FLinearColor::White, 24, 84);
        DrawText(TEXT("E / X inspect • F / A set vent 75% • P / B pause • Esc / View options & decisions"), FLinearColor::White, 24, 106);
        if (const UInteractionComponent* Interaction = Character->FindComponentByClass<UInteractionComponent>())
            DrawText(Interaction->Feedback(), FLinearColor(1.f, 0.85f, 0.3f), 24, 135);
}

AFacilityGameMode::AFacilityGameMode()
{
    DefaultPawnClass = AFacilityCharacter::StaticClass();
    HUDClass = AFacilityHUD::StaticClass();
    PrimaryActorTick.bCanEverTick = true;
}

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, CanopyFoundry, "CanopyFoundry");
