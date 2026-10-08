#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include <canopy/world.hpp>
#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>
#include "SimulationSubsystem.generated.h"

struct FFacilityTimeDebt
{
    void SetRate(int32 NewRate) { if (Rate != NewRate) { Rate = NewRate; Pending = 0.; } }
    void Reset() { Pending = 0.; }
    void Accrue(double RealSeconds, int32 NewRate)
    {
        SetRate(NewRate);
        Pending = std::min(7200., Pending + std::max(0., RealSeconds) * Rate);
    }
    int64 RequestedSteps() const { return static_cast<int64>(std::min(120., Pending)); }
    void Reconcile(canopy::SimSecond Advanced, canopy::AdvanceStop Stop)
    {
        Pending = std::max(0., Pending - static_cast<double>(std::max<canopy::SimSecond>(0, Advanced)));
        if (Stop == canopy::AdvanceStop::DecisionRequired ||
            Stop == canopy::AdvanceStop::InvalidModel ||
            Stop == canopy::AdvanceStop::CapacityExhausted) Reset();
    }
private:
    int32 Rate{1};
    double Pending{};
};

enum class EFacilityIntentKind : uint8 { Initialized, SetControl, ResolveDecision, Target, Pause };
struct FFacilityIntent
{
    EFacilityIntentKind Kind{};
    canopy::Command Command{};
    canopy::SimSecond Target{};
};

template <typename T, uint32 N>
class TFacilitySpscQueue
{
public:
    static constexpr int32 Capacity = N;
    bool Push(const T& Value)
    {
        const uint32 Write = WriteIndex.load(std::memory_order_relaxed);
        const uint32 Next = (Write + 1) % N;
        if (Next == ReadIndex.load(std::memory_order_acquire)) return false;
        Items[Write] = Value;
        WriteIndex.store(Next, std::memory_order_release);
        return true;
    }
    bool Pop(T& Value)
    {
        const uint32 Read = ReadIndex.load(std::memory_order_relaxed);
        if (Read == WriteIndex.load(std::memory_order_acquire)) return false;
        Value = Items[Read];
        ReadIndex.store((Read + 1) % N, std::memory_order_release);
        return true;
    }
private:
    T Items[N]{};
    std::atomic<uint32> ReadIndex{0};
    std::atomic<uint32> WriteIndex{0};
};
using FFacilityIntentQueue = TFacilitySpscQueue<FFacilityIntent, 64>;

struct FFacilityUpdate
{
    canopy::SimSecond Second{};
    canopy::SimSecond NextDecision{-1};
    uint64 Revision{};
    bool bDecisionPending{};
    canopy::ControlView Controls[8]{};
    uint8 ControlCount{};
    canopy::Event Events[16]{};
    uint8 EventCount{};
    canopy::SubmitResult Result{};
    canopy::AdvanceStop Stop{canopy::AdvanceStop::ReachedTarget};
    EFacilityIntentKind Kind{EFacilityIntentKind::Initialized};
    bool bHasResult{};
    bool bCompletesIntent{};
    bool bLoadFailed{};
};

UCLASS()
class CANOPYRUNTIME_API USimulationSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(USimulationSubsystem, STATGROUP_Tickables); }
    virtual bool IsTickable() const override { return !IsTemplate() && bReady; }
    bool Enqueue(const FFacilityIntent& Intent);
    bool SetSpeed(int32 NewSpeed);
    void TogglePause();
    bool SkipToDecision();
    bool SubmitControl(double Fraction);
    bool ResolvePendingDecision();
    const FString& Status() const { return Feedback; }
    canopy::SimSecond Second() const { return CurrentSecond; }
    uint64 Revision() const { return CurrentRevision; }
    double AppliedControl() const { return ControlFraction; }
    int32 Speed() const { return bPaused ? 0 : RequestedSpeed; }
    double AchievedSpeed() const { return AchievedRate; }
    bool IsManagementTimeLapse() const { return RequestedSpeed > 5 || (bTargetOutstanding && OutstandingSpeed > 5); }
    static constexpr canopy::Id TargetId{0x43414e4f50590001ULL, 1};
private:
    friend class FFacilityTargetBatchTest;
    void WorkerMain();
    void Publish(canopy::World& World, FFacilityUpdate& Update, std::vector<canopy::Event>& Published);
    void Consume(const FFacilityUpdate& Update);
    FFacilityIntentQueue Intents;
    TFacilitySpscQueue<FFacilityUpdate, 64> Updates;
    std::thread Worker;
    std::atomic<bool> bStopping{false};
    std::atomic<bool> bWorkerExited{false};
    bool bReady{};
    bool bPaused{true};
    bool bTargetOutstanding{};
    int32 RequestedSpeed{1};
    int32 OutstandingSpeed{1};
    FFacilityTimeDebt TimeDebt;
    double RealWindow{};
    double DomainWindow{};
    double AchievedRate{};
    canopy::SimSecond LastObservedSecond{};
    canopy::SimSecond CurrentSecond{};
    canopy::SimSecond NextDecision{-1};
    uint64 CurrentRevision{};
    uint64 CommandSequence{};
    double ControlFraction{0.25};
    bool bDecisionPending{};
    FString Feedback{TEXT("Loading simulation")};
};
