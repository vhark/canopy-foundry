#include "FacilityRoomCommandlet.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/LightComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/World.h"
#include "UObject/Package.h"
#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, CanopyFoundryEditor)

UFacilityRoomCommandlet::UFacilityRoomCommandlet()
{
    IsClient = false;
    IsServer = false;
    LogToConsole = true;
}

int32 UFacilityRoomCommandlet::Main(const FString& Params)
{
    if (!GEditor) { UE_LOG(LogTemp, Error, TEXT("Editor world unavailable")); return 1; }
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!Cube) { UE_LOG(LogTemp, Error, TEXT("Required engine primitive cube missing")); return 2; }
    UWorld* World = GEditor->NewMap();
    if (!World) return 3;
    auto Block = [World, Cube](const TCHAR* Label, FVector Position, FVector Size)
    {
        AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Position, FRotator::ZeroRotator);
        Actor->SetActorLabel(Label);
        UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent();
        Mesh->SetStaticMesh(Cube);
        Mesh->SetWorldScale3D(Size / 100.f);
        Mesh->SetCollisionProfileName(TEXT("BlockAll"));
        return Actor;
    };
    // Interior 16m × 12m, 3.5m roof; 1.4m narrow portal in the central divider.
    Block(TEXT("Concrete floor"), {0, 0, -20}, {1600, 1200, 40});
    Block(TEXT("West wall"), {-800, 0, 175}, {30, 1200, 350});
    Block(TEXT("East wall"), {800, 0, 175}, {30, 1200, 350});
    Block(TEXT("North wall"), {0, 600, 175}, {1600, 30, 350});
    Block(TEXT("South wall"), {0, -600, 175}, {1600, 30, 350});
    Block(TEXT("Ceiling (obstructs overhead camera)"), {0, 0, 365}, {1600, 1200, 20});
    Block(TEXT("Doorway north jamb"), {0, 335, 175}, {35, 530, 350});
    Block(TEXT("Doorway south jamb"), {0, -335, 175}, {35, 530, 350});
    Block(TEXT("Doorway lintel"), {0, 0, 305}, {35, 140, 90});
    Block(TEXT("Rolling bench"), {350, 180, 82}, {220, 100, 45});
    Block(TEXT("Bench legs A"), {260, 150, 31}, {15, 15, 62});
    Block(TEXT("Bench legs B"), {440, 150, 31}, {15, 15, 62});
    Block(TEXT("Inspection obstacle"), {340, -300, 105}, {90, 90, 210});
    Block(TEXT("Control corner baffle"), {530, 160, 100}, {30, 120, 200});
    APlayerStart* Start = World->SpawnActor<APlayerStart>(FVector(-480, 0, 100), FRotator(0, 0, 0));
    Start->SetActorLabel(TEXT("Worker start"));
    UClass* TargetClass = LoadClass<AActor>(nullptr, TEXT("/Script/CanopyFoundry.FacilityTarget"));
    if (!TargetClass) { UE_LOG(LogTemp, Error, TEXT("FacilityTarget class unavailable")); return 4; }
    AActor* Target = World->SpawnActor<AActor>(TargetClass, FVector(620, 160, 100), FRotator(0, 180, 0));
    if (!Target) return 5;
    Target->SetActorLabel(TEXT("Inspectable vent control"));
    ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector(0, 0, 250), FRotator(-60, 15, 0));
    Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Sun->GetLightComponent()->SetIntensity(4.f);
    ASkyLight* Sky = World->SpawnActor<ASkyLight>(FVector(0, 0, 250), FRotator::ZeroRotator);
    Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Sky->GetLightComponent()->SetIntensity(1.f);
    for (const float X : {-450.f, 450.f})
    {
        APointLight* Fixture = World->SpawnActor<APointLight>(FVector(X, 0, 315), FRotator::ZeroRotator);
        Fixture->PointLightComponent->SetMobility(EComponentMobility::Movable);
        Fixture->PointLightComponent->SetIntensity(8500.f);
        Fixture->PointLightComponent->SetAttenuationRadius(1200.f);
    }
    const FString Filename = FPaths::ProjectContentDir() / TEXT("Maps/FacilityQualification.umap");
    if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true)) return 6;
    if (!FEditorFileUtils::SaveLevel(World->PersistentLevel, Filename))
    {
        UE_LOG(LogTemp, Error, TEXT("Could not save authored room: %s"), *Filename);
        return 6;
    }
    UE_LOG(LogTemp, Display, TEXT("Authored original qualification room: %s"), *Filename);
    return 0;
}
