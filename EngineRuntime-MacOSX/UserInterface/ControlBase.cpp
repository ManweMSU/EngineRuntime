#include "ControlBase.h"

#include "../Interfaces/KeyCodes.h"
#include "../Interfaces/SystemGraphics.h"
#include "../Media/Audio.h"
#include "../PlatformSpecific/MacTouchBar.h"

#include "ButtonControls.h"
#include "CombinedControls.h"
#include "EditControls.h"
#include "GroupControls.h"
#include "ListControls.h"
#include "MenuBarControl.h"
#include "RichEditControl.h"
#include "ScrollableControls.h"
#include "StaticControls.h"
#include "ControlServiceEx.h"

namespace Engine
{
	namespace UI
	{
		class ControlRectangleAnimationController : public Animation::AnimationController<Rectangle>
		{
		public:
			SafePointer<Control> Target;
			Animation::AnimationAction Action;
			ControlRectangleAnimationController(void) : Action(Animation::AnimationAction::None) {}
			virtual void OnAnimationFrame(const Rectangle & state) override { Target->SetRectangle(state); }
			virtual void OnAnimationStart(void) override
			{
				Target->SetRectangle(InitialState);
				if (Action == Animation::AnimationAction::ShowWindow) Target->Show(true);
			}
			virtual void OnAnimationEnd(void) override
			{
				if (Action == Animation::AnimationAction::HideWindow) Target->Show(false);
				else if (Action == Animation::AnimationAction::HideWindowKeepPosition) { Target->Show(false); Target->SetRectangle(InitialState); }
			}
		};
		class ControlPositionAnimationController : public Animation::AnimationController<Math::Vector4>
		{
		public:
			SafePointer<Control> Target;
			Animation::AnimationAction Action;
			ControlPositionAnimationController(void) : Action(Animation::AnimationAction::None) {}
			static Math::Vector4 GetVector(const Box & box) { return Math::Vector4(box.Left, box.Top, box.Right, box.Bottom); }
			static Box GetBox(const Math::Vector4 & state) { return Box(int(state.x), int(state.y), int(state.z), int(state.w)); }
			virtual void OnAnimationFrame(const Math::Vector4 & state) override { Target->SetPosition(GetBox(state)); }
			virtual void OnAnimationStart(void) override
			{
				Target->SetPosition(GetBox(InitialState));
				if (Action == Animation::AnimationAction::ShowWindow) Target->Show(true);
			}
			virtual void OnAnimationEnd(void) override
			{
				if (Action == Animation::AnimationAction::HideWindow) Target->Show(false);
				else if (Action == Animation::AnimationAction::HideWindowKeepPosition) { Target->Show(false); Target->SetPosition(GetBox(InitialState)); }
			}
		};
		class WindowPositionAnimationController : public Animation::AnimationController<Math::Vector4>
		{
		public:
			Windows::IWindow * Window;
			WindowPositionAnimationController(void) : Window(0) {}
			virtual void OnAnimationFrame(const Math::Vector4 & state) override { Window->SetPosition(ControlPositionAnimationController::GetBox(state)); }
			virtual void OnAnimationStart(void) override { Window->SetPosition(ControlPositionAnimationController::GetBox(InitialState)); }
			virtual void OnAnimationEnd(void) override {}
		};
		class WindowShowAnimationController : public Animation::AnimationController<double>
		{
		public:
			Windows::IWindow * Window;
			WindowShowAnimationController(void) : Window(0) {}
			virtual void OnAnimationFrame(const double & state) override { Window->SetOpacity(state); }
			virtual void OnAnimationStart(void) override
			{
				Window->SetOpacity(InitialState);
				if (FinalState) Window->Show(true);
			}
			virtual void OnAnimationEnd(void) override
			{
				if (InitialState) {
					Window->Show(false);
					Window->SetOpacity(1.0);
				}
			}
		};

