#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// An explicit external result survives Shipping's disabled UE_LOG and rejects stale output.
bool WriteQualificationReport(const TCHAR* Case, const TSharedRef<FJsonObject>& Measurements);
