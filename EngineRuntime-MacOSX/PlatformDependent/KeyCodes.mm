#include "../Interfaces/KeyCodes.h"

#include "../Miscellaneous/Volumes.h"

#import <AppKit/AppKit.h>

namespace Engine
{
	namespace Cocoa
	{
		extern Volumes::Dictionary<KeyCodes::Key, bool> KeyboardStatus;
	}
	namespace Keyboard
	{
        bool IsKeyPressed(uint key_code)
        {
            if (key_code == KeyCodes::LeftShift || key_code == KeyCodes::RightShift || key_code == KeyCodes::Shift) return [NSEvent modifierFlags] & NSEventModifierFlagShift;
            if (key_code == KeyCodes::LeftControl || key_code == KeyCodes::RightControl || key_code == KeyCodes::Control) return [NSEvent modifierFlags] & NSEventModifierFlagControl;
            if (key_code == KeyCodes::LeftAlternative || key_code == KeyCodes::RightAlternative || key_code == KeyCodes::Alternative) return [NSEvent modifierFlags] & NSEventModifierFlagOption;
            if (key_code == KeyCodes::LeftSystem || key_code == KeyCodes::RightSystem || key_code == KeyCodes::System) return [NSEvent modifierFlags] & NSEventModifierFlagCommand;
            Engine::KeyCodes::Key key = static_cast<Engine::KeyCodes::Key>(key_code);
            if (Cocoa::KeyboardStatus.GetElementByKey(key)) return true;
            return false;
        }
		bool IsKeyToggled(uint key_code)
        {
            if (key_code == KeyCodes::CapsLock) return [NSEvent modifierFlags] & NSEventModifierFlagCapsLock;
            return false;
        }
		int GetKeyboardDelay(void) { return int([NSEvent keyRepeatDelay] * 1000.0); }
		int GetKeyboardSpeed(void) { return int([NSEvent keyRepeatInterval] * 1000.0); }
    }
}