		void Control::__set_system(ControlSystem * _system)
		{
			auto sys = __system;
			__system = _system;
			for (auto & _child : __children) _child.__set_system(_system);
			if (!__system && sys) {
				sys->SetTimer(this, 0);
				if (sys->_captured.Inner() == this) sys->_captured.SetReference(0);
				if (sys->_focused.Inner() == this) sys->_focused.SetReference(0);
				if (sys->_exclusive.Inner() == this) sys->_exclusive.SetReference(0);
			}
		}
		Control::Control(void) : __parent(0), __system(0), __children(0x10), ControlBoundaries(Box(0, 0, 0, 0)) {}
		Control::~Control(void) { __set_system(0); for (auto & _child : __children) _child.__parent = 0; }
		void Control::Render(Graphics::I2DDeviceContext * device, const Box & at) {}
		void Control::ResetCache(void) { for (auto & child : __children) child.ResetCache(); }
		void Control::ArrangeChildren(void) {}
		void Control::Enable(bool enable) {}
		bool Control::IsEnabled(void) { return true; }
		void Control::Show(bool visible) {}
		bool Control::IsVisible(void) { return true; }
		bool Control::IsTabStop(void) { return false; }
		void Control::SetID(int ID) {}
		int Control::GetID(void) { return 0; }
		Control * Control::FindChild(int ID) { return 0; }
		void Control::SetRectangle(const Rectangle & rect) {}
		Rectangle Control::GetRectangle(void) { return Rectangle::Invalid(); }
		void Control::SetPosition(const Box & box) { ControlBoundaries = box; }
		Box Control::GetPosition(void) { return ControlBoundaries; }
		void Control::SetText(const string & text) {}
		string Control::GetText(void) { return L""; }
		void Control::RaiseEvent(int ID, ControlEvent event, Control * sender) { if (__parent) __parent->RaiseEvent(ID, event, sender); }
		void Control::FocusChanged(bool got_focus) {}
		void Control::CaptureChanged(bool got_capture) {}
		void Control::LostExclusiveMode(void) {}
		void Control::LeftButtonDown(Point at) {}
		void Control::LeftButtonUp(Point at) {}
		void Control::LeftButtonDoubleClick(Point at) {}
		void Control::RightButtonDown(Point at) {}
		void Control::RightButtonUp(Point at) {}
		void Control::RightButtonDoubleClick(Point at) {}
		void Control::MouseMove(Point at) {}
		void Control::ScrollVertically(double delta) { if (__parent) __parent->ScrollVertically(delta); }
		void Control::ScrollHorizontally(double delta) { if (__parent) __parent->ScrollHorizontally(delta); }
		bool Control::KeyDown(int key_code) { return false; }
		void Control::KeyUp(int key_code) {}
		void Control::CharDown(uint32 ucs_code) {}
		void Control::PopupMenuCancelled(void) {}
		void Control::Timer(void) {}
		Control * Control::HitTest(Point at) { return this; }
		void Control::SetCursor(Point at) { SelectCursor(Windows::SystemCursorClass::Arrow); }
		void Control::SelectCursor(Windows::SystemCursorClass cursor) { if (__system) UI::SelectCursor(__system->GetWindow(), cursor); }
		bool Control::IsWindowEventEnabled(Windows::WindowHandler handler) { return false; }
		void Control::HandleWindowEvent(Windows::WindowHandler handler) {}
		ControlRefreshPeriod Control::GetFocusedRefreshPeriod(void) { return ControlRefreshPeriod::None; }
		string Control::GetControlClass(void) { return L"Control"; }
		Control * Control::GetParent(void) { return __parent; }
		ControlSystem * Control::GetControlSystem(void) { return __system; }
		Windows::IWindow * Control::GetWindow(void) { return __system ? __system->_window : 0; }
		void Control::Beep(void) { if (__system) __system->Beep(); }
		void Control::SetOrder(ControlDepthOrder order)
		{
			if (__parent) {
				auto index = __parent->GetChildIndex(this);
				if (order == ControlDepthOrder::SetFirst) {
					for (int i = index + 1; i < __parent->__children.Length(); i++) __parent->__children.SwapAt(i - 1, i);
				} else if (order == ControlDepthOrder::SetLast) {
					for (int i = index - 1; i >= 0; i--) __parent->__children.SwapAt(i + 1, i);
				} else if (order == ControlDepthOrder::MoveUp) {
					if (index < __parent->__children.Length() - 1) __parent->__children.SwapAt(index, index + 1);
				} else if (order == ControlDepthOrder::MoveDown) {
					if (index > 0) __parent->__children.SwapAt(index, index - 1);
				}
			}
		}
		int Control::ChildrenCount(void) { return __children.Length(); }
		Control * Control::Child(int index) { return __children.ElementAt(index); }
		void Control::SetFocus(void) { if (__system) __system->SetFocus(this); }
		Control * Control::GetFocus(void) { return __system ? __system->GetFocus() : 0; }
		void Control::SetCapture(void) { if (__system) __system->SetCapture(this); }
		Control * Control::GetCapture(void) { return __system ? __system->GetCapture() : 0; }
		void Control::ReleaseCapture(void) { if (__system) __system->ReleaseCapture(); }
		Graphics::I2DDeviceContext * Control::GetRenderingDevice(void) { return __system ? __system->GetRenderingDevice() : 0; }
		Windows::ICursor * Control::GetSystemCursor(Windows::SystemCursorClass cursor) { return __system ? __system->GetSystemCursor(cursor) : 0; }
		void Control::AddChild(Control * control) { __children.Append(control); control->__parent = this; control->__set_system(__system); }
		int Control::GetChildIndex(Control * control) { for (int i = 0; i < __children.Length(); i++) if (__children.ElementAt(i) == control) return i; return -1; }
		int Control::GetIndexAtParent(void) { return __parent ? __parent->GetChildIndex(this) : -1; }
		void Control::RemoveChild(Control * control) { int index = GetChildIndex(control); if (index >= 0) RemoveChildAt(index); }
		void Control::RemoveChildAt(int index) { __children[index].__set_system(0); __children[index].ResetCache(); __children.Remove(index); }
		void Control::RemoveFromParent(void) { if (__parent) __parent->RemoveChild(this); }
		void Control::DeferredRemoveFromParent(void) { auto self = this; Windows::GetWindowSystem()->SubmitTask(CreateFunctionalTask([self]() { self->RemoveFromParent(); })); }
		Box Control::GetAbsolutePosition(void)
		{
			auto result = ControlBoundaries;
			auto * current = __parent;
			while (current) {
				result.Left += current->ControlBoundaries.Left;
				result.Top += current->ControlBoundaries.Top;
				result.Right += current->ControlBoundaries.Left;
				result.Bottom += current->ControlBoundaries.Top;
				current = current->__parent;
			}
			return result;
		}
		bool Control::IsGeneralizedParent(Control * control)
		{
			if (this == control) return true;
			auto current = this;
			while (current->__parent) {
				current = current->__parent;
				if (current == control) return true;
			}
			return false;
		}
		bool Control::IsAccessible(void)
		{
			auto current = this;
			do {
				if (!current->IsEnabled() || !current->IsVisible()) return false;
				current = current->__parent;
			} while (current && current->__parent);
			return true;
		}
		bool Control::IsHovered(void)
		{
			if (__system) {
				auto gl = Windows::GetWindowSystem()->GetCursorPosition();
				auto pt = __system->ConvertClientScreenToVirtual(__system->_window->PointGlobalToClient(gl));
				return __system->_window->PointHitTest(gl) && __system->HitTest(pt) == this;
			} else return false;
		}
		void Control::MoveAnimated(const Rectangle & to, uint32 duration, Animation::AnimationClass begin_class, Animation::AnimationClass end_class, Animation::AnimationAction action)
		{
			SafePointer<ControlRectangleAnimationController> controller = new ControlRectangleAnimationController;
			controller->Target.SetRetain(this);
			controller->Action = action;
			controller->Duration = duration;
			controller->InitialState = GetRectangle();
			controller->FinalState = to;
			controller->InitialAnimationClass = begin_class;
			controller->FinalAnimationClass = end_class;
			__system->LaunchAnimation(controller);
		}
		void Control::MoveAnimated(const Box & to, uint32 duration, Animation::AnimationClass begin_class, Animation::AnimationClass end_class, Animation::AnimationAction action)
		{
			SafePointer<ControlPositionAnimationController> controller = new ControlPositionAnimationController;
			controller->Target.SetRetain(this);
			controller->Action = action;
			controller->Duration = duration;
			controller->InitialState = ControlPositionAnimationController::GetVector(GetPosition());
			controller->FinalState = ControlPositionAnimationController::GetVector(to);
			controller->InitialAnimationClass = begin_class;
			controller->FinalAnimationClass = end_class;
			__system->LaunchAnimation(controller);
		}
		void Control::HideAnimated(Animation::SlideSide side, uint32 duration, Animation::AnimationClass begin, Animation::AnimationClass end)
		{
			auto rect = GetRectangle();
			if (rect.IsValid()) {
				Coordinate shift_x, shift_y;
				if (side == Animation::SlideSide::Left) {
					shift_x = -rect.Right;
					shift_y = Coordinate(0);
				} else if (side == Animation::SlideSide::Top) {
					shift_x = Coordinate(0);
					shift_y = -rect.Bottom;
				} else if (side == Animation::SlideSide::Right) {
					shift_x = Coordinate::Right() - rect.Left;
					shift_y = Coordinate(0);
				} else if (side == Animation::SlideSide::Bottom) {
					shift_x = Coordinate(0);
					shift_y = Coordinate::Bottom() - rect.Top;
				}
				SafePointer<ControlRectangleAnimationController> controller = new ControlRectangleAnimationController;
				controller->Target.SetRetain(this);
				controller->Action = Animation::AnimationAction::HideWindowKeepPosition;
				controller->Duration = duration;
				controller->FinalAnimationClass = end;
				controller->FinalState = Rectangle(rect.Left + shift_x, rect.Top + shift_y, rect.Right + shift_x, rect.Bottom + shift_y);
				controller->InitialAnimationClass = begin;
				controller->InitialState = rect;
				__system->LaunchAnimation(controller);
			} else {
				auto box = ControlBoundaries;
				auto parent = __parent ? __parent->ControlBoundaries : Box(0, 0, 0, 0);
				auto right = parent.Right - parent.Left;
				auto bottom = parent.Bottom - parent.Top;
				int shift_x, shift_y;
				if (side == Animation::SlideSide::Left) {
					shift_x = -box.Right;
					shift_y = 0;
				} else if (side == Animation::SlideSide::Top) {
					shift_x = 0;
					shift_y = -box.Bottom;
				} else if (side == Animation::SlideSide::Right) {
					shift_x = right - box.Left;
					shift_y = 0;
				} else if (side == Animation::SlideSide::Bottom) {
					shift_x = 0;
					shift_y = bottom - box.Top;
				}
				SafePointer<ControlPositionAnimationController> controller = new ControlPositionAnimationController;
				controller->Target.SetRetain(this);
				controller->Action = Animation::AnimationAction::HideWindowKeepPosition;
				controller->Duration = duration;
				controller->FinalAnimationClass = end;
				controller->FinalState = ControlPositionAnimationController::GetVector(Box(box.Left + shift_x, box.Top + shift_y, box.Right + shift_x, box.Bottom + shift_y));
				controller->InitialAnimationClass = begin;
				controller->InitialState = ControlPositionAnimationController::GetVector(box);
				__system->LaunchAnimation(controller);
			}
		}
		void Control::ShowAnimated(Animation::SlideSide side, uint32 duration, Animation::AnimationClass end, Animation::AnimationClass begin)
		{
			auto rect = GetRectangle();
			if (rect.IsValid()) {
				Coordinate shift_x, shift_y;
				if (side == Animation::SlideSide::Left) {
					shift_x = -rect.Right;
					shift_y = Coordinate(0);
				} else if (side == Animation::SlideSide::Top) {
					shift_x = Coordinate(0);
					shift_y = -rect.Bottom;
				} else if (side == Animation::SlideSide::Right) {
					shift_x = Coordinate::Right() - rect.Left;
					shift_y = Coordinate(0);
				} else if (side == Animation::SlideSide::Bottom) {
					shift_x = Coordinate(0);
					shift_y = Coordinate::Bottom() - rect.Top;
				}
				SafePointer<ControlRectangleAnimationController> controller = new ControlRectangleAnimationController;
				controller->Target.SetRetain(this);
				controller->Action = Animation::AnimationAction::ShowWindow;
				controller->Duration = duration;
				controller->FinalAnimationClass = end;
				controller->FinalState = rect;
				controller->InitialAnimationClass = begin;
				controller->InitialState = Rectangle(rect.Left + shift_x, rect.Top + shift_y, rect.Right + shift_x, rect.Bottom + shift_y);
				__system->LaunchAnimation(controller);
			} else {
				auto box = ControlBoundaries;
				auto parent = __parent ? __parent->ControlBoundaries : Box(0, 0, 0, 0);
				auto right = parent.Right - parent.Left;
				auto bottom = parent.Bottom - parent.Top;
				int shift_x, shift_y;
				if (side == Animation::SlideSide::Left) {
					shift_x = -box.Right;
					shift_y = 0;
				} else if (side == Animation::SlideSide::Top) {
					shift_x = 0;
					shift_y = -box.Bottom;
				} else if (side == Animation::SlideSide::Right) {
					shift_x = right - box.Left;
					shift_y = 0;
				} else if (side == Animation::SlideSide::Bottom) {
					shift_x = 0;
					shift_y = bottom - box.Top;
				}
				SafePointer<ControlPositionAnimationController> controller = new ControlPositionAnimationController;
				controller->Target.SetRetain(this);
				controller->Action = Animation::AnimationAction::HideWindowKeepPosition;
				controller->Duration = duration;
				controller->FinalAnimationClass = end;
				controller->FinalState = ControlPositionAnimationController::GetVector(box);
				controller->InitialAnimationClass = begin;
				controller->InitialState = ControlPositionAnimationController::GetVector(Box(box.Left + shift_x, box.Top + shift_y, box.Right + shift_x, box.Bottom + shift_y));
				__system->LaunchAnimation(controller);
			}
		}
		Control * Control::GetNextTabStopControl(void)
		{
			auto current = this;
			auto index = GetIndexAtParent();
			do {
				if (current->__children.Length() && current->IsEnabled() && current->IsVisible()) {
					current = current->__children.FirstElement();
					index = 0;
				} else {
					int super_index = index;
					while (current->__parent && super_index == current->__parent->__children.Length() - 1) {
						current = current->__parent;
						super_index = current->GetIndexAtParent();
					}
					if (current->__parent) {
						index = super_index + 1;
						current = current->__parent->__children.ElementAt(index);
					}
				}
				if (current == this) return this;
			} while (!current->IsTabStop() || !current->IsEnabled() || !current->IsVisible());
			return current;
		}
		Control * Control::GetPreviousTabStopControl(void)
		{
			auto current = this;
			auto index = GetIndexAtParent();
			do {
				if (!current->__parent) {
					while (current->__children.Length() && current->IsEnabled() && current->IsVisible()) current = current->__children.LastElement();
					index = current->GetIndexAtParent();
				} else {
					if (index == 0) {
						current = current->__parent;
						index = current->GetIndexAtParent();
					} else {
						index--;
						current = current->__parent->__children.ElementAt(index);
						while (current->__children.Length() && current->IsEnabled() && current->IsVisible()) current = current->__children.LastElement();
						index = current->GetIndexAtParent();
					}
				}
				if (current == this) return this;
			} while (!current->IsTabStop() || !current->IsEnabled() || !current->IsVisible());
			return current;
		}
		void Control::Invalidate(void) { if (__system) __system->Invalidate(); }
		void Control::SubmitEvent(int ID)
		{
			int id = ID; SafePointer<Control> self; self.SetRetain(this);
			Windows::GetWindowSystem()->SubmitTask(CreateFunctionalTask([id, self]() { self->RaiseEvent(id, ControlEvent::Deferred, 0); }));
		}
		void Control::SubmitTask(IDispatchTask * task) { Windows::GetWindowSystem()->SubmitTask(task); }
		void Control::BeginSubmit(void) {}
		void Control::AppendTask(IDispatchTask * task) { Windows::GetWindowSystem()->SubmitTask(task); }
		void Control::EndSubmit(void) {}

