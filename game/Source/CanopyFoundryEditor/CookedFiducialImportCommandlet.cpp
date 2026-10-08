#include "CookedFiducialImportCommandlet.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "InterchangeManager.h"
#include "InterchangeGenericAssetsPipeline.h"
#include "InterchangeGenericAssetsPipelineSharedSettings.h"
#include "InterchangeGenericMeshPipeline.h"
#include "InterchangeMeshDefinitions.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "HAL/FileManager.h"
#include "PlatformCryptoContextIncludes.h"
#include "Runtime/Launch/Resources/Version.h"

UCookedFiducialImportCommandlet::UCookedFiducialImportCommandlet()
{
    IsClient = false;
    IsServer = false;
    LogToConsole = true;
}

namespace
{
    bool ReadTriple(const TArray<TSharedPtr<FJsonValue>>* Values, FVector& Out)
    {
        if (!Values || Values->Num() != 3) return false;
        for (const auto& Value : *Values) if (!Value.IsValid() || Value->Type != EJson::Number) return false;
        Out = FVector((*Values)[0]->AsNumber(), (*Values)[1]->AsNumber(), (*Values)[2]->AsNumber());
        return !Out.ContainsNaN();
    }
    // This is GLTFCore/Private/GLTF/ConversionUtilities.h ConvertVec3,
    // followed by InterchangeGltfMesh.cpp's metre-to-centimetre multiplier.
    FVector Imported(const FVector& GLB) { return FVector(GLB.X, GLB.Z, GLB.Y) * 100.; }
}

