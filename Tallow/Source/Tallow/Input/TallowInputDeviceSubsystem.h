#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Framework/Application/IInputProcessor.h"
#include "Input/TallowInputPrompts.h"
#include "TallowInputDeviceSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FTallowInputDeviceChanged, ETallowInputDevice);
UCLASS()
class TALLOW_API UTallowInputDeviceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	static ETallowInputDevice GetCurrent(const UObject* WorldContext);
	ETallowInputDevice GetDevice() const { return Device; }
	/** Previews and captures with no game instance: every GetCurrent answers this until it is cleared. */
	static void SetPreviewDevice(TOptional<ETallowInputDevice> InDevice);
	void ForceDeviceForTests(ETallowInputDevice InDevice);
	/** What the Slate pre-processor reports; also callable directly. A gamepad key flips to Gamepad, any other key to KeyboardMouse. */
	void NoteKey(const FKey& Key);
	/** Analog values above .3 on a gamepad axis flip to Gamepad; other axes are ignored. */
	void NoteAnalog(const FKey& Key, float Value);
	/** Cursor travel in pixels; more than 4 within a moment flips to KeyboardMouse. */
	void NoteMouseDelta(const FVector2D& Delta);
	FTallowInputDeviceChanged OnDeviceChanged;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
protected:
	void SetDevice(ETallowInputDevice InDevice);
	ETallowInputDevice Device = ETallowInputDevice::KeyboardMouse;
	TSharedPtr<IInputProcessor> Processor;
	float MouseTravel = 0.f;
	double LastMouseTime = 0.0;
};