		void IEventCallback::Timer(Windows::IWindow * window) {}
		void IEventCallback::PopupMenuCancelled(Windows::IWindow * window) {}
		void IEventCallback::HandleControlEvent(Windows::IWindow * window, int ID, ControlEvent event, Control * sender) {}

		ControlSystem::ControlSystemWindowCallback::ControlSystemWindowCallback(ControlSystem & _system) : system(_system) {}
		void ControlSystem::ControlSystemWindowCallback::Created(Windows::IWindow * window)
		{
			system._window = window;
			system._initializer->PostInitialize();
			if (system._event_callback) system._event_callback->Created(window);
		}
		void ControlSystem::ControlSystemWindowCallback::Destroyed(Windows::IWindow * window)
		{
			if (system._event_callback) system._event_callback->Destroyed(window);
			system._event_callback = 0;
			window->SetCallback(0);
			system.Release();
			delete this;
		}
		void ControlSystem::ControlSystemWindowCallback::Shown(Windows::IWindow * window, bool show) { if (system._event_callback) system._event_callback->Shown(window, show); }
		void ControlSystem::ControlSystemWindowCallback::RenderWindow(Windows::IWindow * window)
		{
			if (system._preferred_device_class != Windows::DeviceClass::Null) {
				if (system._engine) {
					bool alive;
					if (system._engine->BeginRenderingPass()) {
						system.Render(system._engine->GetContext());
						alive = system._engine->EndRenderingPass();
					} else alive = false;
					if (!alive) {
						system._window->SetPresentationEngine(0);
						system._engine.SetReference(0);
						system.SetRenderingDevice(0);
						Graphics::ResetCommonDevice();
						system._engine.SetRetain(system._window->Set2DRenderingDevice(system._preferred_device_class));
						system.SetRenderingDevice(system._engine->GetContext());
						system._root->ResetCache();
					}
				}
			} else if (system._event_callback) system._event_callback->RenderWindow(window);
		}
		void ControlSystem::ControlSystemWindowCallback::WindowClose(Windows::IWindow * window) { if (system._event_callback) system._event_callback->WindowClose(window); }
		void ControlSystem::ControlSystemWindowCallback::WindowMaximize(Windows::IWindow * window) { if (system._event_callback) system._event_callback->WindowMaximize(window); }
		void ControlSystem::ControlSystemWindowCallback::WindowMinimize(Windows::IWindow * window) { if (system._event_callback) system._event_callback->WindowMinimize(window); }
		void ControlSystem::ControlSystemWindowCallback::WindowRestore(Windows::IWindow * window) { if (system._event_callback) system._event_callback->WindowRestore(window); }
		void ControlSystem::ControlSystemWindowCallback::WindowHelp(Windows::IWindow * window) { if (system._event_callback) system._event_callback->WindowHelp(window); }
		void ControlSystem::ControlSystemWindowCallback::WindowActivate(Windows::IWindow * window) { if (system._event_callback) system._event_callback->WindowActivate(window); }
		void ControlSystem::ControlSystemWindowCallback::WindowDeactivate(Windows::IWindow * window) { if (system._event_callback) system._event_callback->WindowDeactivate(window); }
		void ControlSystem::ControlSystemWindowCallback::WindowMove(Windows::IWindow * window) { if (system._event_callback) system._event_callback->WindowMove(window); }
		void ControlSystem::ControlSystemWindowCallback::WindowSize(Windows::IWindow * window)
		{
			if (system._autosize) {
				auto size = window->GetClientSize();
				if (size.x >= 1 && size.y >= 1) {
					system._virtual_size = size;
					system._root->SetPosition(Box(0, 0, system._virtual_size.x, system._virtual_size.y));
				}
			}
			if (system._event_callback) system._event_callback->WindowSize(window);
		}
		void ControlSystem::ControlSystemWindowCallback::FocusChanged(Windows::IWindow * window, bool got)
		{
			if (!got) {
				auto focus = system._focused;
				system._focused.SetReference(0);
				if (focus) focus->FocusChanged(false);
				system.ResetEffectiveRefreshRate();
			}
		}
		bool ControlSystem::ControlSystemWindowCallback::KeyDown(Windows::IWindow * window, int key_code)
		{
			if (!system._exclusive && system._root->TranslateAccelerators(key_code)) return true;
			if (system._focused && !system._focused->IsAccessible()) system.SetFocus(0);
			if (system._focused) return system._focused->KeyDown(key_code);
			return false;
		}
		void ControlSystem::ControlSystemWindowCallback::KeyUp(Windows::IWindow * window, int key_code)
		{
			if (system._focused && !system._focused->IsAccessible()) system.SetFocus(0);
			if (system._focused) system._focused->KeyUp(key_code);
		}
		void ControlSystem::ControlSystemWindowCallback::CharDown(Windows::IWindow * window, uint32 ucs_code)
		{
			if (system._focused && !system._focused->IsAccessible()) system.SetFocus(0);
			if (system._focused) system._focused->CharDown(ucs_code);
		}
		void ControlSystem::ControlSystemWindowCallback::CaptureChanged(Windows::IWindow * window, bool got)
		{
			if (!got) {
				if (system._captured) {
					auto control = system._captured;
					system._captured.SetReference(0);
					control->CaptureChanged(false);
				}
				if (system._exclusive) {
					auto control = system._exclusive;
					system._exclusive.SetReference(0);
					control->LostExclusiveMode();
				}
			}
		}
		void ControlSystem::ControlSystemWindowCallback::SetCursor(Windows::IWindow * window, Point at) {}
		void ControlSystem::ControlSystemWindowCallback::MouseMove(Windows::IWindow * window, Point at)
		{
			if (system._captured && !system._captured->IsAccessible()) system.SetCapture(0);
			Control * responder; Point lp;
			if (system._captured) {
				responder = system._captured;
				lp = system.ConvertClientToControl(responder, system.ConvertClientScreenToVirtual(at));
			} else system.EvaluateControlAt(system.ConvertClientScreenToVirtual(at), &responder, &lp);
			if (responder) {
				responder->SetCursor(lp);
				responder->MouseMove(lp);
			}
		}
		void ControlSystem::ControlSystemWindowCallback::LeftButtonDown(Windows::IWindow * window, Point at)
		{
			if (system._captured && !system._captured->IsAccessible()) system.SetCapture(0);
			Control * responder; Point lp;
			if (system._captured) {
				responder = system._captured;
				lp = system.ConvertClientToControl(responder, system.ConvertClientScreenToVirtual(at));
			} else system.EvaluateControlAt(system.ConvertClientScreenToVirtual(at), &responder, &lp);
			if (responder) responder->LeftButtonDown(lp); else if (system._exclusive) system.SetExclusiveControl(0);
		}
		void ControlSystem::ControlSystemWindowCallback::LeftButtonUp(Windows::IWindow * window, Point at)
		{
			if (system._captured && !system._captured->IsAccessible()) system.SetCapture(0);
			Control * responder; Point lp;
			if (system._captured) {
				responder = system._captured;
				lp = system.ConvertClientToControl(responder, system.ConvertClientScreenToVirtual(at));
			} else system.EvaluateControlAt(system.ConvertClientScreenToVirtual(at), &responder, &lp);
			if (responder) responder->LeftButtonUp(lp);
		}
		void ControlSystem::ControlSystemWindowCallback::LeftButtonDoubleClick(Windows::IWindow * window, Point at)
		{
			if (system._captured && !system._captured->IsAccessible()) system.SetCapture(0);
			Control * responder; Point lp;
			if (system._captured) {
				responder = system._captured;
				lp = system.ConvertClientToControl(responder, system.ConvertClientScreenToVirtual(at));
			} else system.EvaluateControlAt(system.ConvertClientScreenToVirtual(at), &responder, &lp);
			if (responder) responder->LeftButtonDoubleClick(lp);
		}
		void ControlSystem::ControlSystemWindowCallback::RightButtonDown(Windows::IWindow * window, Point at)
		{
			if (system._captured && !system._captured->IsAccessible()) system.SetCapture(0);
			Control * responder; Point lp;
			if (system._captured) {
				responder = system._captured;
				lp = system.ConvertClientToControl(responder, system.ConvertClientScreenToVirtual(at));
			} else system.EvaluateControlAt(system.ConvertClientScreenToVirtual(at), &responder, &lp);
			if (responder) responder->RightButtonDown(lp); else if (system._exclusive) system.SetExclusiveControl(0);
		}
		void ControlSystem::ControlSystemWindowCallback::RightButtonUp(Windows::IWindow * window, Point at)
		{
			if (system._captured && !system._captured->IsAccessible()) system.SetCapture(0);
			Control * responder; Point lp;
			if (system._captured) {
				responder = system._captured;
				lp = system.ConvertClientToControl(responder, system.ConvertClientScreenToVirtual(at));
			} else system.EvaluateControlAt(system.ConvertClientScreenToVirtual(at), &responder, &lp);
			if (responder) responder->RightButtonUp(lp);
		}
		void ControlSystem::ControlSystemWindowCallback::RightButtonDoubleClick(Windows::IWindow * window, Point at)
		{
			if (system._captured && !system._captured->IsAccessible()) system.SetCapture(0);
			Control * responder; Point lp;
			if (system._captured) {
				responder = system._captured;
				lp = system.ConvertClientToControl(responder, system.ConvertClientScreenToVirtual(at));
			} else system.EvaluateControlAt(system.ConvertClientScreenToVirtual(at), &responder, &lp);
			if (responder) responder->RightButtonDoubleClick(lp);
		}
		void ControlSystem::ControlSystemWindowCallback::ScrollVertically(Windows::IWindow * window, double delta)
		{
			if (system._captured && !system._captured->IsAccessible()) system.SetCapture(0);
			if (!system._captured) {
				auto gl = Windows::GetWindowSystem()->GetCursorPosition();
				auto responder = system._window->PointHitTest(gl) ? system.AccessibleHitTest(system.ConvertClientScreenToVirtual(system._window->PointGlobalToClient(gl))) : 0;
				if (responder) responder->ScrollVertically(delta);
			} else system._captured->ScrollVertically(delta);
		}
		void ControlSystem::ControlSystemWindowCallback::ScrollHorizontally(Windows::IWindow * window, double delta)
		{
			if (system._captured && !system._captured->IsAccessible()) system.SetCapture(0);
			if (!system._captured) {
				auto gl = Windows::GetWindowSystem()->GetCursorPosition();
				auto responder = system._window->PointHitTest(gl) ? system.AccessibleHitTest(system.ConvertClientScreenToVirtual(system._window->PointGlobalToClient(gl))) : 0;
				if (responder) responder->ScrollHorizontally(delta);
			} else system._captured->ScrollHorizontally(delta);
		}
		void ControlSystem::ControlSystemWindowCallback::Timer(Windows::IWindow * window, int timer_id)
		{
			if (timer_id == 1) {
				window->InvalidateContents();
			} else {
				auto control = system._timers.GetObjectByKey(timer_id);
				if (control) control->Timer();
			}
		}
		void ControlSystem::ControlSystemWindowCallback::ThemeChanged(Windows::IWindow * window) { if (system._event_callback) system._event_callback->ThemeChanged(window); }
		bool ControlSystem::ControlSystemWindowCallback::IsWindowEventEnabled(Windows::IWindow * window, Windows::WindowHandler handler)
		{
			if (system._event_callback && system._event_callback->IsWindowEventEnabled(window, handler)) return true;
			if (system._focused && system._focused->IsWindowEventEnabled(handler)) return true;
			return false;
		}
		void ControlSystem::ControlSystemWindowCallback::HandleWindowEvent(Windows::IWindow * window, Windows::WindowHandler handler)
		{
			if (system._event_callback && system._event_callback->IsWindowEventEnabled(window, handler)) {
				system._event_callback->HandleWindowEvent(window, handler);
			} else if (system._focused) system._focused->HandleWindowEvent(handler);
		}

