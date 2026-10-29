#include "Input/TallowInputPrompts.h"
#include "Input/TallowInputDeviceSubsystem.h"
#include "UObject/Class.h"

namespace
{
	struct FPromptRow { const TCHAR* KeyLabel; const TCHAR* KeyGlyph; const TCHAR* PadLabel; const TCHAR* PadGlyph; };
	FPromptRow RowFor(ETallowPromptAction Action)
	{
		switch (Action)
		{
		case ETallowPromptAction::Interact: return {TEXT("E"), TEXT("Key.E"), TEXT("square"), TEXT("PS.Square")};
		case ETallowPromptAction::RockChair: return {TEXT("space"), TEXT("Key.Space"), TEXT("cross"), TEXT("PS.Cross")};
		case ETallowPromptAction::Skip: return {TEXT("space"), TEXT("Key.Space"), TEXT("cross"), TEXT("PS.Cross")};
		case ETallowPromptAction::Cup: return {TEXT("right mouse"), TEXT("Mouse.Right"), TEXT("L1"), TEXT("PS.L1")};
		case ETallowPromptAction::Relight: return {TEXT("R"), TEXT("Key.R"), TEXT("d-pad up"), TEXT("PS.DPadUp")};
		case ETallowPromptAction::HoldBreath: return {TEXT("F"), TEXT("Key.F"), TEXT("R1"), TEXT("PS.R1")};
		case ETallowPromptAction::Tension: return {TEXT("left mouse"), TEXT("Mouse.Left"), TEXT("R2"), TEXT("PS.R2")};
		case ETallowPromptAction::PickMove: return {TEXT("mouse"), TEXT("Mouse.Move"), TEXT("right stick"), TEXT("PS.RightStick")};
		case ETallowPromptAction::Run: return {TEXT("shift"), TEXT("Key.Shift"), TEXT("L3"), TEXT("PS.L3")};
		case ETallowPromptAction::Crouch: return {TEXT("ctrl"), TEXT("Key.Ctrl"), TEXT("circle"), TEXT("PS.Circle")};
		case ETallowPromptAction::Drop: return {TEXT("Q"), TEXT("Key.Q"), TEXT("d-pad down"), TEXT("PS.DPadDown")};
		case ETallowPromptAction::Inventory: return {TEXT("I"), TEXT("Key.I"), TEXT("triangle"), TEXT("PS.Triangle")};
		case ETallowPromptAction::Map: return {TEXT("M"), TEXT("Key.M"), TEXT("create"), TEXT("PS.Create")};
		case ETallowPromptAction::Pause: return {TEXT("esc"), TEXT("Key.Escape"), TEXT("options"), TEXT("PS.Options")};
		case ETallowPromptAction::Back: return {TEXT("esc"), TEXT("Key.Escape"), TEXT("circle"), TEXT("PS.Circle")};
		case ETallowPromptAction::Confirm: return {TEXT("enter"), TEXT("Key.Enter"), TEXT("cross"), TEXT("PS.Cross")};
		case ETallowPromptAction::ReadToggle: return {TEXT("V"), TEXT("Key.V"), TEXT("d-pad left"), TEXT("PS.DPadLeft")};
		case ETallowPromptAction::ReadSkip: return {TEXT("backspace"), TEXT("Key.Backspace"), TEXT("d-pad right"), TEXT("PS.DPadRight")};
		case ETallowPromptAction::PagePrev: return {TEXT("left"), TEXT("Key.Left"), TEXT("L1"), TEXT("PS.L1")};
		case ETallowPromptAction::PageNext: return {TEXT("right"), TEXT("Key.Right"), TEXT("R1"), TEXT("PS.R1")};
		case ETallowPromptAction::FloorPrev: return {TEXT("left"), TEXT("Key.Left"), TEXT("L1"), TEXT("PS.L1")};
		case ETallowPromptAction::FloorNext: return {TEXT("right"), TEXT("Key.Right"), TEXT("R1"), TEXT("PS.R1")};
		case ETallowPromptAction::StopLockpick: return {TEXT("E"), TEXT("Key.E"), TEXT("circle"), TEXT("PS.Circle")};
		case ETallowPromptAction::KeypadErase: return {TEXT("backspace"), TEXT("Key.Backspace"), TEXT("square"), TEXT("PS.Square")};
		case ETallowPromptAction::KeypadClear: return {TEXT("delete"), TEXT("Key.Delete"), TEXT("triangle"), TEXT("PS.Triangle")};
		case ETallowPromptAction::KeypadSubmit: return {TEXT("enter"), TEXT("Key.Enter"), TEXT("R1"), TEXT("PS.R1")};
		}
		return {TEXT(""), TEXT(""), TEXT(""), TEXT("")};
	}
}

FTallowPrompt TallowInputPrompts::Get(ETallowPromptAction Action, ETallowInputDevice Device)
{
	const FPromptRow Row = RowFor(Action);
	const bool bPad = Device == ETallowInputDevice::Gamepad;
	FTallowPrompt Prompt;
	Prompt.Label = FText::FromString(bPad ? Row.PadLabel : Row.KeyLabel);
	Prompt.Glyph = FName(bPad ? Row.PadGlyph : Row.KeyGlyph);
	return Prompt;
}
FTallowPrompt TallowInputPrompts::GetCurrent(const UObject* Context, ETallowPromptAction Action) { return Get(Action, UTallowInputDeviceSubsystem::GetCurrent(Context)); }
FString TallowInputPrompts::Expand(const UObject* Context, const FString& TextWithTokens)
{
	FString Result = TextWithTokens;
	const UEnum* Enum = StaticEnum<ETallowPromptAction>();
	for (int32 I = 0; I < Enum->NumEnums() - 1; ++I)
	{
		const auto Action = static_cast<ETallowPromptAction>(Enum->GetValueByIndex(I));
		Result.ReplaceInline(*(TEXT("{") + Enum->GetNameStringByIndex(I) + TEXT("}")), *GetCurrent(Context, Action).Label.ToString());
	}
	return Result;
}
FText TallowInputPrompts::ControlsSheet(ETallowInputDevice Device)
{
	const FString Tail = TEXT("\n\ndraughts can snuff the candle. cup it to keep it.\nstriking a match is loud. running costs you your light.");
	if (Device == ETallowInputDevice::Gamepad)
		return FText::FromString(TEXT("left stick   move\nright stick   look\nsquare   use, pick up, read\ncross   jump, rock the chair\nhold L1   cup the flame\nd-pad up   strike a match, or put the candle out\nhold R1   hold your breath\nhold R2   tension, when picking a lock\nL3   run\ncircle   crouch\nd-pad down   put down what you carry\ntriangle   look through your things\ncreate   unfold your map, once found\noptions   pause") + Tail);
	return FText::FromString(TEXT("WASD   move\nmouse   look\nE   use, pick up, read\nspace   jump, rock the chair\nhold right mouse   cup the flame\nR   strike a match, or put the candle out\nhold F   hold your breath\nhold left mouse   tension, when picking a lock\nshift   run\nctrl   crouch\nQ   put down what you carry\nI / Tab   look through your things\nM   unfold your map, once found\nesc   pause") + Tail);
}
