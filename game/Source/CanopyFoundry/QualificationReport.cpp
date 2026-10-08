#include "QualificationReport.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool WriteQualificationReport(const TCHAR* Case, const TSharedRef<FJsonObject>& Measurements)
{
    FString Filename;
    if (!FParse::Value(FCommandLine::Get(), TEXT("QualificationReport="), Filename) || Filename.IsEmpty()) return false;
    Filename = FPaths::ConvertRelativePathToFull(Filename);
    if (IFileManager::Get().FileExists(*Filename) ||
        !IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true)) return false;
    const TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetNumberField(TEXT("schema"), 1);
    Report->SetStringField(TEXT("case"), Case);
    Report->SetStringField(TEXT("status"), TEXT("pass"));
    Report->SetStringField(TEXT("engine"), FEngineVersion::Current().ToString());
    Report->SetObjectField(TEXT("measurements"), Measurements);
    FString Json;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
    return FJsonSerializer::Serialize(Report, Writer) &&
        FFileHelper::SaveStringToFile(Json, *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
