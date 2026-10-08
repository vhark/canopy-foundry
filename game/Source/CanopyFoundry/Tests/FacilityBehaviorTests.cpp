#include "Misc/AutomationTest.h"
#include "Player/FacilityCameraComponent.h"
#include "Player/FacilityCharacter.h"
#include "Player/FacilityMenu.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "SimulationSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFacilityViewOnlyTest, "Canopy.F03.ViewOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFacilityViewOnlyTest::RunTest(const FString& Parameters)
{
    const FFacilityViewState Start{EFacilityView::ThirdPerson, EFacilityView::ThirdPerson, 90.f, 350.f};
    const FFacilityViewState First = Start.TogglePerspective();
    TestEqual(TEXT("first person"), First.Current, EFacilityView::FirstPerson);
    TestEqual(TEXT("one actor and unchanged domain rate"), First.HandsOn, EFacilityView::FirstPerson);
    const FFacilityViewState Overhead = First.ToggleManagement();
    TestEqual(TEXT("management"), Overhead.Current, EFacilityView::Overhead);
    TestEqual(TEXT("restore hands-on"), Overhead.ToggleManagement().Current, EFacilityView::FirstPerson);
    TestEqual(TEXT("perspective switch ignored in management"), Overhead.TogglePerspective().Current, EFacilityView::Overhead);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFacilityBoundsTest, "Canopy.F03.BoundedHandoff", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFacilityBoundsTest::RunTest(const FString& Parameters)
{
    FFacilityIntentQueue Queue;
    FFacilityIntent Intent{};
    Intent.Kind = EFacilityIntentKind::Pause;
    for (int32 Index = 0; Index < FFacilityIntentQueue::Capacity - 1; ++Index)
    {
        TestTrue(TEXT("admit until full"), Queue.Push(Intent));
    }
    TestFalse(TEXT("full must reject"), Queue.Push(Intent));
    for (int32 Index = 0; Index < FFacilityIntentQueue::Capacity - 1; ++Index)
    {
        TestTrue(TEXT("every admitted intent delivered"), Queue.Pop(Intent));
    }
    TestFalse(TEXT("empty"), Queue.Pop(Intent));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFacilityViewBindingTest, "Canopy.F03.ViewBindings", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFacilityViewBindingTest::RunTest(const FString& Parameters)
{
    UInputMappingContext* Context = NewObject<UInputMappingContext>();
    UInputAction* View = NewObject<UInputAction>(Context);
    UInputAction* Control = NewObject<UInputAction>(Context);
    UInputAction* Pause = NewObject<UInputAction>(Context);
    UInputAction* Management = NewObject<UInputAction>(Context);
    Context->MapKey(Control, EKeys::F);
    Context->MapKey(Control, EKeys::Gamepad_FaceButton_Bottom);
    Context->MapKey(View, EKeys::V);
    Context->MapKey(Pause, EKeys::P);
    Context->MapKey(Management, EKeys::Tab);
    TestFalse(TEXT("physical control key cannot rebind a view"), AFacilityCharacter::IsViewKeyAvailable(*Context, *View, EKeys::F));
    TestFalse(TEXT("controller physical control cannot rebind a view"), AFacilityCharacter::IsViewKeyAvailable(*Context, *View, EKeys::Gamepad_FaceButton_Bottom));
    TestFalse(TEXT("pause key never becomes a view mutation"), AFacilityCharacter::IsViewKeyAvailable(*Context, *View, EKeys::P));
    TestFalse(TEXT("other camera action stays independent"), AFacilityCharacter::IsViewKeyAvailable(*Context, *View, EKeys::Tab));
    TestTrue(TEXT("existing view key remains valid"), AFacilityCharacter::IsViewKeyAvailable(*Context, *View, EKeys::V));
    TestTrue(TEXT("unused key is safe"), AFacilityCharacter::IsViewKeyAvailable(*Context, *View, EKeys::Gamepad_LeftShoulder));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFacilityMenuRootTest, "Canopy.F03.MenuFirstOpen", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFacilityMenuRootTest::RunTest(const FString& Parameters)
{
    UFacilityMenu* Menu = NewObject<UFacilityMenu>();
    Menu->Initialize();
    Menu->TakeWidget();
    TestNotNull(TEXT("first Slate build has a real focusable menu root"), Menu->WidgetTree->RootWidget.Get());
    const UBorder* Root = Cast<UBorder>(Menu->WidgetTree->RootWidget);
    const UVerticalBox* Controls = Root ? Cast<UVerticalBox>(Root->GetContent()) : nullptr;
    TestTrue(TEXT("first display includes operational controls"), Controls && Controls->GetChildrenCount() >= 12);
    return true;
}

#endif
