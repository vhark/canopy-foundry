#include "FacilityGameMode.h"
#include "QualificationReport.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformMisc.h"

namespace
{
    // Independently derived semantic parent Rz(90) T(1.7,.9,.4), child mirror-X
    // T(.25,.1,.15): p_semantic=(1.6-y, 1.15-x, .55+z) metres.
    // The bridge is (100*x, -100*y, 100*z), NOT the GLB importer basis.
    const FVector Corners[] = {
        {160., -115., 55.}, {160., -35., 55.}, {115., -115., 55.}, {160., -115., 85.}
    };
    struct FExpectedPort
    {
        FName Tag;
        FVector Position;
        FVector Normal;
    };
    const FExpectedPort Ports[] = {
        {TEXT("CookedPortSupply"), {152., -35., 65.}, {0., 1., 0.}},
        {TEXT("CookedPortReturn"), {115., -103., 75.}, {-1., 0., 0.}}
    };
    bool Near(const FVector& Actual, const FVector& Expected, double Tolerance = .8)
    {
        return FVector::Dist(Actual, Expected) <= Tolerance;
    }
}

void AFacilityGameMode::QualifyCookedFiducial()
{
    const auto Fail = [](const TCHAR* Reason)
    {
        UE_LOG(LogTemp, Error, TEXT("B02 cooked fiducial FAIL: %s"), Reason);
        FPlatformMisc::RequestExitWithStatus(false, 8);
    };
    AStaticMeshActor* Fixture = nullptr;
    AActor* PortActors[2]{};
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* Actor = *It;
        if (Actor->ActorHasTag(TEXT("CookedFiducialMesh")))
        {
            if (Fixture || !Actor->IsA<AStaticMeshActor>()) { Fail(TEXT("Missing or duplicate imported fixture actor")); return; }
            Fixture = CastChecked<AStaticMeshActor>(Actor);
        }
        for (int32 I = 0; I < 2; ++I)
        {
            if (Actor->ActorHasTag(Ports[I].Tag))
            {
                if (PortActors[I]) { Fail(TEXT("Duplicate cooked port frame")); return; }
                PortActors[I] = Actor;
            }
        }
    }
    if (!Fixture || !Fixture->GetStaticMeshComponent()) { Fail(TEXT("GLB imported mesh actor absent")); return; }
    UStaticMeshComponent* Component = Fixture->GetStaticMeshComponent();
    UStaticMesh* Mesh = Component->GetStaticMesh();
    if (!Mesh || !Mesh->bAllowCPUAccess || !Mesh->GetRenderData() || Mesh->GetRenderData()->LODResources.IsEmpty() ||
        !Mesh->GetBodySetup() || Mesh->GetBodySetup()->AggGeom.ConvexElems.Num() != 1)
    { Fail(TEXT("Imported cooked render LOD / CPU access / GLB convex collision absent")); return; }
    const FStaticMeshLODResources& LOD = Mesh->GetRenderData()->LODResources[0];
    const FPositionVertexBuffer& Positions = LOD.VertexBuffers.PositionVertexBuffer;
    const FStaticMeshVertexBuffer& Normals = LOD.VertexBuffers.StaticMeshVertexBuffer;
    if (Positions.GetNumVertices() < 18 || Normals.GetNumVertices() != Positions.GetNumVertices())
    { Fail(TEXT("Imported fiducial render vertices / asymmetric port triangles absent")); return; }
    for (const FVector& Corner : Corners)
    {
        int32 Found = 0;
        for (uint32 I = 0; I < Positions.GetNumVertices(); ++I)
        {
            if (Near(Component->GetComponentTransform().TransformPosition(FVector(Positions.VertexPosition(I))), Corner)) ++Found;
        }
        if (!Found) { Fail(TEXT("Cooked GLB render corner differs from independently derived semantic->UE cm")); return; }
    }
    const FKConvexElem& Collision = Mesh->GetBodySetup()->AggGeom.ConvexElems[0];
    // Interchange preserves duplicated hard-normal source vertices in VertexData.
    // Require the four expected geometric corners, not four raw entries.
    bool FoundCorners[UE_ARRAY_COUNT(Corners)]{};
    for (const FVector& Vertex : Collision.VertexData)
    {
        const FVector Position = Component->GetComponentTransform().TransformPosition(Vertex);
        bool Matched = false;
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Corners); ++Index)
        {
            if (Near(Position, Corners[Index])) { FoundCorners[Index] = true; Matched = true; break; }
        }
        if (!Matched) { Fail(TEXT("Cooked convex body contains an unexpected geometric corner")); return; }
    }
    int32 UniqueCorners = 0;
    for (bool Found : FoundCorners)
    {
        if (!Found) { Fail(TEXT("Cooked imported custom convex vertex differs from semantic->UE cm")); return; }
        ++UniqueCorners;
    }
    FHitResult Hit;
    const bool Blocked = GetWorld()->LineTraceSingleByChannel(Hit, {100., -95., 62.}, {200., -95., 62.}, ECC_Visibility);
    if (!Blocked || Hit.GetComponent() != Component)
    { Fail(TEXT("Cooked imported GLB convex body does not block its interior ray")); return; }
    for (int32 P = 0; P < 2; ++P)
    {
        const FExpectedPort& Port = Ports[P];
        if (!PortActors[P] || !PortActors[P]->GetRootComponent() ||
            !Near(PortActors[P]->GetActorLocation(), Port.Position) ||
            !Near(PortActors[P]->GetActorForwardVector(), Port.Normal, .01))
        { Fail(P == 0 ? TEXT("Supply port transform/normal failed") : TEXT("Return port transform/normal failed")); return; }
        FVector TriangleCenter = FVector::ZeroVector;
        int32 Count = 0;
        for (uint32 I = 0; I < Positions.GetNumVertices(); ++I)
        {
            const FVector4f PackedNormal = Normals.VertexTangentZ(I);
            const FVector Normal = Component->GetComponentTransform().TransformVectorNoScale(
                FVector(PackedNormal.X, PackedNormal.Y, PackedNormal.Z)).GetSafeNormal();
            const FVector Vertex = Component->GetComponentTransform().TransformPosition(FVector(Positions.VertexPosition(I)));
            if (FVector::DotProduct(Normal, Port.Normal) > .97 && FVector::Dist(Vertex, Port.Position) < 12.)
            {
                TriangleCenter += Vertex;
                ++Count;
            }
        }
        if (Count != 3 || !Near(TriangleCenter / 3., Port.Position))
        { Fail(P == 0 ? TEXT("Cooked supply render port center/normal failed") : TEXT("Cooked return render port center/normal failed")); return; }
    }
    const TSharedRef<FJsonObject> Measurements = MakeShared<FJsonObject>();
    Measurements->SetNumberField(TEXT("renderVertices"), Positions.GetNumVertices());
    Measurements->SetNumberField(TEXT("collisionVertices"), Collision.VertexData.Num());
    Measurements->SetNumberField(TEXT("collisionUniqueCorners"), UniqueCorners);
    Measurements->SetBoolField(TEXT("collisionRayBlocked"), Blocked);
    const auto Point = [](const FVector& Position) -> TSharedPtr<FJsonValue>
    {
        return MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
            MakeShared<FJsonValueNumber>(Position.X),
            MakeShared<FJsonValueNumber>(Position.Y),
            MakeShared<FJsonValueNumber>(Position.Z)});
    };
    TArray<TSharedPtr<FJsonValue>> ActualPorts;
    for (const AActor* Port : PortActors)
    {
        const TSharedRef<FJsonObject> Frame = MakeShared<FJsonObject>();
        Frame->SetField(TEXT("position"), Point(Port->GetActorLocation()));
        Frame->SetField(TEXT("direction"), Point(Port->GetActorForwardVector()));
        ActualPorts.Add(MakeShared<FJsonValueObject>(Frame));
    }
    Measurements->SetArrayField(TEXT("ports"), ActualPorts);
    if (!WriteQualificationReport(TEXT("b02-cooked-fiducial"), Measurements))
    { Fail(TEXT("Could not publish fresh cooked-fiducial report")); return; }
    UE_LOG(LogTemp, Display, TEXT("B02 cooked fiducial PASS: imported GLB render vertices, actual convex collider/raycast, asymmetric XYZ and both cooked port frames/normals in independently derived UE centimetres"));
    FPlatformMisc::RequestExitWithStatus(false, 0);
}
