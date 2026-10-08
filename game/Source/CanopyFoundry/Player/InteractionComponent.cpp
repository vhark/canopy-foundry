#include "Player/InteractionComponent.h"
#include "Player/FacilityTarget.h"
#include "Player/FacilityCharacter.h"
#include "SimulationSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

AFacilityTarget* UInteractionComponent::FindReachableTarget()
{
    const AFacilityCharacter* Character = Cast<AFacilityCharacter>(GetOwner());
    APlayerController* Player = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
    if (!Player || !GetWorld()) return nullptr;
    FVector Eye; FRotator Aim;
    Player->GetPlayerViewPoint(Eye, Aim);
    FHitResult Focus;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(FacilityFocus), false, Character);
    if (!GetWorld()->LineTraceSingleByChannel(Focus, Eye, Eye + Aim.Vector() * 650.f, ECC_Visibility, Query))
    {
        LastFeedback = TEXT("Aim at the control panel");
        return nullptr;
    }
    AFacilityTarget* Target = Cast<AFacilityTarget>(Focus.GetActor());
    if (!Target || Target->DomainId() != USimulationSubsystem::TargetId)
    {
        LastFeedback = TEXT("Aim at the control panel");
        return nullptr;
    }
    if (!CanReach(*Target))
    {
        LastFeedback = TEXT("Control out of reach or obstructed by facility geometry");
        return nullptr;
    }
    return Target;
}

bool UInteractionComponent::CanReach(const AFacilityTarget& Target) const
{
    const AFacilityCharacter* Character = Cast<AFacilityCharacter>(GetOwner());
    if (!Character || !GetWorld()) return false;
    const FVector Origin = Character->GetActorLocation() + FVector(0, 0, 60);
    const FVector Destination = Target.GetActorLocation() + FVector(0, 0, 25);
    if (FVector::Dist(Origin, Destination) > 220.f) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(FacilityAvatarReach), false, Character);
    FHitResult Obstruction;
    return !GetWorld()->SweepSingleByChannel(Obstruction, Origin, Destination, FQuat::Identity,
        ECC_Visibility, FCollisionShape::MakeSphere(9.f), Query) || Obstruction.GetActor() == &Target;
}

bool UInteractionComponent::Inspect()
{
    AFacilityTarget* Target = FindReachableTarget();
    if (!Target) return false;
    const auto* Simulation = GetWorld()->GetGameInstance()->GetSubsystem<USimulationSubsystem>();
    LastFeedback = Simulation ? FString::Printf(TEXT("Vent control • applied %.0f%% • %s"), Simulation->AppliedControl() * 100., *Simulation->Status()) : TEXT("Vent control unavailable");
    return Simulation != nullptr;
}

bool UInteractionComponent::SetControl(double Value)
{
    if (!FindReachableTarget()) return false;
    auto* Simulation = GetWorld()->GetGameInstance()->GetSubsystem<USimulationSubsystem>();
    if (!Simulation || !Simulation->SubmitControl(Value)) { LastFeedback = TEXT("Simulation busy; control not queued"); return false; }
    LastFeedback = TEXT("Control queued; awaiting authoritative receipt");
    return true;
}
