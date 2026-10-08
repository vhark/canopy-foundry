#include "SimulationSubsystem.h"
#include "Modules/ModuleManager.h"
#include <algorithm>
#include <chrono>
#include <tuple>

IMPLEMENT_MODULE(FDefaultModuleImpl, CanopyRuntime)

void USimulationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bReady = true;
    Worker = std::thread([this] { WorkerMain(); });
}

void USimulationSubsystem::Deinitialize()
{
    bReady = false;
    bStopping.store(true, std::memory_order_release);
    // The worker finishes all admitted intents; drain the bounded output even if gameplay has stopped ticking.
    while (Worker.joinable())
    {
        FFacilityUpdate Update;
        while (Updates.Pop(Update)) Consume(Update);
        if (bWorkerExited.load(std::memory_order_acquire)) { Worker.join(); break; }
        std::this_thread::yield();
    }
    FFacilityUpdate Update;
    while (Updates.Pop(Update)) Consume(Update);
    Super::Deinitialize();
}

bool USimulationSubsystem::Enqueue(const FFacilityIntent& Intent)
{
    if (bStopping.load(std::memory_order_acquire) || !Intents.Push(Intent))
    {
        Feedback = TEXT("Simulation busy: action not queued");
        return false;
    }
    return true;
}

bool USimulationSubsystem::SetSpeed(int32 NewSpeed)
{
    if (NewSpeed != 1 && NewSpeed != 5 && NewSpeed != 60 && NewSpeed != 360 && NewSpeed != 1440) return false;
    TimeDebt.SetRate(NewSpeed);
    RequestedSpeed = NewSpeed;
    Feedback = FString::Printf(TEXT("Requested %dx; time reached %lld"), NewSpeed, static_cast<long long>(CurrentSecond));
    return true;
}

void USimulationSubsystem::TogglePause()
{
    FFacilityIntent Intent{};
    Intent.Kind = EFacilityIntentKind::Pause;
    if (Enqueue(Intent))
    {
        bPaused = !bPaused;
        if (bPaused) TimeDebt.Reset();
        Feedback = bPaused ? TEXT("Pause queued") : TEXT("Resume queued");
    }
}

bool USimulationSubsystem::SkipToDecision()
{
    if (NextDecision <= CurrentSecond || bTargetOutstanding)
    {
        Feedback = TEXT("No available decision to skip to, or advance still pending");
        return false;
    }
    FFacilityIntent Intent{};
    Intent.Kind = EFacilityIntentKind::Target;
    Intent.Target = NextDecision;
    if (!Enqueue(Intent)) return false;
    bTargetOutstanding = true;
    OutstandingSpeed = 60;
    TimeDebt.SetRate(60);
    TimeDebt.Reset();
    RequestedSpeed = 60;
    bPaused = false;
    Feedback = TEXT("Advancing toward decision (bounded steps)");
    return true;
}

bool USimulationSubsystem::SubmitControl(double Fraction)
{
    if (!FMath::IsFinite(Fraction) || Fraction < 0.0 || Fraction > 1.0) return false;
    FFacilityIntent Intent{};
    Intent.Kind = EFacilityIntentKind::SetControl;
    Intent.Command.header = {{0x43414e4f50590002ULL, ++CommandSequence}, {0x43414e4f50590003ULL, 1}, 0, 0};
    Intent.Command.action = canopy::SetControl{TargetId, canopy::Fraction{Fraction}};
    return Enqueue(Intent);
}

bool USimulationSubsystem::ResolvePendingDecision()
{
    if (!bDecisionPending) { Feedback = TEXT("No decision pending"); return false; }
    FFacilityIntent Intent{};
    Intent.Kind = EFacilityIntentKind::ResolveDecision;
    Intent.Command.header = {{0x43414e4f50590002ULL, ++CommandSequence}, {0x43414e4f50590003ULL, 1}, 0, 0};
    Intent.Command.action = canopy::ResolveDecision{{0x43414e4f50590004ULL, 1}, CurrentSecond};
    return Enqueue(Intent);
}

void USimulationSubsystem::Publish(canopy::World& World, FFacilityUpdate& Update, std::vector<canopy::Event>& Published)
{
    Update.bCompletesIntent = false;
    const canopy::WorldView View = World.view();
    Update.Second = View.second;
    Update.NextDecision = View.next_decision_second;
    Update.bDecisionPending = View.decision_pending;
    Update.Revision = View.revision;
    Update.ControlCount = static_cast<uint8>(std::min<size_t>(View.controls.size(), 8));
    std::copy_n(View.controls.begin(), Update.ControlCount, Update.Controls);
    const auto Events = World.events();
    if (Events.size() == Published.size())
    {
        Update.bCompletesIntent = true;
        while (!Updates.Push(Update)) std::this_thread::yield();
        return;
    }
    // Domain events are sorted, not append-only. A command at an existing second
    // may insert before previously published events; merge against the prior
    // immutable sequence rather than indexing by cumulative event count.
    size_t PriorIndex = 0;
    for (const canopy::Event& Event : Events)
    {
        while (PriorIndex < Published.size() &&
            std::tie(Published[PriorIndex].second, Published[PriorIndex].type, Published[PriorIndex].entity, Published[PriorIndex].command_id) <
            std::tie(Event.second, Event.type, Event.entity, Event.command_id)) ++PriorIndex;
        if (PriorIndex < Published.size() && Published[PriorIndex] == Event) { ++PriorIndex; continue; }
        if (Update.EventCount == 16)
        {
            while (!Updates.Push(Update)) std::this_thread::yield();
            Update.EventCount = 0;
            Update.bHasResult = false;
        }
        Update.Events[Update.EventCount++] = Event;
    }
    Update.bCompletesIntent = true;
    while (!Updates.Push(Update)) std::this_thread::yield();
    Published.assign(Events.begin(), Events.end());
}

