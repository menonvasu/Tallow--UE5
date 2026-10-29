#include "Input/TallowInputDeviceSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
	/** Watches every Slate input event ahead of the widgets and never consumes one. */
	class FTallowDeviceProcessor : public IInputProcessor
	{
	public:
		explicit FTallowDeviceProcessor(UTallowInputDeviceSubsystem* InOwner) : Owner(InOwner) {}
		virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}
		virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& E) override { if (Owner.IsValid()) Owner->NoteKey(E.GetKey()); return false; }
		virtual bool HandleAnalogInputEvent(FSlateApplication&, const FAnalogInputEvent& E) override { if (Owner.IsValid()) Owner->NoteAnalog(E.GetKey(), E.GetAnalogValue()); return false; }
		virtual bool HandleMouseMoveEvent(FSlateApplication&, const FPointerEvent& E) override { if (Owner.IsValid()) Owner->NoteMouseDelta(E.GetCursorDelta()); return false; }
		virtual bool HandleMouseButtonDownEvent(FSlateApplication&, const FPointerEvent& E) override { if (Owner.IsValid()) Owner->NoteKey(E.GetEffectingButton()); return false; }
		virtual const TCHAR* GetDebugName() const override { return TEXT("TallowDeviceProcessor"); }
	private:
		TWeakObjectPtr<UTallowInputDeviceSubsystem> Owner;
	};
}

namespace { TOptional<ETallowInputDevice> GPreviewDevice; }
void UTallowInputDeviceSubsystem::SetPreviewDevice(TOptional<ETallowInputDevice> InDevice) { GPreviewDevice = InDevice; }

ETallowInputDevice UTallowInputDeviceSubsystem::GetCurrent(const UObject* WorldContext)
{
	if (GPreviewDevice.IsSet()) return GPreviewDevice.GetValue();
	const UWorld* World = WorldContext ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
	if (const auto* Self = Instance ? Instance->GetSubsystem<UTallowInputDeviceSubsystem>() : nullptr) return Self->GetDevice();
	return ETallowInputDevice::KeyboardMouse;
}
void UTallowInputDeviceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (FSlateApplication::IsInitialized() && !IsRunningDedicatedServer())
	{
		Processor = MakeShared<FTallowDeviceProcessor>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(Processor);
	}
}
void UTallowInputDeviceSubsystem::Deinitialize()
{
	if (Processor.IsValid() && FSlateApplication::IsInitialized()) FSlateApplication::Get().UnregisterInputPreProcessor(Processor);
	Processor.Reset();
	Super::Deinitialize();
}
void UTallowInputDeviceSubsystem::ForceDeviceForTests(ETallowInputDevice InDevice) { SetDevice(InDevice); }
void UTallowInputDeviceSubsystem::SetDevice(ETallowInputDevice InDevice)
{
	if (InDevice == Device) return;
	Device = InDevice;
	UE_LOG(LogTemp, Display, TEXT("TALLOW_INPUT device=%s"), InDevice == ETallowInputDevice::Gamepad ? TEXT("Gamepad") : TEXT("KeyboardMouse"));
	OnDeviceChanged.Broadcast(Device);
}
void UTallowInputDeviceSubsystem::NoteKey(const FKey& Key)
{
	if (!Key.IsValid()) return;
	if (Key.IsGamepadKey()) SetDevice(ETallowInputDevice::Gamepad);
	else if (!Key.IsTouch() && !Key.IsGamepadKey()) SetDevice(ETallowInputDevice::KeyboardMouse);
}
void UTallowInputDeviceSubsystem::NoteAnalog(const FKey& Key, float Value)
{
	if (Key.IsGamepadKey() && FMath::Abs(Value) > .3f) SetDevice(ETallowInputDevice::Gamepad);
}
void UTallowInputDeviceSubsystem::NoteMouseDelta(const FVector2D& Delta)
{
	const double Now = FPlatformTime::Seconds();
	if (Now - LastMouseTime > .25) MouseTravel = 0.f;
	LastMouseTime = Now;
	MouseTravel += Delta.Size();
	if (MouseTravel > 4.f) { MouseTravel = 0.f; SetDevice(ETallowInputDevice::KeyboardMouse); }
}
