#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "FacilityGameMode.generated.h"

UCLASS()
class AFacilityHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
private:
    FString CachedHints;
    FName CachedPerspective;
    FName CachedManagement;
    FName CachedPadPerspective;
    FName CachedPadManagement;
};

UCLASS()
class AFacilityGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AFacilityGameMode();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
private:
    bool bQualifying{};
    int32 QualificationStage{};
    float QualificationElapsed{};
    uint64 InitialRevision{};
    int64 InitialSecond{};
    void FailQualification(const TCHAR* Reason);
    void QualifyCookedFiducial();
};
