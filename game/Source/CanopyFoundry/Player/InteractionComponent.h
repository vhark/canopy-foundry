#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractionComponent.generated.h"
class AFacilityTarget;

UCLASS(ClassGroup=(Canopy), meta=(BlueprintSpawnableComponent))
class UInteractionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    bool Inspect();
    bool SetControl(double Value);
    bool CanReach(const AFacilityTarget& Target) const;
    const FString& Feedback() const { return LastFeedback; }
private:
    AFacilityTarget* FindReachableTarget();
    FString LastFeedback;
};