		ControlSystem::ControlSystemInitializer::ControlSystemInitializer(void) {}
		void ControlSystem::ControlSystemInitializer::PreInitialize(void)
		{
			if (FrameTemplate && FrameTemplate->Properties->GetTemplateClass() != L"DialogFrame") { Release(); throw InvalidArgumentException(); }
			System = new ControlSystem;
			System->_event_callback = static_cast<IEventCallback *>(WindowDesc.Callback);
			System->_initializer.SetReference(this);
			WindowDesc.Callback = System->_window_callback;
			if (FrameTemplate) {
				Template::Controls::FrameExtendedData * ex_data = 0;
				for (auto & child : FrameTemplate->Children) if (child.Properties->GetTemplateClass() == L"FrameExtendedData") {
					ex_data = static_cast<Template::Controls::FrameExtendedData *>(child.Properties);
					break;
				}
				auto data = static_cast<Template::Controls::DialogFrame *>(FrameTemplate->Properties);
				WindowDesc.Flags = 0;
				if (data->Captioned) WindowDesc.Flags |= Windows::WindowFlagHasTitle;
				if (data->Sizeble) WindowDesc.Flags |= Windows::WindowFlagSizeble;
				if (data->CloseButton) WindowDesc.Flags |= Windows::WindowFlagCloseButton;
				if (data->MinimizeButton) WindowDesc.Flags |= Windows::WindowFlagMinimizeButton;
				if (data->MaximizeButton) WindowDesc.Flags |= Windows::WindowFlagMaximizeButton;
				if (data->HelpButton) WindowDesc.Flags |= Windows::WindowFlagHelpButton;
				if (data->ToolWindow) WindowDesc.Flags |= Windows::WindowFlagToolWindow;
				if (ex_data) {
					if (ex_data->WindowsLeftMargin || ex_data->WindowsTopMargin || ex_data->WindowsRightMargin || ex_data->WindowsBottomMargin) {
						WindowDesc.Flags |= Windows::WindowFlagWindowsExtendedFrame;
						WindowDesc.FrameMargins = Box(ex_data->WindowsLeftMargin, ex_data->WindowsTopMargin, ex_data->WindowsRightMargin, ex_data->WindowsBottomMargin);
					}
					if (ex_data->WindowsTabbedTitle) WindowDesc.Flags |= Windows::WindowFlagWindowsTabbedTitle;
					else if (ex_data->WindowsTransientTitle) WindowDesc.Flags |= Windows::WindowFlagWindowsTransientTitle;
					else if (ex_data->WindowsColoredTitle) {
						WindowDesc.Flags |= Windows::WindowFlagWindowsColoredTitle;
						WindowDesc.BackgroundColor = ex_data->WindowsTitleColor;
					}
					if (ex_data->MacTransparentTitle) WindowDesc.Flags |= Windows::WindowFlagCocoaTransparentTitle;
					if (ex_data->MacEffectBackground || ex_data->MacEffectBackgroundMaterial.Length()) WindowDesc.Flags |= Windows::WindowFlagCocoaEffectBackground;
					if (ex_data->MacUseLightTheme) {
						WindowDesc.Flags |= Windows::WindowFlagOverrideTheme;
						WindowDesc.Theme = Windows::ThemeClass::Light;
					}
					if (ex_data->MacUseDarkTheme) {
						WindowDesc.Flags |= Windows::WindowFlagOverrideTheme;
						WindowDesc.Theme = Windows::ThemeClass::Dark;
					}
					if (ex_data->MacShadowlessWindow) WindowDesc.Flags |= Windows::WindowFlagCocoaShadowless;
					if (ex_data->MacCustomBackgroundColor) {
						WindowDesc.Flags |= Windows::WindowFlagCocoaCustomBackground;
						WindowDesc.BackgroundColor = ex_data->MacBackgroundColor;
					}
				}
				if (data->Transparentcy) {
					WindowDesc.Flags |= Windows::WindowFlagNonOpaque;
					WindowDesc.Opacity = 1.0 - data->Transparentcy;
				}
				if (data->Transparent) WindowDesc.Flags |= Windows::WindowFlagTransparent;
				if (data->BlurBehind) {
					WindowDesc.Flags |= Windows::WindowFlagBlurBehind;
					if (data->BlurFactor) {
						WindowDesc.Flags |= Windows::WindowFlagBlurFactor;
						WindowDesc.BlurFactor = data->BlurFactor;
					}
				}
				WindowDesc.Title = data->Title;
				Box outer_box;
				if (WindowDesc.ParentWindow) {
					outer_box = WindowDesc.ParentWindow->GetPosition();
				} else {
					SafePointer<Windows::IScreen> screen = Windows::GetDefaultScreen();
					outer_box = screen->GetUserRectangle();
				}
				if (OuterRectangle.IsValid()) outer_box = Box(OuterRectangle, outer_box);
				auto client_box = Box(data->ControlPosition, outer_box);
				auto window_size = Windows::GetWindowSystem()->ConvertClientToWindow(Point(client_box.Right - client_box.Left, client_box.Bottom - client_box.Top), WindowDesc.Flags);
				WindowDesc.Position.Left = outer_box.Left + (outer_box.Right - outer_box.Left - window_size.x) / 2;
				WindowDesc.Position.Right = WindowDesc.Position.Left + window_size.x;
				WindowDesc.Position.Top = outer_box.Top + (outer_box.Bottom - outer_box.Top - window_size.y) / 2;
				WindowDesc.Position.Bottom = WindowDesc.Position.Top + window_size.y;
				if (data->MinimalWidth || data->MinimalHeight) WindowDesc.MinimalConstraints = Windows::GetWindowSystem()->ConvertClientToWindow(Point(data->MinimalWidth, data->MinimalHeight), WindowDesc.Flags);
				else WindowDesc.MinimalConstraints = Point(0, 0);
				if (data->MaximalWidth || data->MaximalHeight) WindowDesc.MaximalConstraints = Windows::GetWindowSystem()->ConvertClientToWindow(Point(data->MaximalWidth, data->MaximalHeight), WindowDesc.Flags);
				else WindowDesc.MaximalConstraints = Point(0, 0);
				WindowDesc.Screen = 0;
				ExtendedData = ex_data;
				Data = data;
			} else {
				ExtendedData = 0;
				Data = 0;
			}
		}
		void ControlSystem::ControlSystemInitializer::PostInitialize(void)
		{
			if (ExtendedData) {
				auto ex_data = static_cast<Template::Controls::FrameExtendedData *>(ExtendedData);
				#ifdef ENGINE_MACOSX
				if (ex_data->MacTouchBar) {
					SafePointer<MacOSXSpecific::TouchBar> touch_bar = MacOSXSpecific::SetTouchBarFromTemplate(System->GetWindow(), ex_data->MacTouchBar);
					touch_bar->SetCallback(System->GetCallback());
				}
				#endif
				if (ex_data->MacEffectBackgroundMaterial.Length()) {
					Windows::CocoaEffectMaterial material = Windows::CocoaEffectMaterial::WindowBackground;
					if (ex_data->MacEffectBackgroundMaterial == L"Titlebar") material = Windows::CocoaEffectMaterial::Titlebar;
					else if (ex_data->MacEffectBackgroundMaterial == L"Selection") material = Windows::CocoaEffectMaterial::Selection;
					else if (ex_data->MacEffectBackgroundMaterial == L"Menu") material = Windows::CocoaEffectMaterial::Menu;
					else if (ex_data->MacEffectBackgroundMaterial == L"Popover") material = Windows::CocoaEffectMaterial::Popover;
					else if (ex_data->MacEffectBackgroundMaterial == L"Sidebar") material = Windows::CocoaEffectMaterial::Sidebar;
					else if (ex_data->MacEffectBackgroundMaterial == L"HeaderView") material = Windows::CocoaEffectMaterial::HeaderView;
					else if (ex_data->MacEffectBackgroundMaterial == L"Sheet") material = Windows::CocoaEffectMaterial::Sheet;
					else if (ex_data->MacEffectBackgroundMaterial == L"WindowBackground") material = Windows::CocoaEffectMaterial::WindowBackground;
					else if (ex_data->MacEffectBackgroundMaterial == L"HUD") material = Windows::CocoaEffectMaterial::HUD;
					else if (ex_data->MacEffectBackgroundMaterial == L"FullScreenUI") material = Windows::CocoaEffectMaterial::FullScreenUI;
					else if (ex_data->MacEffectBackgroundMaterial == L"ToolTip") material = Windows::CocoaEffectMaterial::ToolTip;
					System->GetWindow()->SetCocoaEffectMaterial(material);
				}
			}
			System->SetPrefferedDevice(DeviceClass);
			System->SetVirtualClientAutoresize(true);
			if (FrameTemplate) {
				auto data = static_cast<Template::Controls::DialogFrame *>(Data);
				if (data->Background) {
					System->GetRootControl()->SetBackground(data->Background);
				} else {
					Color background = 0;
					if (data->DefaultBackground) {
						SafePointer<Windows::ITheme> theme = System->GetWindow()->GetCurrentTheme();
						background = theme->GetColor(Windows::ThemeColor::WindowBackgroup);
					} else background = data->BackgroundColor;
					if (background.a) {
						SafePointer<Template::BarShape> shape = new Template::BarShape;
						shape->Position = Rectangle::Entire();
						shape->Gradient << Template::GradientPoint(Template::ColorTemplate(background), 0.0);
						System->GetRootControl()->SetBackground(shape);
					}
				}
				CreateChildrenControls(System->GetRootControl(), FrameTemplate, Factory);
				System->_root->ArrangeChildren();
			}
			System->_initializer.SetReference(0);
		}
		Windows::IWindow * ControlSystem::ControlSystemInitializer::CreateWindow(void) { PreInitialize(); return Windows::GetWindowSystem()->CreateWindow(WindowDesc); }
		Windows::IWindow * ControlSystem::ControlSystemInitializer::CreateModalWindow(void) { PreInitialize(); return Windows::GetWindowSystem()->CreateModalWindow(WindowDesc); }

