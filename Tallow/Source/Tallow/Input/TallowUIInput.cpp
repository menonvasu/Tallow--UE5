#include "Input/TallowUIInput.h"
bool TallowUIInput::IsGamepadKey(const FKey& Key) { return Key.IsGamepadKey(); }
ETallowPadIntent TallowUIInput::ClassifyGamepad(const FKey& Key)
{
    if (Key == EKeys::Gamepad_FaceButton_Bottom) return ETallowPadIntent::Confirm;
    if (Key == EKeys::Gamepad_FaceButton_Right) return ETallowPadIntent::Back;
    if (Key == EKeys::Gamepad_FaceButton_Left) return ETallowPadIntent::Secondary;
    if (Key == EKeys::Gamepad_FaceButton_Top) return ETallowPadIntent::Tertiary;
    if (Key == EKeys::Gamepad_LeftShoulder) return ETallowPadIntent::PrevTab;
    if (Key == EKeys::Gamepad_RightShoulder) return ETallowPadIntent::NextTab;
    if (Key == EKeys::Gamepad_Special_Right) return ETallowPadIntent::Menu;
    if (Key == EKeys::Gamepad_Special_Left) return ETallowPadIntent::View;
    if (Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up) return ETallowPadIntent::Up;
    if (Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down) return ETallowPadIntent::Down;
    if (Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_LeftStick_Left) return ETallowPadIntent::Left;
    if (Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_LeftStick_Right) return ETallowPadIntent::Right;
    return ETallowPadIntent::None;
}