void USimulationSubsystem::WorkerMain()
{
    canopy::World World({8, 256, 65536, 8});
    const canopy::InitialControl Control{TargetId, canopy::Fraction{0.25}};
    const canopy::DecisionDeadline Deadline{{0x43414e4f50590004ULL, 1}, TargetId, 60};
    const canopy::LoadError Load = World.load(std::span(&Control, 1), std::span(&Deadline, 1), 0x43414e4f5059ULL);
    std::vector<canopy::Event> Published;
    Published.reserve(65536);
    FFacilityUpdate Initial;
    Initial.bLoadFailed = Load != canopy::LoadError::None;
    if (Initial.bLoadFailed)
    {
        while (!Updates.Push(Initial)) std::this_thread::yield();
        bWorkerExited.store(true, std::memory_order_release);
        return;
    }
    Publish(World, Initial, Published);
    for (;;)
    {
        FFacilityIntent Intent;
        if (!Intents.Pop(Intent))
        {
            if (bStopping.load(std::memory_order_acquire)) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        FFacilityUpdate Update;
        Update.Kind = Intent.Kind;
        if (Intent.Kind == EFacilityIntentKind::SetControl || Intent.Kind == EFacilityIntentKind::ResolveDecision)
        {
            Intent.Command.header.expected_revision = World.view().revision;
            Intent.Command.header.issued_at = World.view().second;
            Update.Result = World.submit(Intent.Command);
            Update.bHasResult = true;
        }
        else if (Intent.Kind == EFacilityIntentKind::Target)
        {
            const canopy::SimSecond BoundedTarget = std::min(Intent.Target, World.view().second + 120);
            Update.Stop = World.advance({BoundedTarget, 120}).stop;
        }
        Publish(World, Update, Published);
    }
    bWorkerExited.store(true, std::memory_order_release);
}

void USimulationSubsystem::Consume(const FFacilityUpdate& Update)
{
    if (Update.bLoadFailed) { Feedback = TEXT("Simulation model failed to load"); bPaused = true; return; }
    if (Update.Kind == EFacilityIntentKind::Initialized) Feedback = TEXT("Simulation ready (paused)");
    const canopy::SimSecond Previous = CurrentSecond;
    CurrentSecond = Update.Second;
    CurrentRevision = Update.Revision;
    NextDecision = Update.NextDecision;
    bDecisionPending = Update.bDecisionPending;
    for (uint8 Index = 0; Index < Update.ControlCount; ++Index)
        if (Update.Controls[Index].id == TargetId) ControlFraction = Update.Controls[Index].applied.value;
    if (Update.Kind == EFacilityIntentKind::Target)
    {
        TimeDebt.Reconcile(CurrentSecond - Previous, Update.Stop);
        if (Update.bCompletesIntent) bTargetOutstanding = false;
        if (Update.Stop == canopy::AdvanceStop::DecisionRequired) { bPaused = true; Feedback = TEXT("Decision required: press Resolve"); }
        else if (Update.Stop == canopy::AdvanceStop::CapacityExhausted || Update.Stop == canopy::AdvanceStop::InvalidModel) { bPaused = true; Feedback = TEXT("Simulation halted: event capacity/model error"); }
    }
    if (Update.bHasResult)
        Feedback = Update.Result.error == canopy::SubmitError::None
            ? FString::Printf(TEXT("Accepted control/decision, revision %llu"), static_cast<unsigned long long>(Update.Result.receipt.revision))
            : FString::Printf(TEXT("Action rejected (reason %d)"), static_cast<int32>(Update.Result.error));
    for (uint8 Index = 0; Index < Update.EventCount; ++Index)
        if (Update.Events[Index].type == canopy::EventType::DecisionRequired) Feedback = TEXT("Decision required: press Resolve");
}

void USimulationSubsystem::Tick(float DeltaTime)
{
    FFacilityUpdate Update;
    while (Updates.Pop(Update)) Consume(Update);
    DomainWindow += static_cast<double>(CurrentSecond - LastObservedSecond);
    LastObservedSecond = CurrentSecond;
    RealWindow += FMath::Max(0.f, DeltaTime);
    if (RealWindow >= 1.0)
    {
        AchievedRate = DomainWindow / RealWindow;
        DomainWindow = 0.0;
        RealWindow = 0.0;
    }
    if (bPaused || bTargetOutstanding || bDecisionPending) return;
    TimeDebt.Accrue(FMath::Max(0.f, DeltaTime), RequestedSpeed);
    const int64 Steps = TimeDebt.RequestedSteps();
    if (Steps == 0) return;
    FFacilityIntent Intent{};
    Intent.Kind = EFacilityIntentKind::Target;
    Intent.Target = CurrentSecond + Steps;
    if (Enqueue(Intent))
    {
        bTargetOutstanding = true;
        OutstandingSpeed = RequestedSpeed;
    }
}