		ControlSystem::ControlSystem(void) : _animations(0x10), _preferred_device_class(Windows::DeviceClass::Null), _caret_width(int(CurrentScaleFactor)), _virtual_size(1, 1), _autosize(false), _beep(true), _event_callback(0), _window(0),
			_general_rate(ControlRefreshPeriod::None), _evaluated_rate(ControlRefreshPeriod::None)
		{
			_window_callback = new ControlSystemWindowCallback(*this);
			_root = new RootControl;
			_root->__system = this;
		}
		void ControlSystem::ResetEffectiveRefreshRate(void)
		{
			auto _focus_rate = _focused ? _focused->GetFocusedRefreshPeriod() : ControlRefreshPeriod::None;
			auto _animation_rate = _animations.Length() ? ControlRefreshPeriod::Cinematic : ControlRefreshPeriod::None;
			uint32 frame_time;
			if (_focus_rate == ControlRefreshPeriod::Cinematic || _animation_rate == ControlRefreshPeriod::Cinematic || _general_rate == ControlRefreshPeriod::Cinematic) { frame_time = 30; _evaluated_rate = ControlRefreshPeriod::Cinematic; }
			else if (_focus_rate == ControlRefreshPeriod::CaretBlink || _general_rate == ControlRefreshPeriod::CaretBlink) { frame_time = _device ? (_device->GetCaretBlinkPeriod() / 2) : 500; _evaluated_rate = ControlRefreshPeriod::CaretBlink; }
			else { frame_time = 0; _evaluated_rate = ControlRefreshPeriod::None; }
			_window->SetTimer(1, frame_time);
		}
		void ControlSystem::ResetRenderingDevice(void)
		{
			auto device = GetRenderingDevice();
			_window->SetPresentationEngine(0);
			_engine.SetReference(0);
			SetRenderingDevice(0);
			if (_preferred_device_class != Windows::DeviceClass::Null) {
				_engine.SetRetain(_window->Set2DRenderingDevice(_preferred_device_class));
				SetRenderingDevice(_engine->GetContext());
			}
			if (device) _root->ResetCache();
		}
		ControlSystem::~ControlSystem(void) { _root->__set_system(0); _root->ResetCache(); }
		void ControlSystem::SubmitTask(IDispatchTask * task) { Windows::GetWindowSystem()->SubmitTask(task); }
		void ControlSystem::BeginSubmit(void) {}
		void ControlSystem::AppendTask(IDispatchTask * task) { Windows::GetWindowSystem()->SubmitTask(task); }
		void ControlSystem::EndSubmit(void) {}
		void ControlSystem::SetCaretWidth(int width) { _caret_width = width; }
		int ControlSystem::GetCaretWidth(void) { return _caret_width; }
		void ControlSystem::SetRefreshPeriod(ControlRefreshPeriod period) { _general_rate = period; ResetEffectiveRefreshRate(); }
		ControlRefreshPeriod ControlSystem::GetRefreshPeriod(void) { return _general_rate; }
		void ControlSystem::SetRenderingDevice(Graphics::I2DDeviceContext * device) { _device.SetRetain(device); }
		Graphics::I2DDeviceContext * ControlSystem::GetRenderingDevice(void) { return _device; }
		void ControlSystem::SetPrefferedDevice(Windows::DeviceClass device) { _preferred_device_class = device; ResetRenderingDevice(); }
		Windows::DeviceClass ControlSystem::GetPrefferedDevice(void) { return _preferred_device_class; }
		void ControlSystem::SetVirtualClientSize(const Point & size)
		{
			if (!_autosize) {
				_virtual_size = size;
				if (_virtual_size.x < 1) _virtual_size.x = 1;
				if (_virtual_size.y < 1) _virtual_size.y = 1;
				_root->SetPosition(Box(0, 0, _virtual_size.x, _virtual_size.y));
				Invalidate();
			}
		}
		Point ControlSystem::GetVirtualClientSize(void) { return _virtual_size; }
		void ControlSystem::SetVirtualClientAutoresize(bool autoresize)
		{
			_autosize = autoresize;
			if (_autosize) {
				auto size = _window->GetClientSize();
				if (size.x >= 1 && size.y >= 1) _virtual_size = size; else _virtual_size = Point(1, 1);
				_root->SetPosition(Box(0, 0, _virtual_size.x, _virtual_size.y));
				Invalidate();
			}
		}
		bool ControlSystem::IsVirtualClientAutoresize(void) { return _autosize; }
		void ControlSystem::SetVirtualPopupStyles(VirtualPopupStyles * styles) { _virtual_styles.SetRetain(styles); }
		VirtualPopupStyles * ControlSystem::GetVirtualPopupStyles(void) { return _virtual_styles; }
		void ControlSystem::SetCallback(IEventCallback * callback) { _event_callback = callback; }
		IEventCallback * ControlSystem::GetCallback(void) { return _event_callback; }
		Windows::ICursor * ControlSystem::GetSystemCursor(Windows::SystemCursorClass cursor)
		{
			auto result = _cursors[cursor];
			return result ? result : Windows::GetWindowSystem()->GetSystemCursor(cursor);
		}
		void ControlSystem::OverrideSystemCursor(Windows::SystemCursorClass cursor, Windows::ICursor * with)
		{
			_cursors.Remove(cursor);
			if (with) _cursors.Append(cursor, with);
		}
		RootControl * ControlSystem::GetRootControl(void) { return _root; }
		Control * ControlSystem::HitTest(Point virtual_client) { return _root->HitTest(virtual_client); }
		Control * ControlSystem::AccessibleHitTest(Point virtual_client)
		{
			auto control = HitTest(virtual_client);
			auto current = control;
			while (current) {
				if (!current->IsEnabled()) control = current->__parent;
				current = current->__parent;
			}
			if (_exclusive && control && !control->IsGeneralizedParent(_exclusive)) control = 0;
			return control;
		}
		Point ControlSystem::GetVirtualCursorPosition(void) { return ConvertClientScreenToVirtual(_window->PointGlobalToClient(Windows::GetWindowSystem()->GetCursorPosition())); }
		void ControlSystem::SetVirtualCursorPosition(Point position) { Windows::GetWindowSystem()->SetCursorPosition(_window->PointClientToGlobal(ConvertClientVirtualToScreen(position))); }
		Point ControlSystem::ConvertControlToClient(Control * control, Point point)
		{
			auto result = point;
			auto current = control;
			while (current) {
				result.x += current->ControlBoundaries.Left;
				result.y += current->ControlBoundaries.Top;
				current = current->__parent;
			}
			return result;
		}
		Point ControlSystem::ConvertClientToControl(Control * control, Point point)
		{
			auto result = point;
			auto current = control;
			while (current) {
				result.x -= current->ControlBoundaries.Left;
				result.y -= current->ControlBoundaries.Top;
				current = current->__parent;
			}
			return result;
		}
		Point ControlSystem::ConvertClientVirtualToScreen(Point point)
		{
			auto client = _window->GetClientSize();
			return Point(point.x * client.x /_virtual_size.x, point.y * client.y / _virtual_size.y);
		}
		Point ControlSystem::ConvertClientScreenToVirtual(Point point)
		{
			auto client = _window->GetClientSize();
			if (client.x < 1) client.x = 1;
			if (client.y < 1) client.y = 1;
			return Point(point.x * _virtual_size.x / client.x, point.y * _virtual_size.y / client.y);
		}
		void ControlSystem::EvaluateControlAt(Point virtual_client, Control ** control_ref, Point * local_ref)
		{
			auto control = AccessibleHitTest(virtual_client);
			if (_exclusive && control && !control->IsGeneralizedParent(_exclusive)) control = 0;
			if (control) {
				*control_ref = control;
				*local_ref = ConvertClientToControl(control, virtual_client);
			} else *control_ref = 0;
		}
		void ControlSystem::LaunchAnimation(Animation::IAnimationController * controller)
		{
			_animations.Append(controller);
			controller->InitialStateTime = GetTimerValue();
			controller->FinalStateTime = controller->InitialStateTime + controller->Duration;
			controller->OnAnimationStart();
			if (_animations.Length() == 1) ResetEffectiveRefreshRate();
		}
		void ControlSystem::MoveWindowAnimated(const Box & to, uint32 duration, Animation::AnimationClass begin_class, Animation::AnimationClass end_class)
		{
			SafePointer<WindowPositionAnimationController> controller = new WindowPositionAnimationController;
			controller->Window = _window;
			controller->InitialState = ControlPositionAnimationController::GetVector(_window->GetPosition());
			controller->FinalState = ControlPositionAnimationController::GetVector(to);
			controller->InitialAnimationClass = begin_class;
			controller->FinalAnimationClass = end_class;
			controller->Duration = duration;
			LaunchAnimation(controller);
		}
		void ControlSystem::HideWindowAnimated(uint32 duration)
		{
			SafePointer<WindowShowAnimationController> controller = new WindowShowAnimationController;
			controller->Window = _window;
			controller->Duration = duration;
			controller->InitialAnimationClass = controller->FinalAnimationClass = Animation::AnimationClass::Hard;
			controller->InitialState = 1.0;
			controller->FinalState = 0.0;
			LaunchAnimation(controller);
		}
		void ControlSystem::ShowWindowAnimated(uint32 duration)
		{
			SafePointer<WindowShowAnimationController> controller = new WindowShowAnimationController;
			controller->Window = _window;
			controller->Duration = duration;
			controller->InitialAnimationClass = controller->FinalAnimationClass = Animation::AnimationClass::Hard;
			controller->InitialState = 0.0;
			controller->FinalState = 1.0;
			LaunchAnimation(controller);
		}
		void ControlSystem::SetFocus(Control * control)
		{
			_window->SetFocus();
			if (_window->IsFocused()) {
				if (_focused.Inner() == control) return;
				auto current = _focused;
				_focused.SetRetain(control);
				if (current) current->FocusChanged(false);
				if (_focused) _focused->FocusChanged(true);
				ResetEffectiveRefreshRate();
			}
		}
		Control * ControlSystem::GetFocus(void) { return _window->IsFocused() ? _focused.Inner() : 0; }
		void ControlSystem::SetCapture(Control * control)
		{
			if (control) _window->SetCapture();
			else if (_window->IsCaptured() && !_exclusive) _window->ReleaseCapture();
			if (control == _captured.Inner()) return;
			auto current = _captured;
			_captured.SetRetain(control);
			if (current) current->CaptureChanged(false);
			if (_captured) _captured->CaptureChanged(true);
			else _window_callback->MouseMove(_window, _window->PointGlobalToClient(Windows::GetWindowSystem()->GetCursorPosition()));
		}
		Control * ControlSystem::GetCapture(void) { return _window->IsCaptured() ? _captured.Inner() : 0; }
		void ControlSystem::ReleaseCapture(void)
		{
			if (!_exclusive) _window->ReleaseCapture();
			if (!_captured) return;
			auto current = _captured;
			_captured.SetReference(0);
			current->CaptureChanged(false);
			_window_callback->MouseMove(_window, _window->PointGlobalToClient(Windows::GetWindowSystem()->GetCursorPosition()));
		}
		void ControlSystem::SetExclusiveControl(Control * control)
		{
			if (control) _window->SetCapture();
			else if (_window->IsCaptured() && !_captured) _window->ReleaseCapture();
			auto current = _exclusive;
			_exclusive.SetRetain(control);
			if (current) current->LostExclusiveMode();
			if (!_exclusive) _window_callback->MouseMove(_window, _window->PointGlobalToClient(Windows::GetWindowSystem()->GetCursorPosition()));
			if (_captured && control && !_captured->IsGeneralizedParent(control)) SetCapture(0);
		}
		Control * ControlSystem::GetExclusiveControl(void) { return _window->IsCaptured() ? _exclusive.Inner() : 0; }
		void ControlSystem::SetTimer(Control * control, uint32 period)
		{
			if (period) {
				int id = -1;
				auto entry = _timers.GetFirst();
				while (entry) {
					if (entry->GetValue().value.Inner() == control) break;
					if (id == -1) {
						auto prev = entry->GetPrevious();
						if (prev) {
							if (entry->GetValue().key - prev->GetValue().key > 1) id = prev->GetValue().key + 1;
						} else {
							if (entry->GetValue().key > 2) id = 2;
						}
					}
					entry = entry->GetNext();
				}
				if (entry) id = entry->GetValue().key; else {
					if (id == -1) {
						auto last = _timers.GetLast();
						id = last ? last->GetValue().key + 1 : 2;
					}
					_timers.Append(id, control);
				}
				_window->SetTimer(id, period);
			} else {
				auto timer = _timers.GetFirst();
				while (timer) {
					if (timer->GetValue().value.Inner() == control) break;
					timer = timer->GetNext();
				}
				if (timer) {
					_window->SetTimer(timer->GetValue().key, 0);
					_timers.BinaryTree::Remove(timer);
				}
			}
		}
		void ControlSystem::Invalidate(void) { _window->InvalidateContents(); }
		void ControlSystem::Render(Graphics::I2DDeviceContext * device)
		{
			auto moment = GetTimerValue();
			device->SetAnimationTime(moment);
			for (int i = _animations.Length() - 1; i >= 0; i--) {
				if (moment - _animations[i].InitialStateTime >= _animations[i].Duration) {
					_animations[i].OnAnimationTick(_animations[i].FinalStateTime);
					_animations[i].OnAnimationEnd();
					_animations.Remove(i);
					if (!_animations.Length()) ResetEffectiveRefreshRate();
				} else _animations[i].OnAnimationTick(moment);
			}
			_root->Render(device, _root->ControlBoundaries);
		}
		void ControlSystem::EnableBeep(bool enable) { _beep = enable; }
		bool ControlSystem::IsBeepEnabled(void) { return _beep; }
		void ControlSystem::Beep(void) { if (_beep) Audio::Beep(); }
		Windows::IWindow * ControlSystem::GetWindow(void) { return _window; }

