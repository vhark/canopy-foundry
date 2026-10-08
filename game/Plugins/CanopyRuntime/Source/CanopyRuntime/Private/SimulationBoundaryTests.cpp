#include "Misc/AutomationTest.h"
#include "SimulationSubsystem.h"
#include "Engine/GameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFacilityDebtTransitionTest, "Canopy.F03.ManagementDebt", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFacilityDebtTransitionTest::RunTest(const FString& Parameters)
{
    FFacilityTimeDebt Clock;
    Clock.Accrue(.1, 1440);
    TestEqual(TEXT("144 requested"), Clock.RequestedSteps(), int64(120));
    Clock.Reconcile(60, canopy::AdvanceStop::DecisionRequired);
    TestEqual(TEXT("authored stop cancels all unachieved acceleration"), Clock.RequestedSteps(), int64(0));
    Clock.Accrue(0.1, 1440);
    Clock.SetRate(1);
    TestEqual(TEXT("rate change cancels previous management debt"), Clock.RequestedSteps(), int64(0));
    Clock.Accrue(.1, 1);
    TestEqual(TEXT("hands-on time must be earned anew"), Clock.RequestedSteps(), int64(0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFacilityTargetBatchTest, "Canopy.F03.TargetBatchCompletion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFacilityTargetBatchTest::RunTest(const FString& Parameters)
{
    UGameInstance* Game = NewObject<UGameInstance>();
    USimulationSubsystem* Sim = NewObject<USimulationSubsystem>(Game);
    canopy::World World({8, 256, 65536, 8});
    const canopy::InitialControl Control{USimulationSubsystem::TargetId, canopy::Fraction{.25}};
    if (World.load(std::span(&Control, 1), {}, 1) != canopy::LoadError::None) return false;
    std::vector<canopy::Event> Published;
    Published.reserve(65536);
    FFacilityUpdate Initial;
    Sim->Publish(World, Initial, Published);
    FFacilityUpdate Batch;
    while (Sim->Updates.Pop(Batch)) Sim->Consume(Batch);
    // 24 climate and two crop events require two packets.
    TestTrue(TEXT("core reaches target"), World.advance({120, 120}).stop == canopy::AdvanceStop::ReachedTarget);
    FFacilityUpdate Advance;
    Advance.Kind = EFacilityIntentKind::Target;
    Sim->Publish(World, Advance, Published);
    Sim->bTargetOutstanding = true;
    Sim->OutstandingSpeed = 1440;
    Sim->SetSpeed(1); // Lowering requested speed must not unlock pending management work.
    if (!Sim->Updates.Pop(Batch)) return false;
    Sim->Consume(Batch);
    TestTrue(TEXT("first event batch cannot acknowledge the target"), Sim->bTargetOutstanding);
    TestTrue(TEXT("hands-on interaction remains blocked"), Sim->IsManagementTimeLapse());
    if (!Sim->Updates.Pop(Batch)) return false;
    Sim->Consume(Batch);
    TestFalse(TEXT("final event batch acknowledges exactly once"), Sim->bTargetOutstanding);
    TestFalse(TEXT("completed management work releases hands-on mode"), Sim->IsManagementTimeLapse());
    TestEqual(TEXT("time advanced once across both packets"), Sim->Second(), canopy::SimSecond(120));
    TestFalse(TEXT("no empty trailing acknowledgement"), Sim->Updates.Pop(Batch));
    return true;
}
#endif