int32 UCookedFiducialImportCommandlet::Main(const FString& Params)
{
    FString FixtureDir;
    if (!FParse::Value(*Params, TEXT("FixtureDir="), FixtureDir) || FixtureDir.IsEmpty())
    { UE_LOG(LogTemp, Error, TEXT("B02 import FAIL: -FixtureDir=<generated-private-dir> required")); return 20; }
    const FString GLBPath = FPaths::ConvertRelativePathToFull(FixtureDir / TEXT("CookedFiducial.glb"));
    const FString BasisPath = FPaths::ConvertRelativePathToFull(FixtureDir / TEXT("CookedFiducial.basis.json"));
    const FString MapFile = FPaths::ProjectContentDir() / TEXT("Maps/FacilityQualification.umap");
    if (!IFileManager::Get().FileExists(*GLBPath) || !IFileManager::Get().FileExists(*MapFile))
    { UE_LOG(LogTemp, Error, TEXT("B02 import FAIL: GLB or F03 authored map absent: %s / %s"), *GLBPath, *MapFile); return 21; }
    FString Text;
    TSharedPtr<FJsonObject> Basis;
    if (!FFileHelper::LoadFileToString(Text, *BasisPath) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Basis) || !Basis.IsValid())
    { UE_LOG(LogTemp, Error, TEXT("B02 import FAIL: missing/invalid source-export-import basis: %s"), *BasisPath); return 22; }
    FString Format, Source, Export, Import;
    if (!Basis->TryGetStringField(TEXT("format"), Format) || Format != TEXT("canopy-cooked-fiducial-1") ||
        !Basis->TryGetStringField(TEXT("source_basis"), Source) || !Source.Contains(TEXT("RH Z-up metres")) ||
        !Basis->TryGetStringField(TEXT("export_basis"), Export) || !Export.Contains(TEXT("RH Y-up metres")) ||
        !Basis->TryGetStringField(TEXT("import_basis"), Import) ||
        !Import.StartsWith(FString::Printf(TEXT("UE %d.%d.%d Interchange "), ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION, ENGINE_PATCH_VERSION)))
    { UE_LOG(LogTemp, Error, TEXT("B02 import FAIL: basis stages missing or mismatched")); return 23; }
    FString ExpectedDigest;
    TArray<uint8> GLBBytes;
    FEncryptionContext Crypto;
    TArray<uint8> Digest;
    if (!Basis->TryGetStringField(TEXT("glb_sha256"), ExpectedDigest) || ExpectedDigest.Len() != 64 ||
        !FFileHelper::LoadFileToArray(GLBBytes, *GLBPath) || GLBBytes.IsEmpty() ||
        !Crypto.CalcSHA256(MakeArrayView(GLBBytes), Digest) ||
        !BytesToHex(Digest.GetData(), Digest.Num()).Equals(ExpectedDigest, ESearchCase::IgnoreCase))
    { UE_LOG(LogTemp, Error, TEXT("B02 import FAIL: sidecar GLB SHA-256 mismatch")); return 23; }

    UInterchangeManager& Manager = UInterchangeManager::GetInterchangeManager();
    UInterchangeSourceData* SourceData = UInterchangeManager::CreateSourceData(GLBPath);
    if (!SourceData || !Manager.CanTranslateSourceData(SourceData))
    { UE_LOG(LogTemp, Error, TEXT("B02 import FAIL: Interchange GLB translator unavailable")); return 24; }
    TStrongObjectPtr<UInterchangeGenericAssetsPipeline> Pipeline(NewObject<UInterchangeGenericAssetsPipeline>());
    // Preserve source mesh identity and authored data rather than using the editor's ambient import recipe.
    Pipeline->bUseSourceNameForAsset = false;
    Pipeline->CommonMeshesProperties->bBakeMeshes = true;
    Pipeline->CommonMeshesProperties->bRecomputeNormals = false;
    Pipeline->MeshPipeline->bBuildNanite = false;
    Pipeline->MeshPipeline->Collision = EInterchangeMeshCollision::None;
    FImportAssetParameters Parameters;
    Parameters.bIsAutomated = true;
    Parameters.bReplaceExisting = true;
    Parameters.OverridePipelines.Add(Pipeline.Get());
    TArray<UObject*> ImportedObjects;
    if (!Manager.ImportAsset(TEXT("/Game/B02"), SourceData, Parameters, ImportedObjects))
    { UE_LOG(LogTemp, Error, TEXT("B02 import FAIL: Interchange rejected GLB: %s"), *GLBPath); return 25; }
    UStaticMesh* Mesh = nullptr;
    for (UObject* Object : ImportedObjects)
    {
        if (UStaticMesh* Candidate = Cast<UStaticMesh>(Object))
        {
            if (Candidate->GetName().Equals(TEXT("Fiducial"), ESearchCase::CaseSensitive))
            {
                if (Mesh) { UE_LOG(LogTemp, Error, TEXT("B02 import FAIL: duplicate Fiducial static mesh")); return 26; }
                Mesh = Candidate;
            }
        }
    }
    if (!Mesh || !Mesh->GetBodySetup() || Mesh->GetBodySetup()->AggGeom.ConvexElems.IsEmpty())
    { UE_LOG(LogTemp, Error, TEXT("B02 import FAIL: Fiducial/UCX_Fiducial_00 imported mesh or convex collision absent")); return 27; }
    // Cooked runtime must inspect actual imported LOD vertices, not a hand-copied fixture.
    Mesh->bAllowCPUAccess = true;
    Mesh->MarkPackageDirty();
    UPackage* Package = Mesh->GetOutermost();
    const FString PackageFile = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    if (!UPackage::SavePackage(Package, Mesh, *PackageFile, SaveArgs))
    { UE_LOG(LogTemp, Error, TEXT("B02 import FAIL: could not persist Interchange mesh: %s"), *PackageFile); return 28; }
    if (!FEditorFileUtils::LoadMap(MapFile, false, false))
    { UE_LOG(LogTemp, Error, TEXT("B02 import FAIL: cannot reopen F03 qualification room")); return 29; }
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) return 29;
    AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(FVector::ZeroVector, FRotator::ZeroRotator);
    if (!Actor) return 30;
    Actor->SetActorLabel(TEXT("B02 imported GLB fiducial"));
    Actor->Tags.Add(TEXT("CookedFiducialMesh"));
    Actor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
    Actor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
    const TArray<TSharedPtr<FJsonValue>>* Ports = nullptr;
    if (!Basis->TryGetArrayField(TEXT("ports"), Ports) || !Ports || Ports->Num() != 2)
    { UE_LOG(LogTemp, Error, TEXT("B02 import FAIL: incomplete source port frames")); return 31; }
    for (int32 Index = 0; Index < 2; ++Index)
    {
        const TSharedPtr<FJsonObject> Port = (*Ports)[Index].IsValid() ? (*Ports)[Index]->AsObject() : nullptr;
        if (!Port.IsValid()) return 31;
        FString Name;
        FVector Point, Normal;
        const TArray<TSharedPtr<FJsonValue>>* PositionValues = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* NormalValues = nullptr;
        if (!Port->TryGetStringField(TEXT("name"), Name) ||
            Name != (Index == 0 ? TEXT("Supply") : TEXT("Return")) ||
            !Port->TryGetArrayField(TEXT("export_glb_m"), PositionValues) ||
            !Port->TryGetArrayField(TEXT("export_glb_normal"), NormalValues) ||
            !ReadTriple(PositionValues, Point) || !ReadTriple(NormalValues, Normal) ||
            !FMath::IsNearlyEqual(Normal.Size(), 1., .0001))
        { UE_LOG(LogTemp, Error, TEXT("B02 import FAIL: port frame missing/invalid")); return 31; }
        AActor* Marker = World->SpawnActor<AActor>();
        if (!Marker) return 32;
        USceneComponent* Frame = NewObject<USceneComponent>(Marker, TEXT("CookedPortFrame"));
        Marker->SetRootComponent(Frame);
        Frame->RegisterComponent();
        Marker->SetActorLocationAndRotation(Imported(Point), Imported(Normal).Rotation());
        Marker->SetActorLabel(TEXT("B02 GLB ") + Name + TEXT(" port"));
        Marker->Tags.Add(Index == 0 ? TEXT("CookedPortSupply") : TEXT("CookedPortReturn"));
    }
    if (!FEditorFileUtils::SaveLevel(World->PersistentLevel, MapFile))
    { UE_LOG(LogTemp, Error, TEXT("B02 import FAIL: cannot save qualification map")); return 33; }
    UE_LOG(LogTemp, Display, TEXT("B02 imported RH Y-up metre GLB via UE Interchange; saved imported render/convex collision and port frames in F03 room: %s"), *MapFile);
    return 0;
}