		ParentControl::ParentControl(void) {}
		Control * ParentControl::FindChild(int ID)
		{
			if (!ID) return 0;
			if (ID == GetID()) return this;
			for (auto & child : __children) { auto result = child.FindChild(ID); if (result) return result; }
			return 0;
		}
		void ParentControl::Render(Graphics::I2DDeviceContext * device, const Box & at)
		{
			for (auto & child : __children) {
				if (!child.IsVisible()) continue;
				auto rect = Box(child.ControlBoundaries.Left + at.Left, child.ControlBoundaries.Top + at.Top, child.ControlBoundaries.Right + at.Left, child.ControlBoundaries.Bottom + at.Top);
				child.Render(device, rect);
			}
		}
		void ParentControl::ArrangeChildren(void)
		{
			auto inner = Box(0, 0, ControlBoundaries.Right - ControlBoundaries.Left, ControlBoundaries.Bottom - ControlBoundaries.Top);
			for (auto & child : __children) {
				auto rect = child.GetRectangle();
				if (rect.IsValid()) child.SetPosition(Box(rect, inner));
			}
			Invalidate();
		}
		void ParentControl::SetPosition(const Box & box) { Control::SetPosition(box); ArrangeChildren(); }
		Control * ParentControl::HitTest(Point at)
		{
			if (!IsEnabled()) return this;
			for (auto & child : __children.InversedElements()) {
				if (!child.IsVisible()) continue;
				auto & box = child.ControlBoundaries;
				if (box.IsInside(at)) return child.HitTest(Point(at.x - box.Left, at.y - box.Top));
			}
			return this;
		}
		string ParentControl::GetControlClass(void) { return L"ParentControl"; }

		RootControl::RootControl(void) : _accelerators(0x10) {}
		void RootControl::Render(Graphics::I2DDeviceContext * device, const Box & at)
		{
			if (_background_template && !_background) {
				ZeroArgumentProvider provider;
				_background = _background_template->Initialize(&provider);
			}
			if (_background) _background->Render(device, at);
			ParentControl::Render(device, at);
		}
		void RootControl::ResetCache(void) { _background.SetReference(0); ParentControl::ResetCache(); }
		void RootControl::RaiseEvent(int ID, ControlEvent event, Control * sender)
		{
			auto callback = GetControlSystem() ? GetControlSystem()->GetCallback() : 0;
			if (callback) callback->HandleControlEvent(GetWindow(), ID, event, sender);
		}
		void RootControl::PopupMenuCancelled(void)
		{
			auto callback = GetControlSystem() ? GetControlSystem()->GetCallback() : 0;
			if (callback) callback->PopupMenuCancelled(GetWindow());
		}
		void RootControl::Timer(void)
		{
			auto callback = GetControlSystem() ? GetControlSystem()->GetCallback() : 0;
			if (callback) callback->Timer(GetWindow());
		}
		string RootControl::GetControlClass(void) { return L"RootControl"; }
		void RootControl::SetBackground(Template::Shape * shape) { _background_template.SetRetain(shape); _background.SetReference(0); Invalidate(); }
		Template::Shape * RootControl::GetBackground(void) { return _background_template; }
		Array<Accelerators::AcceleratorCommand> & RootControl::GetAcceleratorTable(void) { return _accelerators; }
		const Array<Accelerators::AcceleratorCommand> & RootControl::GetAcceleratorTable(void) const { return _accelerators; }
		void RootControl::AddDialogStandardAccelerators(void)
		{
			_accelerators << Accelerators::AcceleratorCommand(1, KeyCodes::Return, false, false, false);
			_accelerators << Accelerators::AcceleratorCommand(2, KeyCodes::Escape, false, false, false);
			_accelerators << Accelerators::AcceleratorCommand(Accelerators::AcceleratorSystemCommand::WindowNextControl, KeyCodes::Tab, false, false, false);
			_accelerators << Accelerators::AcceleratorCommand(Accelerators::AcceleratorSystemCommand::WindowPreviousControl, KeyCodes::Tab, false, true, false);
		}
		bool RootControl::TranslateAccelerators(int key_code)
		{
			for (auto & accel : _accelerators) {
				if (key_code == accel.KeyCode &&
					Keyboard::IsKeyPressed(KeyCodes::Shift) == accel.Shift &&
					Keyboard::IsKeyPressed(KeyCodes::Control) == accel.Control &&
					Keyboard::IsKeyPressed(KeyCodes::Alternative) == accel.Alternative) {
					auto callback = GetControlSystem() ? GetControlSystem()->GetCallback() : 0;
					if (accel.SystemCommand) {
						if (accel.CommandID == static_cast<int>(Accelerators::AcceleratorSystemCommand::WindowClose)) {
							if (callback) callback->WindowClose(GetWindow());
						} else if (accel.CommandID == static_cast<int>(Accelerators::AcceleratorSystemCommand::WindowInvokeHelp)) {
							if (callback) callback->WindowHelp(GetWindow());
						} else if (accel.CommandID == static_cast<int>(Accelerators::AcceleratorSystemCommand::WindowNextControl)) {
							auto focus = GetFocus();
							if (!focus) focus = this;
							auto next = focus->GetNextTabStopControl();
							if (next) next->SetFocus();
						} else if (accel.CommandID == static_cast<int>(Accelerators::AcceleratorSystemCommand::WindowPreviousControl)) {
							auto focus = GetFocus();
							if (!focus) focus = this;
							auto next = focus->GetPreviousTabStopControl();
							if (next) next->SetFocus();
						}
					} else if (callback) callback->HandleControlEvent(GetWindow(), accel.CommandID, ControlEvent::AcceleratorCommand, 0);
					return true;
				}
			}
			return false;
		}

		Windows::IWindow * CreateWindow(const Windows::CreateWindowDesc & desc, Windows::DeviceClass device)
		{
			auto initializer = new ControlSystem::ControlSystemInitializer;
			initializer->WindowDesc = desc;
			initializer->DeviceClass = device;
			initializer->Factory = 0;
			initializer->OuterRectangle = Rectangle::Invalid();
			initializer->FrameTemplate = 0;
			return initializer->CreateWindow();
		}
		Windows::IWindow * CreateWindow(Template::ControlTemplate * base, IEventCallback * callback, const Rectangle & outer, Windows::IWindow * parent, IControlFactory * factory, Windows::DeviceClass device)
		{
			auto initializer = new ControlSystem::ControlSystemInitializer;
			initializer->WindowDesc.Flags = 0;
			initializer->WindowDesc.Position = Box(0, 0, 0, 0);
			initializer->WindowDesc.MinimalConstraints = Point(0, 0);
			initializer->WindowDesc.MaximalConstraints = Point(0, 0);
			initializer->WindowDesc.FrameMargins = Box(0, 0, 0, 0);
			initializer->WindowDesc.Opacity = 1.0;
			initializer->WindowDesc.Theme = Windows::ThemeClass::Light;
			initializer->WindowDesc.BackgroundColor = 0;
			initializer->WindowDesc.Callback = callback;
			initializer->WindowDesc.ParentWindow = parent;
			initializer->WindowDesc.Screen = 0;
			initializer->DeviceClass = device;
			initializer->Factory = factory;
			initializer->OuterRectangle = outer;
			initializer->FrameTemplate = base;
			return initializer->CreateWindow();
		}
		Windows::IWindow * CreateModalWindow(const Windows::CreateWindowDesc & desc, Windows::DeviceClass device)
		{
			auto initializer = new ControlSystem::ControlSystemInitializer;
			initializer->WindowDesc = desc;
			initializer->DeviceClass = device;
			initializer->Factory = 0;
			initializer->OuterRectangle = Rectangle::Invalid();
			initializer->FrameTemplate = 0;
			return initializer->CreateModalWindow();
		}
		Windows::IWindow * CreateModalWindow(Template::ControlTemplate * base, IEventCallback * callback, const Rectangle & outer, Windows::IWindow * parent, IControlFactory * factory, Windows::DeviceClass device)
		{
			auto initializer = new ControlSystem::ControlSystemInitializer;
			initializer->WindowDesc.Flags = 0;
			initializer->WindowDesc.Position = Box(0, 0, 0, 0);
			initializer->WindowDesc.MinimalConstraints = Point(0, 0);
			initializer->WindowDesc.MaximalConstraints = Point(0, 0);
			initializer->WindowDesc.FrameMargins = Box(0, 0, 0, 0);
			initializer->WindowDesc.Opacity = 1.0;
			initializer->WindowDesc.Theme = Windows::ThemeClass::Light;
			initializer->WindowDesc.BackgroundColor = 0;
			initializer->WindowDesc.Callback = callback;
			initializer->WindowDesc.ParentWindow = parent;
			initializer->WindowDesc.Screen = 0;
			initializer->DeviceClass = device;
			initializer->Factory = factory;
			initializer->OuterRectangle = outer;
			initializer->FrameTemplate = base;
			return initializer->CreateModalWindow();
		}
		Control * CreateStandardControl(Template::ControlTemplate * base, IControlFactory * factory)
		{
			// Group controls
			if (base->Properties->GetTemplateClass() == L"ControlGroup") return new Controls::ControlGroup(base, factory);
			else if (base->Properties->GetTemplateClass() == L"RadioButtonGroup") return new Controls::RadioButtonGroup(base);
			else if (base->Properties->GetTemplateClass() == L"ScrollBox") return new Controls::ScrollBox(base, factory);
			else if (base->Properties->GetTemplateClass() == L"VerticalSplitBox") return new Controls::VerticalSplitBox(base, factory);
			else if (base->Properties->GetTemplateClass() == L"HorizontalSplitBox") return new Controls::HorizontalSplitBox(base, factory);
			else if (base->Properties->GetTemplateClass() == L"BookmarkView") return new Controls::BookmarkView(base, factory);
			// Button controls
			else if (base->Properties->GetTemplateClass() == L"Button") return new Controls::Button(base);
			else if (base->Properties->GetTemplateClass() == L"CheckBox") return new Controls::CheckBox(base);
			else if (base->Properties->GetTemplateClass() == L"ToolButton") return new Controls::ToolButton(base);
			// Static controls
			else if (base->Properties->GetTemplateClass() == L"Static") return new Controls::Static(base);
			else if (base->Properties->GetTemplateClass() == L"ColorView") return new Controls::ColorView(base);
			else if (base->Properties->GetTemplateClass() == L"ProgressBar") return new Controls::ProgressBar(base);
			else if (base->Properties->GetTemplateClass() == L"ComplexView") return new Controls::ComplexView(base);
			// Scrollable controls
			else if (base->Properties->GetTemplateClass() == L"VerticalScrollBar") return new Controls::VerticalScrollBar(base);
			else if (base->Properties->GetTemplateClass() == L"HorizontalScrollBar") return new Controls::HorizontalScrollBar(base);
			else if (base->Properties->GetTemplateClass() == L"VerticalTrackBar") return new Controls::VerticalTrackBar(base);
			else if (base->Properties->GetTemplateClass() == L"HorizontalTrackBar") return new Controls::HorizontalTrackBar(base);
			// Edit controls
			else if (base->Properties->GetTemplateClass() == L"Edit") return new Controls::Edit(base);
			else if (base->Properties->GetTemplateClass() == L"MultiLineEdit") return new Controls::MultiLineEdit(base);
			else if (base->Properties->GetTemplateClass() == L"RichEdit") return new Controls::RichEdit(base);
			// List controls
			else if (base->Properties->GetTemplateClass() == L"ListBox") return new Controls::ListBox(base);
			else if (base->Properties->GetTemplateClass() == L"TreeView") return new Controls::TreeView(base);
			else if (base->Properties->GetTemplateClass() == L"ListView") return new Controls::ListView(base);
			// Combined controls
			else if (base->Properties->GetTemplateClass() == L"ComboBox") return new Controls::ComboBox(base);
			else if (base->Properties->GetTemplateClass() == L"TextComboBox") return new Controls::TextComboBox(base);
			// Menu bar control
			else if (base->Properties->GetTemplateClass() == L"MenuBar") return new Controls::MenuBar(base);
			// That's all
			else return 0;
		}
		void CreateChildrenControls(Control * control_on, Template::ControlTemplate * base, IControlFactory * factory)
		{
			for (auto & child : base->Children) {
				SafePointer<Control> control;
				if (factory) {
					control = factory->CreateControl(&child, factory);
					if (!control) control = CreateStandardControl(&child, factory);
				} else control = CreateStandardControl(&child);
				if (control) control_on->AddChild(control);
			}
		}
		Windows::IMenu * CreateMenu(Template::ControlTemplate * base)
		{
			class MenuItemCallback : public Windows::IMenuItemCallback
			{
				SafePointer<Shape> _normal;
				SafePointer<Shape> _normal_checked;
				SafePointer<Shape> _disabled;
				SafePointer<Shape> _disabled_checked;
				SafePointer<Shape> _hot;
				SafePointer<Shape> _hot_checked;
			public:
				SafePointer<Template::ControlTemplate> Template;
				virtual Point MeasureMenuItem(Windows::IMenuItem * item, Graphics::I2DDeviceContext * device) override
				{
					auto props = static_cast<Template::Controls::MenuItem *>(Template->Properties);
					if (!props->Font) return Point(1, 1);
					SafePointer<Graphics::ITextBrush> text = device->CreateTextBrush(props->Font, item->GetText(), 0, 0, 0);
					SafePointer<Graphics::ITextBrush> right = device->CreateTextBrush(props->Font, item->GetSideText(), 0, 0, 0);
					int x1, x2, y;
					text->GetExtents(x1, y);
					right->GetExtents(x2, y);
					return Point(3 * props->Height + x1 + x2, props->Height);
				}
				virtual void RenderMenuItem(Windows::IMenuItem * item, Graphics::I2DDeviceContext * device, const Box & at, bool hot_state) override
				{
					auto props = static_cast<Template::Controls::MenuItem *>(Template->Properties);
					Shape ** _view;
					Template::Shape * View;
					if (item->IsEnabled()) {
						if (hot_state) {
							if (item->IsChecked()) {
								_view = _hot_checked.InnerRef();
								View = props->ViewCheckedHot;
							} else {
								_view = _hot.InnerRef();
								View = props->ViewHot;
							}
						} else {
							if (item->IsChecked()) {
								_view = _normal_checked.InnerRef();
								View = props->ViewCheckedNormal;
							} else {
								_view = _normal.InnerRef();
								View = props->ViewNormal;
							}
						}
					} else {
						if (item->IsChecked()) {
							_view = _disabled_checked.InnerRef();
							View = props->ViewCheckedDisabled;
						} else {
							_view = _disabled.InnerRef();
							View = props->ViewDisabled;
						}
					}
					if (!(*_view)) {
						if (View) {
							auto provider = MenuArgumentProvider(item, props);
							*_view = View->Initialize(&provider);
							(*_view)->Render(device, at);
						}
					} else (*_view)->Render(device, at);
				}
				virtual void MenuClosed(Windows::IMenuItem * item) override { _normal.SetReference(0); _normal_checked.SetReference(0); _hot.SetReference(0); _hot_checked.SetReference(0); _disabled.SetReference(0); _disabled_checked.SetReference(0); }
				virtual void MenuItemDisposed(Windows::IMenuItem * item) override { delete this; }
			};
			class MenuSeparatorCallback : public Windows::IMenuItemCallback
			{
				SafePointer<Shape> _shape;
			public:
				SafePointer<Template::ControlTemplate> Template;
				virtual Point MeasureMenuItem(Windows::IMenuItem * item, Graphics::I2DDeviceContext * device) override { return Point(1, static_cast<Template::Controls::MenuSeparator *>(Template->Properties)->Height); }
				virtual void RenderMenuItem(Windows::IMenuItem * item, Graphics::I2DDeviceContext * device, const Box & at, bool hot_state) override
				{
					if (!_shape) {
						ZeroArgumentProvider provider;
						auto base = static_cast<Template::Controls::MenuSeparator *>(Template->Properties)->View.Inner();
						if (base) _shape = base->Initialize(&provider);
					}
					if (_shape) _shape->Render(device, at);
				}
				virtual void MenuClosed(Windows::IMenuItem * item) override { _shape.SetReference(0); }
				virtual void MenuItemDisposed(Windows::IMenuItem * item) override { delete this; }
			};
			if (base->Properties->GetTemplateClass() != L"PopupMenu" && base->Properties->GetTemplateClass() != L"MenuItem") throw InvalidArgumentException();
			SafePointer<Windows::IMenu> menu = Windows::GetWindowSystem()->CreateMenu();
			for (auto & child : base->Children) {
				if (child.Properties->GetTemplateClass() == L"MenuItem") {
					SafePointer<Windows::IMenuItem> item = Windows::GetWindowSystem()->CreateMenuItem();
					auto props = static_cast<Template::Controls::MenuItem *>(child.Properties);
					item->SetID(props->ID);
					item->Enable(!props->Disabled);
					item->Check(props->Checked);
					item->SetText(props->Text);
					item->SetSideText(props->RightText);
					if (!props->SystemAppearance) {
						auto callback = new MenuItemCallback;
						callback->Template.SetRetain(&child);
						item->SetCallback(callback);
					}
					if (child.Children.Length() || props->HasChildren) {
						SafePointer<Windows::IMenu> submenu = CreateMenu(&child);
						item->SetSubmenu(submenu);
					}
					menu->AppendMenuItem(item);
				} else if (child.Properties->GetTemplateClass() == L"MenuSeparator") {
					SafePointer<Windows::IMenuItem> item = Windows::GetWindowSystem()->CreateMenuItem();
					item->SetIsSeparator(true);
					auto props = static_cast<Template::Controls::MenuSeparator *>(child.Properties);
					if (!props->SystemAppearance) {
						auto callback = new MenuSeparatorCallback;
						callback->Template.SetRetain(&child);
						item->SetCallback(callback);
					}
					menu->AppendMenuItem(item);
				} else throw InvalidArgumentException();
			}
			menu->Retain();
			return menu;
		}
		void RunMenu(Windows::IMenu * menu, Control * for_control, Point at)
		{
			auto system = for_control->GetControlSystem();
			if (system->GetVirtualPopupStyles()) {
				RunVirtualMenu(menu, for_control, at);
			} else {
				auto window = system->GetWindow();
				auto client = system->ConvertClientVirtualToScreen(system->ConvertControlToClient(for_control, at));
				auto global = window->PointClientToGlobal(client);
				auto result = menu->Run(window, global);
				if (result) for_control->RaiseEvent(result, ControlEvent::MenuCommand, 0);
				else for_control->PopupMenuCancelled();
			}
		}
		Box GetPopupMargins(Control * for_control)
		{
			auto system = for_control->GetControlSystem();
			auto abs_pos = for_control->GetAbsolutePosition();
			if (system->GetVirtualPopupStyles() || !system->IsVirtualClientAutoresize()) {
				auto size = system->GetVirtualClientSize();
				return Box(abs_pos.Left, abs_pos.Top, size.x - abs_pos.Right, size.y - abs_pos.Bottom);
			} else {
				auto client_lt = system->ConvertClientVirtualToScreen(Point(abs_pos.Left, abs_pos.Top));
				auto client_rb = system->ConvertClientVirtualToScreen(Point(abs_pos.Right, abs_pos.Bottom));
				auto global_lt = system->GetWindow()->PointClientToGlobal(client_lt);
				auto global_rb = system->GetWindow()->PointClientToGlobal(client_rb);
				SafePointer<Windows::IScreen> screen = system->GetWindow()->GetCurrentScreen();
				auto desktop = screen->GetUserRectangle();
				return Box(global_lt.x - desktop.Left, global_lt.y - desktop.Top, desktop.Right - global_rb.x, desktop.Bottom - global_rb.y);
			}
		}
		Control * CreatePopup(Control * for_control, const Box & at, Windows::DeviceClass device)
		{
			auto system = for_control->GetControlSystem();
			auto abs_pos = for_control->GetAbsolutePosition();
			if (system->GetVirtualPopupStyles()) {
				auto rect = Box(abs_pos.Left + at.Left, abs_pos.Top + at.Top, abs_pos.Left + at.Right, abs_pos.Top + at.Bottom);
				return RunVirtualPopup(system, rect);
			} else {
				auto client = system->ConvertClientVirtualToScreen(Point(abs_pos.Left, abs_pos.Top));
				auto global = system->GetWindow()->PointClientToGlobal(client);
				auto rect = Box(global.x + at.Left, global.y + at.Top, global.x + at.Right, global.y + at.Bottom);
				Windows::CreateWindowDesc desc;
				desc.Flags = Windows::WindowFlagPopup;
				desc.Position = rect;
				desc.MinimalConstraints = desc.MaximalConstraints = Point(0, 0);
				desc.Callback = 0;
				desc.ParentWindow = system->GetWindow();
				desc.Screen = 0;
				auto window = CreateWindow(desc, device);
				return GetRootControl(window);
			}
		}
		void ShowPopup(Control * control, bool show)
		{
			auto system = control->GetControlSystem();
			if (system->GetVirtualPopupStyles()) control->Show(show);
			else control->GetWindow()->Show(show);
		}
		void DestroyPopup(Control * control)
		{
			auto c = control;
			control->SubmitTask(CreateFunctionalTask([c]() {
				auto system = c->GetControlSystem();
				if (system->GetVirtualPopupStyles()) c->RemoveFromParent();
				else c->GetWindow()->Destroy();
			}));
		}
		ControlSystem * GetControlSystem(Windows::IWindow * window) { return &static_cast<ControlSystem::ControlSystemWindowCallback *>(window->GetCallback())->system; }
		Control * FindControl(Windows::IWindow * window, int id) { return static_cast<ControlSystem::ControlSystemWindowCallback *>(window->GetCallback())->system.GetRootControl()->FindChild(id); }
		RootControl * GetRootControl(Windows::IWindow * window) { return static_cast<ControlSystem::ControlSystemWindowCallback *>(window->GetCallback())->system.GetRootControl(); }

		void SelectCursor(Windows::IWindow * window, Windows::SystemCursorClass cursor) { Windows::GetWindowSystem()->SetCursor(GetControlSystem(window)->GetSystemCursor(cursor)); }
	}
}