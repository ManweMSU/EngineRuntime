#include "ListControls.h"

#include "../Interfaces/KeyCodes.h"

namespace Engine
{
	namespace UI
	{
		namespace Controls
		{
			namespace ArgumentService
			{
				class ListBoxSimpleArgumentProvider : public IArgumentProvider
				{
				public:
					ListBox * Owner;
					const string & Text;
					ListBoxSimpleArgumentProvider(ListBox * owner, const string & text) : Owner(owner), Text(text) {}
					virtual void GetArgument(const string & name, int * value) override { *value = 0; }
					virtual void GetArgument(const string & name, double * value) override { *value = 0.0; }
					virtual void GetArgument(const string & name, Color * value) override { *value = 0; }
					virtual void GetArgument(const string & name, string * value) override
					{
						if (name == L"Text") *value = Text;
						else *value = L"";
					}
					virtual void GetArgument(const string & name, Graphics::IBitmap ** value) override { *value = 0; }
					virtual void GetArgument(const string & name, Graphics::IFont ** value) override
					{
						if (name == L"Font" && Owner->Font) {
							*value = Owner->Font;
							(*value)->Retain();
						} else *value = 0;
					}
				};
				class ListBoxWrapperArgumentProvider : public IArgumentProvider
				{
				public:
					ListBox * Owner;
					IArgumentProvider * Inner;
					ListBoxWrapperArgumentProvider(ListBox * owner, IArgumentProvider * inner) : Owner(owner), Inner(inner) {}
					virtual void GetArgument(const string & name, int * value) override { Inner->GetArgument(name, value); }
					virtual void GetArgument(const string & name, double * value) override { Inner->GetArgument(name, value); }
					virtual void GetArgument(const string & name, Color * value) override { Inner->GetArgument(name, value); }
					virtual void GetArgument(const string & name, string * value) override { Inner->GetArgument(name, value); }
					virtual void GetArgument(const string & name, Graphics::IBitmap ** value) override { Inner->GetArgument(name, value); }
					virtual void GetArgument(const string & name, Graphics::IFont ** value) override
					{
						if (name == L"Font") {
							*value = Owner->Font;
							if (Owner->Font) (*value)->Retain();
						} else Inner->GetArgument(name, value);
					}
				};
				class TreeViewSimpleArgumentProvider : public IArgumentProvider
				{
				public:
					TreeView * Owner;
					const string & Text;
					Graphics::IBitmap * ImageNormal;
					Graphics::IBitmap * ImageGrayed;
					TreeViewSimpleArgumentProvider(TreeView * owner, const string & text, Graphics::IBitmap * image_normal, Graphics::IBitmap * image_grayed) : Owner(owner), Text(text), ImageNormal(image_normal), ImageGrayed(image_grayed) {}
					virtual void GetArgument(const string & name, int * value) override { *value = 0; }
					virtual void GetArgument(const string & name, double * value) override { *value = 0.0; }
					virtual void GetArgument(const string & name, Color * value) override { *value = 0; }
					virtual void GetArgument(const string & name, string * value) override
					{
						if (name == L"Text") *value = Text;
						else *value = L"";
					}
					virtual void GetArgument(const string & name, Graphics::IBitmap ** value) override
					{
						if (name == L"ImageNormal" && ImageNormal) {
							*value = ImageNormal;
							ImageNormal->Retain();
						} else if (name == L"ImageGrayed" && ImageGrayed) {
							*value = ImageGrayed;
							ImageGrayed->Retain();
						}
						*value = 0;
					}
					virtual void GetArgument(const string & name, Graphics::IFont ** value) override
					{
						if (name == L"Font" && Owner->Font) {
							*value = Owner->Font;
							(*value)->Retain();
						} else *value = 0;
					}
				};
				class TreeViewWrapperArgumentProvider : public IArgumentProvider
				{
				public:
					TreeView * Owner;
					IArgumentProvider * Inner;
					TreeViewWrapperArgumentProvider(TreeView * owner, IArgumentProvider * inner) : Owner(owner), Inner(inner) {}
					virtual void GetArgument(const string & name, int * value) override { Inner->GetArgument(name, value); }
					virtual void GetArgument(const string & name, double * value) override { Inner->GetArgument(name, value); }
					virtual void GetArgument(const string & name, Color * value) override { Inner->GetArgument(name, value); }
					virtual void GetArgument(const string & name, string * value) override { Inner->GetArgument(name, value); }
					virtual void GetArgument(const string & name, Graphics::IBitmap ** value) override { Inner->GetArgument(name, value); }
					virtual void GetArgument(const string & name, Graphics::IFont ** value) override
					{
						if (name == L"Font") {
							*value = Owner->Font;
							if (Owner->Font) (*value)->Retain();
						} else Inner->GetArgument(name, value);
					}
				};
				class ListViewWrapperArgumentProvider : public IArgumentProvider
				{
				public:
					ListView * Owner;
					IArgumentProvider * Inner;
					ListViewWrapperArgumentProvider(ListView * owner, IArgumentProvider * inner) : Owner(owner), Inner(inner) {}
					virtual void GetArgument(const string & name, int * value) override { Inner->GetArgument(name, value); }
					virtual void GetArgument(const string & name, double * value) override { Inner->GetArgument(name, value); }
					virtual void GetArgument(const string & name, Color * value) override { Inner->GetArgument(name, value); }
					virtual void GetArgument(const string & name, string * value) override { Inner->GetArgument(name, value); }
					virtual void GetArgument(const string & name, Graphics::IBitmap ** value) override { Inner->GetArgument(name, value); }
					virtual void GetArgument(const string & name, Graphics::IFont ** value) override
					{
						if (name == L"Font") {
							*value = Owner->Font;
							if (Owner->Font) (*value)->Retain();
						} else Inner->GetArgument(name, value);
					}
				};
				class ListViewColumnArgumentProvider : public IArgumentProvider
				{
				public:
					ListView * Owner;
					const string & Text;
					ListViewColumnArgumentProvider(ListView * owner, const string & text) : Owner(owner), Text(text) {}
					virtual void GetArgument(const string & name, int * value) override { *value = 0; }
					virtual void GetArgument(const string & name, double * value) override { *value = 0.0; }
					virtual void GetArgument(const string & name, Color * value) override { *value = 0; }
					virtual void GetArgument(const string & name, string * value) override
					{
						if (name == L"Text") *value = Text;
						else *value = L"";
					}
					virtual void GetArgument(const string & name, Graphics::IBitmap ** value) override { *value = 0; }
					virtual void GetArgument(const string & name, Graphics::IFont ** value) override
					{
						if (name == L"Font" && Owner->Font) {
							*value = Owner->Font;
							(*value)->Retain();
						} else *value = 0;
					}
				};
			}
			void ListBox::reset_scroll_ranges(void)
			{
				int page = ControlBoundaries.Bottom - ControlBoundaries.Top - Border - Border;
				int hpage = ControlBoundaries.Right - ControlBoundaries.Left - Border - Border - (_svisible ? ScrollSize : 0);
				int lines_max;
				if (Tiled) {
					int epl = max(hpage / ElementHeight, 1);
					lines_max = (_elements.Length() / epl + ((_elements.Length() % epl) ? 1 : 0)) * ElementHeight - 1;
				} else {
					lines_max = ElementHeight * _elements.Length() - 1;
				}
				_scroll->SetRange(0, lines_max);
				_scroll->Show(_svisible = (lines_max >= page));
			}
			void ListBox::scroll_to_current(void)
			{
				if (_current >= 0) {
					if (Tiled) {
						int hpage = ControlBoundaries.Right - ControlBoundaries.Left - Border - Border - (_svisible ? ScrollSize : 0);
						int epl = max(hpage / ElementHeight, 1);
						int min = ElementHeight * (_current / epl);
						int max = min + ElementHeight;
						if (_scroll->Position > min) _scroll->SetScrollerPosition(min);
						if (_scroll->Position + _scroll->Page < max) _scroll->SetScrollerPosition(max - _scroll->Page);
					} else {
						int min = ElementHeight * _current;
						int max = min + ElementHeight;
						if (_scroll->Position > min) _scroll->SetScrollerPosition(min);
						if (_scroll->Position + _scroll->Page < max) _scroll->SetScrollerPosition(max - _scroll->Page);
					}
				}
			}
			void ListBox::select(int index, bool down)
			{
				if (MultiChoose) {
					if (Keyboard::IsKeyPressed(KeyCodes::Control) && down) {
						if (index != -1) {
							if (_elements[index].Selected) {
								_elements[index].Selected = false;
								if (_current == index) {
									_current = -1;
									for (int i = 0; i < _elements.Length(); i++) if (_elements[i].Selected) { _current = i; break; }
									if (_editor) { _editor->RemoveFromParent(); _editor = 0; }
								}
								Invalidate();
								GetParent()->RaiseEvent(ID, ControlEvent::ValueChange, this);
							} else {
								_elements[index].Selected = true;
								_current = index;
								if (_editor) { _editor->RemoveFromParent(); _editor = 0; }
								scroll_to_current();
								Invalidate();
								GetParent()->RaiseEvent(ID, ControlEvent::ValueChange, this);
							}
						}
					} else if (Keyboard::IsKeyPressed(KeyCodes::Shift) && down) {
						if (index != -1) {
							if (_current == -1) {
								_current = index;
								_elements[_current].Selected = true;
								scroll_to_current();
								Invalidate();
								GetParent()->RaiseEvent(ID, ControlEvent::ValueChange, this);
							} else {
								if (_current != index) {
									for (int i = min(_current, index); i <= max(_current, index); i++) _elements[i].Selected = true;
									_current = index;
									if (_editor) { _editor->RemoveFromParent(); _editor = 0; }
									scroll_to_current();
									Invalidate();
									GetParent()->RaiseEvent(ID, ControlEvent::ValueChange, this);
								}
							}
						}
					} else if (down) {
						if (index != -1) {
							if (_elements[index].Selected) {
								if (_current != index) {
									_current = index;
									if (_editor) { _editor->RemoveFromParent(); _editor = 0; }
									scroll_to_current();
									Invalidate();
									GetParent()->RaiseEvent(ID, ControlEvent::ValueChange, this);
								}
							} else {
								_current = index;
								for (int i = 0; i < _elements.Length(); i++) _elements[i].Selected = i == _current;
								if (_editor) { _editor->RemoveFromParent(); _editor = 0; }
								scroll_to_current();
								Invalidate();
								GetParent()->RaiseEvent(ID, ControlEvent::ValueChange, this);
							}
						}
					}
				} else {
					if (Keyboard::IsKeyPressed(KeyCodes::Control) && down) {
						if (_current == index && index != -1) {
							_elements[_current].Selected = false;
							_current = -1;
							if (_editor) { _editor->RemoveFromParent(); _editor = 0; }
							Invalidate();
							GetParent()->RaiseEvent(ID, ControlEvent::ValueChange, this);
						}
					} else {
						if (_current != index && index != -1) {
							if (_current != -1) _elements[_current].Selected = false;
							_current = index;
							_elements[_current].Selected = true;
							if (_editor) { _editor->RemoveFromParent(); _editor = 0; }
							scroll_to_current();
							Invalidate();
							GetParent()->RaiseEvent(ID, ControlEvent::ValueChange, this);
						}
					}
				}
			}
			void ListBox::move_selection(int to)
			{
				int index = max(min(to, _elements.Length() - 1), 0);
				if (!_elements.Length()) return;
				if (_current != index) {
					if (MultiChoose) {
						if (Keyboard::IsKeyPressed(KeyCodes::Shift) && _current != -1) {
							for (int i = min(_current, index); i <= max(_current, index); i++) _elements[i].Selected = true;
							_current = index;
							if (_editor) { _editor->RemoveFromParent(); _editor = 0; }
							scroll_to_current();
							Invalidate();
							GetParent()->RaiseEvent(ID, ControlEvent::ValueChange, this);
						} else {
							_current = index;
							for (int i = 0; i < _elements.Length(); i++) _elements[i].Selected = i == _current;
							if (_editor) { _editor->RemoveFromParent(); _editor = 0; }
							scroll_to_current();
							Invalidate();
							GetParent()->RaiseEvent(ID, ControlEvent::ValueChange, this);
						}
					} else {
						if (_current != -1) _elements[_current].Selected = false;
						_current = index;
						_elements[_current].Selected = true;
						if (_editor) { _editor->RemoveFromParent(); _editor = 0; }
						scroll_to_current();
						Invalidate();
						GetParent()->RaiseEvent(ID, ControlEvent::ValueChange, this);
					}
				}
			}
			ListBox::ListBox(void) : _elements(0x10)
			{
				ControlPosition = Rectangle::Invalid();
				Reflection::PropertyZeroInitializer Initializer;
				EnumerateProperties(Initializer);
				ResetCache();
			}
			ListBox::ListBox(Template::ControlTemplate * Template) : _elements(0x10)
			{
				if (Template->Properties->GetTemplateClass() != L"ListBox") throw InvalidArgumentException();
				static_cast<Template::Controls::ListBox &>(*this) = static_cast<Template::Controls::ListBox &>(*Template->Properties);
				ResetCache();
			}
			ListBox::~ListBox(void) {}
			void ListBox::Render(Graphics::I2DDeviceContext * device, const Box & at)
			{
				Shape ** back = 0;
				Template::Shape * temp = 0;
				if (Disabled) {
					back = _view_disabled.InnerRef();
					temp = ViewDisabled.Inner();
				} else {
					if (GetFocus() == this) {
						back = _view_focused.InnerRef();
						temp = ViewFocused.Inner();
					} else {
						back = _view_normal.InnerRef();
						temp = ViewNormal.Inner();
					}
				}
				if (*back) {
					(*back)->Render(device, at);
				} else if (temp) {
					ZeroArgumentProvider provider;
					*back = temp->Initialize(&provider);
					(*back)->Render(device, at);
				}
				if (!_view_element_hot && ViewElementHot) {
					ZeroArgumentProvider provider;
					_view_element_hot.SetReference(ViewElementHot->Initialize(&provider));
				}
				if (!_view_element_selected && ViewElementSelected) {
					ZeroArgumentProvider provider;
					_view_element_selected.SetReference(ViewElementSelected->Initialize(&provider));
				}
				Box viewport = Box(at.Left + Border, at.Top + Border, at.Right - Border, at.Bottom - Border);
				device->PushClip(viewport);
				int width = at.Right - at.Left - Border - Border - (_svisible ? ScrollSize : 0);
				int height = at.Bottom - at.Top - Border - Border;
				int from_unclamped = _scroll->Position / ElementHeight;
				int to_unclamped = (_scroll->Position + _scroll->Page) / ElementHeight;
				if (ViewElementAlternateBackground) {
					if (!_view_element_alternate) {
						ZeroArgumentProvider provider;
						_view_element_alternate = ViewElementAlternateBackground->Initialize(&provider);
					}
					for (int i = from_unclamped; i <= to_unclamped; i++) if (i & 1) {
						Box item(at.Left + Border, at.Top + Border + i * ElementHeight - _scroll->Position, at.Left + Border + width, 0);
						item.Bottom = item.Top + ElementHeight;
						_view_element_alternate->Render(device, item);
					}
				}
				if (_elements.Length()) {
					if (Tiled) {
						int epl = max(width / ElementHeight, 1);
						int from = max(min((_scroll->Position / ElementHeight) * epl, _elements.Length() - 1), 0);
						int to = max(min((_scroll->Position + _scroll->Page) / ElementHeight * epl + epl, _elements.Length() - 1), 0);
						if (Disabled) {
							for (int i = from; i <= to; i++) {
								if (i == _current && _editor) continue;
								int x = i % epl, y = i / epl;
								Box item(at.Left + Border + x * ElementHeight, at.Top + Border + y * ElementHeight - _scroll->Position, 0, 0);
								item.Right = item.Left + ElementHeight; item.Bottom = item.Top + ElementHeight;
								_elements[i].ViewDisabled->Render(device, item);
							}
						} else {
							for (int i = from; i <= to; i++) {
								int x = i % epl, y = i / epl;
								Box item(at.Left + Border + x * ElementHeight, at.Top + Border + y * ElementHeight - _scroll->Position, 0, 0);
								item.Right = item.Left + ElementHeight; item.Bottom = item.Top + ElementHeight;
								if (i == _hot && _view_element_hot) {
									_view_element_hot->Render(device, item);
								} else if (_elements[i].Selected && _view_element_selected) {
									_view_element_selected->Render(device, item);
								}
								if (i == _current && _editor) continue;
								_elements[i].ViewNormal->Render(device, item);
							}
						}
					} else {
						int from = max(min(from_unclamped, _elements.Length() - 1), 0);
						int to = max(min(to_unclamped, _elements.Length() - 1), 0);
						if (Disabled) {
							for (int i = from; i <= to; i++) {
								if (i == _current && _editor) continue;
								Box item(at.Left + Border, at.Top + Border + i * ElementHeight - _scroll->Position,
									at.Left + Border + width, 0);
								item.Bottom = item.Top + ElementHeight;
								_elements[i].ViewDisabled->Render(device, item);
							}
						} else {
							for (int i = from; i <= to; i++) {
								Box item(at.Left + Border, at.Top + Border + i * ElementHeight - _scroll->Position,
									at.Left + Border + width, 0);
								item.Bottom = item.Top + ElementHeight;
								if (i == _hot && _view_element_hot) {
									_view_element_hot->Render(device, item);
								} else if (_elements[i].Selected && _view_element_selected) {
									_view_element_selected->Render(device, item);
								}
								if (i == _current && _editor) continue;
								_elements[i].ViewNormal->Render(device, item);
							}
						}
					}
				}
				if (_editor && _editor->IsVisible()) {
					Box pos = _editor->GetPosition();
					Box rect = Box(pos.Left + at.Left, pos.Top + at.Top, pos.Right + at.Left, pos.Bottom + at.Top);
					_editor->Render(device, rect);
				}
				device->PopClip();
				if (_scroll && _svisible) {
					Box pos = _scroll->GetPosition();
					Box rect = Box(pos.Left + at.Left, pos.Top + at.Top, pos.Right + at.Left, pos.Bottom + at.Top);
					_scroll->Render(device, rect);
				}
			}
			void ListBox::ResetCache(void)
			{
				int pos = (_scroll) ? _scroll->Position : 0;
				if (_editor) _editor->ResetCache();
				if (_scroll) _scroll->RemoveFromParent();
				int page = ControlBoundaries.Bottom - ControlBoundaries.Top - Border - Border;
				int hpage = ControlBoundaries.Right - ControlBoundaries.Left - Border - Border - (_svisible ? ScrollSize : 0);
				int lines_max;
				if (Tiled) {
					int epl = max(hpage / ElementHeight, 1);
					lines_max = (_elements.Length() / epl + ((_elements.Length() % epl) ? 1 : 0)) * ElementHeight - 1;
				} else {
					lines_max = ElementHeight * _elements.Length() - 1;
				}
				SafePointer<VerticalScrollBar> vert = new VerticalScrollBar(this);
				_scroll = vert;
				AddChild(vert);
				_scroll->SetRectangle(Rectangle(Coordinate::Right() - ScrollSize - Border, Border, Coordinate::Right() - Border, Coordinate::Bottom() - Border));
				_scroll->Disabled = Disabled;
				_scroll->Line = ElementHeight;
				_scroll->SetPageSilent(page);
				_scroll->SetRangeSilent(0, lines_max);
				_scroll->SetScrollerPositionSilent(pos);
				_scroll->Invisible = !(_svisible = (lines_max >= page));
				for (int i = 0; i < _elements.Length(); i++) {
					_elements[i].ViewNormal->ClearCache();
					_elements[i].ViewDisabled->ClearCache();
				}
				_view_normal.SetReference(0);
				_view_disabled.SetReference(0);
				_view_focused.SetReference(0);
				_view_element_hot.SetReference(0);
				_view_element_selected.SetReference(0);
				_view_element_alternate.SetReference(0);
			}
			void ListBox::ArrangeChildren(void)
			{
				Box inner = Box(0, 0, ControlBoundaries.Right - ControlBoundaries.Left, ControlBoundaries.Bottom - ControlBoundaries.Top);
				if (_scroll) {
					auto rect = _scroll->GetRectangle();
					if (rect.IsValid()) _scroll->SetPosition(Box(rect, inner));
				}
				if (_editor) {
					auto rect = _editor->GetRectangle();
					auto spos = _scroll ? _scroll->Position : 0;
					if (Tiled) {
						int hpage = ControlBoundaries.Right - ControlBoundaries.Left - Border - Border - (_svisible ? ScrollSize : 0);
						int epl = max(hpage / ElementHeight, 1);
						int x = _current % epl, y = _current / epl;
						auto item = Box(Border + x * ElementHeight, Border + y * ElementHeight - spos,
							Border + (x + 1) * ElementHeight, Border + (y + 1) * ElementHeight - spos);
						_editor->SetPosition(Box(rect, item));
					} else {
						auto item = Box(Border, Border + ElementHeight * _current - spos,
							inner.Right - Border - (_svisible ? ScrollSize : 0), Border + ElementHeight * (_current + 1) - spos);
						_editor->SetPosition(Box(rect, item));
					}
				}
			}
			void ListBox::Enable(bool enable) { Disabled = !enable; _hot = -1; if (_scroll) _scroll->Enable(enable); Invalidate(); }
			bool ListBox::IsEnabled(void) { return !Disabled; }
			void ListBox::Show(bool visible) { Invisible = !visible; _hot = -1; Invalidate(); }
			bool ListBox::IsVisible(void) { return !Invisible; }
			bool ListBox::IsTabStop(void) { return true; }
			void ListBox::SetID(int _ID) { ID = _ID; }
			int ListBox::GetID(void) { return ID; }
			void ListBox::SetRectangle(const Rectangle & rect) { ControlPosition = rect; if (GetParent()) GetParent()->ArrangeChildren(); Invalidate(); }
			Rectangle ListBox::GetRectangle(void) { return ControlPosition; }
			void ListBox::SetPosition(const Box & box)
			{
				ControlBoundaries = box;
				int page = ControlBoundaries.Bottom - ControlBoundaries.Top - Border - Border;
				int hpage = ControlBoundaries.Right - ControlBoundaries.Left - Border - Border - (_svisible ? ScrollSize : 0);
				int lines_max;
				if (Tiled) {
					int epl = max(hpage / ElementHeight, 1);
					lines_max = (_elements.Length() / epl + ((_elements.Length() % epl) ? 1 : 0)) * ElementHeight - 1;
				} else {
					lines_max = ElementHeight * _elements.Length() - 1;
				}
				_scroll->SetRange(0, lines_max);
				_scroll->SetPage(page);
				_scroll->Show(_svisible = (lines_max >= page));
				ArrangeChildren();
				Invalidate();
			}
			void ListBox::RaiseEvent(int ID, ControlEvent event, Control * sender)
			{
				if (sender == _scroll) {
					Invalidate();
					if (_editor) ArrangeChildren();
				} else GetParent()->RaiseEvent(ID, event, sender);
			}
			void ListBox::FocusChanged(bool got_focus) { Invalidate(); }
			void ListBox::CaptureChanged(bool got_capture) { if (!got_capture) { _hot = -1; Invalidate(); } }
			void ListBox::LeftButtonDown(Point at) { SetFocus(); select(_hot, true); }
			void ListBox::LeftButtonUp(Point at) { select(_hot, false); }
			void ListBox::LeftButtonDoubleClick(Point at) { if (_hot != -1) GetParent()->RaiseEvent(ID, ControlEvent::DoubleClick, this); }
			void ListBox::RightButtonDown(Point at) { SetFocus(); select(_hot, true); }
			void ListBox::RightButtonUp(Point at) { select(_hot, false); GetParent()->RaiseEvent(ID, ControlEvent::ContextClick, this); }
			void ListBox::MouseMove(Point at)
			{
				Box element(Border, Border, ControlBoundaries.Right - ControlBoundaries.Left - Border - (_svisible ? ScrollSize : 0), ControlBoundaries.Bottom - ControlBoundaries.Top - Border);
				int oh = _hot;
				if (element.IsInside(at) && IsHovered()) {
					int index = (at.y - Border + _scroll->Position) / ElementHeight;
					if (Tiled) {
						int epl = max((element.Right - element.Left) / ElementHeight, 1);
						int x = (at.x - Border) / ElementHeight;
						if (x >= 0 && x < epl) {
							index = index * epl + x;
						} else index = -1;
					}
					if (index < 0 || index >= _elements.Length()) index = -1;
					_hot = index;
				} else {
					_hot = -1;
				}
				if (oh != _hot) Invalidate();
				if (oh == -1 && _hot != -1) SetCapture();
				else if ((oh != -1 || GetCapture() == this) && _hot == -1) ReleaseCapture();
			}
			void ListBox::ScrollVertically(double delta)
			{
				if (_svisible) {
					_scroll->SetScrollerPosition(_scroll->Position + int(delta * double(_scroll->Line)));
				}
			}
			bool ListBox::KeyDown(int key_code)
			{
				if (!_elements.Length()) return true;
				if (Tiled) {
					int page = max((ControlBoundaries.Bottom - ControlBoundaries.Top - Border - Border) / ElementHeight, 1);
					int hpage = ControlBoundaries.Right - ControlBoundaries.Left - Border - Border - (_svisible ? ScrollSize : 0);
					int epl = max(hpage / ElementHeight, 1);
					if (key_code == KeyCodes::Right) {
						if (_current != -1) move_selection(_current + 1); else move_selection(0);
					} else if (key_code == KeyCodes::Left) {
						if (_current != -1) move_selection(_current - 1); else move_selection(0);
					} else if (key_code == KeyCodes::Down) {
						if (_current != -1) move_selection(_current + epl); else move_selection(0);
					} else if (key_code == KeyCodes::Up) {
						if (_current != -1) move_selection(_current - epl); else move_selection(0);
					} else if (key_code == KeyCodes::End) {
						move_selection(_elements.Length() - 1);
					} else if (key_code == KeyCodes::Home) {
						move_selection(0);
					} else if (key_code == KeyCodes::PageDown) {
						if (_current != -1) move_selection(_current + page * epl); else move_selection(0);
					} else if (key_code == KeyCodes::PageUp) {
						if (_current != -1) move_selection(_current - page * epl); else move_selection(0);
					}
				} else {
					int page = max((ControlBoundaries.Bottom - ControlBoundaries.Top - Border - Border) / ElementHeight, 1);
					if (key_code == KeyCodes::Down) {
						if (_current != -1) move_selection(_current + 1); else move_selection(0);
					} else if (key_code == KeyCodes::Up) {
						if (_current != -1) move_selection(_current - 1); else move_selection(0);
					} else if (key_code == KeyCodes::End) {
						move_selection(_elements.Length() - 1);
					} else if (key_code == KeyCodes::Home) {
						move_selection(0);
					} else if (key_code == KeyCodes::PageDown) {
						if (_current != -1) move_selection(_current + page); else move_selection(0);
					} else if (key_code == KeyCodes::PageUp) {
						if (_current != -1) move_selection(_current - page); else move_selection(0);
					}
				}
				return true;
			}
			Control * ListBox::HitTest(Point at)
			{
				if (!IsEnabled()) return this;
				if (_scroll && _svisible) {
					auto box = _scroll->GetPosition();
					if (box.IsInside(at)) return _scroll->HitTest(Point(at.x - box.Left, at.y - box.Top));
				}
				if (_editor && at.x >= Border && at.y >= Border && at.x < ControlBoundaries.Right - ControlBoundaries.Left - Border && at.y < ControlBoundaries.Bottom - ControlBoundaries.Top - Border) {
					if (!_editor->IsVisible()) return this;
					auto box = _editor->GetPosition();
					if (box.IsInside(at)) return _editor->HitTest(Point(at.x - box.Left, at.y - box.Top));
				}
				return this;
			}
			string ListBox::GetControlClass(void) { return L"ListBox"; }
			void ListBox::AddItem(const string & text, void * user) { InsertItem(text, _elements.Length(), user); }
			void ListBox::AddItem(IArgumentProvider * provider, void * user) { InsertItem(provider, _elements.Length(), user); }
			void ListBox::AddItem(Reflection::Reflected & object, void * user) { InsertItem(object, _elements.Length(), user); }
			void ListBox::InsertItem(const string & text, int at, void * user)
			{
				ArgumentService::ListBoxSimpleArgumentProvider provider(this, text);
				_elements.Insert(Element(), at);
				auto & e = _elements[at];
				e.User = user;
				e.ViewNormal.SetReference(ViewElementNormal->Initialize(&provider));
				e.ViewDisabled.SetReference(ViewElementDisabled->Initialize(&provider));
				e.Selected = false;
				reset_scroll_ranges();
				if (_current >= at) {
					_current++;
					if (_editor) ArrangeChildren();
				}
				Invalidate();
			}
			void ListBox::InsertItem(IArgumentProvider * provider, int at, void * user)
			{
				ArgumentService::ListBoxWrapperArgumentProvider wrapper(this, provider);
				_elements.Insert(Element(), at);
				auto & e = _elements[at];
				e.User = user;
				e.ViewNormal.SetReference(ViewElementNormal->Initialize(&wrapper));
				e.ViewDisabled.SetReference(ViewElementDisabled->Initialize(&wrapper));
				e.Selected = false;
				reset_scroll_ranges();
				if (_current >= at) {
					_current++;
					if (_editor) ArrangeChildren();
				}
				Invalidate();
			}
			void ListBox::InsertItem(Reflection::Reflected & object, int at, void * user)
			{
				ReflectorArgumentProvider provider(&object);
				InsertItem(&provider, at, user);
			}
			void ListBox::ResetItem(int index, const string & text)
			{
				ArgumentService::ListBoxSimpleArgumentProvider provider(this, text);
				_elements[index].ViewNormal.SetReference(ViewElementNormal->Initialize(&provider));
				_elements[index].ViewDisabled.SetReference(ViewElementDisabled->Initialize(&provider));
				Invalidate();
			}
			void ListBox::ResetItem(int index, IArgumentProvider * provider)
			{
				ArgumentService::ListBoxWrapperArgumentProvider wrapper(this, provider);
				_elements[index].ViewNormal.SetReference(ViewElementNormal->Initialize(&wrapper));
				_elements[index].ViewDisabled.SetReference(ViewElementDisabled->Initialize(&wrapper));
				Invalidate();
			}
			void ListBox::ResetItem(int index, Reflection::Reflected & object)
			{
				ReflectorArgumentProvider provider(&object);
				ResetItem(index, &provider);
			}
			void ListBox::SwapItems(int i, int j)
			{
				if (_current == i) { _current = j; if (_editor) ArrangeChildren(); }
				else if (_current == j) { _current = i; if (_editor) ArrangeChildren(); }
				_elements.SwapAt(i, j);
				Invalidate();
			}
			void ListBox::RemoveItem(int index)
			{
				_elements.Remove(index);
				reset_scroll_ranges();
				if (_current == index) {
					_current = -1;
					if (_editor) { _editor->RemoveFromParent(); _editor = 0; }
				} else if (_current > index) {
					_current--;
					if (_editor) ArrangeChildren();
				}
				_hot = -1;
				Invalidate();
			}
			void ListBox::ClearItems(void)
			{
				_elements.Clear();
				if (_editor) { _editor->RemoveFromParent(); _editor = 0; }
				_current = -1; _hot = -1;
				reset_scroll_ranges();
				Invalidate();
			}
			int ListBox::ItemCount(void) { return _elements.Length(); }
			void * ListBox::GetItemUserData(int index) { return _elements[index].User; }
			void ListBox::SetItemUserData(int index, void * user) { _elements[index].User = user; }
			int ListBox::GetSelectedIndex(void) { return _current; }
			void ListBox::SetSelectedIndex(int index, bool scroll_to_view)
			{
				if (_current != index && _editor) { _editor->RemoveFromParent(); _editor = 0; }
				if (MultiChoose) {
					_current = index;
					for (int i = 0; i < _elements.Length(); i++) _elements[i].Selected = i == _current;
				} else {
					if (_current != -1) _elements[_current].Selected = false;
					_current = index;
					if (_current != -1) _elements[_current].Selected = true;
				}
				if (scroll_to_view && _current >= 0) scroll_to_current();
				Invalidate();
			}
			bool ListBox::IsItemSelected(int index) { return _elements[index].Selected; }
			void ListBox::SelectItem(int index, bool select)
			{
				if (MultiChoose) {
					_elements[index].Selected = select;
					if (!select && index == _current) {
						_current = -1;
						if (_editor) { _editor->RemoveFromParent(); _editor = 0; }
						for (int i = 0; i < _elements.Length(); i++) if (_elements[i].Selected) { _current = i; break; }
					}
				} else {
					if (select) SetSelectedIndex(index);
					else if (index == _current) SetSelectedIndex(-1);
				}
				Invalidate();
			}
			Control * ListBox::CreateEmbeddedEditor(Template::ControlTemplate * Template, const Rectangle & Position, IControlFactory * Factory)
			{
				CloseEmbeddedEditor();
				if (_current == -1) return 0;
				SafePointer<ControlGroup> group = new ControlGroup;
				AddChild(group);
				try {
					group->ControlPosition = Position;
					CreateChildrenControls(group, Template, Factory);
				} catch (...) { group->RemoveFromParent(); throw; }
				_editor = group;
				ArrangeChildren();
				Invalidate();
				return group;
			}
			Control * ListBox::GetEmbeddedEditor(void) { return _editor; }
			void ListBox::CloseEmbeddedEditor(void) { if (_editor) { auto editor = _editor; _editor = 0; editor->RemoveFromParent(); Invalidate(); } }
			int ListBox::GetScrollerPosition(void) { return _scroll->Position; }
			void ListBox::SetScrollerPosition(int position) { _scroll->SetScrollerPosition(position); }

			int TreeView::TreeViewItem::get_height(void) const { int s = Parent ? 1 : 0; if (Expanded) for (int i = 0; i < Children.Length(); i++) s += Children[i].get_height(); return s; }
			void TreeView::TreeViewItem::reset_cache(void)
			{
				if (ViewNormal) ViewNormal->ClearCache();
				if (ViewDisabled) ViewDisabled->ClearCache();
				for (int i = 0; i < Children.Length(); i++) Children[i].reset_cache();
			}
			int TreeView::TreeViewItem::render(Graphics::I2DDeviceContext * device, int left_offset, int & y, const Box & outer, bool enabled)
			{
				if (Parent) {
					int sy = y;
					if (y + View->ElementHeight > 0) {
						Box content(outer.Left + left_offset + View->ChildOffset, outer.Top + y, outer.Right, outer.Top + y + View->ElementHeight);
						if (enabled) {
							if (View->_hot == this && !View->_is_arrow_hot && View->_view_element_hot) {
								View->_view_element_hot->Render(device, content);
							} else if (View->_current == this && View->_view_element_selected) {
								View->_view_element_selected->Render(device, content);
							}
							if (!View->_editor || View->_current != this) ViewNormal->Render(device, content);
						} else {
							ViewDisabled->Render(device, content);
						}
					}
					y += View->ElementHeight;
					int ry = y;
					if (Expanded) {
						Graphics::IBitmapBrush * line = 0;
						if (enabled) line = View->_line_normal;
						else line = View->_line_disabled;
						int le = 0;
						for (int i = 0; i < Children.Length(); i++) {
							int py = y;
							le = Children[i].render(device, left_offset + View->ChildOffset, y, outer, enabled);
							if (line && py + View->ElementHeight > 0) {
								device->Render(line, Box(outer.Left + left_offset + (View->ChildOffset >> 1),
									outer.Top + py + (View->ElementHeight >> 1),
									outer.Left + left_offset + View->ChildOffset + (Children[i].IsParent() ? 0 : View->ChildOffset),
									outer.Top + py + (View->ElementHeight >> 1) + 1));
							}
						}
						if (line) {
							int s = sy + View->ElementHeight;
							int e = le - (View->ElementHeight >> 1);
							if (s < outer.Bottom && e > 0) {
								device->Render(line, Box(outer.Left + left_offset + (View->ChildOffset >> 1),
									outer.Top + s, outer.Left + left_offset + (View->ChildOffset >> 1) + 1, outer.Top + e));
							}
						}
					}
					if (y + View->ElementHeight > 0 && IsParent()) {
						Box node_arrow(outer.Left + left_offset, outer.Top + sy, outer.Left + left_offset + View->ChildOffset, outer.Top + sy + View->ElementHeight);
						if (Expanded) {
							if (enabled) {
								if (View->_hot == this && View->_is_arrow_hot) {
									if (View->_view_node_expanded_hot) View->_view_node_expanded_hot->Render(device, node_arrow);
								} else {
									if (View->_view_node_expanded_normal) View->_view_node_expanded_normal->Render(device, node_arrow);
								}
							} else {
								if (View->_view_node_expanded_disabled) View->_view_node_expanded_disabled->Render(device, node_arrow);
							}
						} else {
							if (enabled) {
								if (View->_hot == this && View->_is_arrow_hot) {
									if (View->_view_node_collapsed_hot) View->_view_node_collapsed_hot->Render(device, node_arrow);
								} else {
									if (View->_view_node_collapsed_normal) View->_view_node_collapsed_normal->Render(device, node_arrow);
								}
							} else {
								if (View->_view_node_collapsed_disabled) View->_view_node_collapsed_disabled->Render(device, node_arrow);
							}
						}
					}
					return ry;
				} else {
					for (int i = 0; i < Children.Length(); i++) {
						if (y > outer.Bottom) break;
						Children[i].render(device, 0, y, outer, enabled);
					}
				}
				return 0;
			}
			bool TreeView::TreeViewItem::is_generalized_children(TreeViewItem * node) const
			{
				auto current = this;
				while (current) {
					if (current == node) return true;
					current = current->Parent;
				}
				return false;
			}
			TreeView::TreeViewItem * TreeView::TreeViewItem::hit_test(const Point & p, const Box & outer, int left_offset, int & y, bool & node)
			{
				if (Parent) {
					int sy = y;
					if (Box(outer.Left + left_offset + View->ChildOffset, outer.Top + y,
						outer.Right, outer.Top + y + View->ElementHeight).IsInside(p)) { node = false; return this; }
					y += View->ElementHeight;
					if (IsParent()) {
						if (Box(outer.Left + left_offset, outer.Top + sy, outer.Left + left_offset + View->ChildOffset,
							outer.Top + sy + View->ElementHeight).IsInside(p)) { node = true; return this; }
					}
					if (Expanded) for (int i = 0; i < Children.Length(); i++) {
						auto result = Children[i].hit_test(p, outer, left_offset + View->ChildOffset, y, node);
						if (result) return result;
					}
				} else {
					for (int i = 0; i < Children.Length(); i++) {
						auto result = Children[i].hit_test(p, outer, left_offset, y, node);
						if (result) return result;
					}
				}
				return 0;
			}
			TreeView::TreeViewItem * TreeView::TreeViewItem::get_topmost(void) { if (Parent) return this; else if (Children.Length()) return &Children[0]; else return 0; }
			TreeView::TreeViewItem * TreeView::TreeViewItem::get_bottommost(void)
			{
				if (Expanded && IsParent()) return Children.LastElement().get_bottommost();
				else if (Parent) return this; else return 0;
			}
			TreeView::TreeViewItem * TreeView::TreeViewItem::get_previous(void)
			{
				int index = GetIndexAtParent();
				if (index) return Parent->Children[index - 1].get_bottommost();
				if (Parent->Parent) return Parent; else return this;
			}
			TreeView::TreeViewItem * TreeView::TreeViewItem::get_next(void)
			{
				if (IsParent() && Expanded) return &Children[0];
				int index;
				auto node = this;
				do {
					index = node->GetIndexAtParent();
					node = node->Parent;
				} while (node && node->Children.Length() == index + 1);
				if (node) {
					return &node->Children[index + 1];
				} else return this;
			}
			TreeView::TreeViewItem::TreeViewItem(TreeView * view, TreeViewItem * parent) : Children(0x10), View(view), Parent(parent), Expanded(false), User(0) {}
			TreeView::TreeViewItem::TreeViewItem(void) : View(0), Parent(0) {}
			TreeView::TreeViewItem::~TreeViewItem(void) {}
			bool TreeView::TreeViewItem::IsExpanded(void) const { return Expanded; }
			bool TreeView::TreeViewItem::IsParent(void) const { return Children.Length() != 0; }
			bool TreeView::TreeViewItem::IsAccessible(void) const
			{
				auto current = this->Parent;
				while (current->Parent) {
					if (!current->Expanded) return false;
					current = current->Parent;
				}
				return true;
			}
			void TreeView::TreeViewItem::Expand(bool expand)
			{
				if (!Parent && Expanded == expand) return;
				Expanded = expand;
				if (View->_hot != this) View->_hot = 0;
				View->reset_scroll_ranges();
				if (!expand && View->_editor && View->_current && !View->_current->IsAccessible()) {
					View->_editor->RemoveFromParent(); View->_editor = 0;
				}
				if (View->_editor) View->ArrangeChildren();
				View->Invalidate();
			}
			Box TreeView::TreeViewItem::GetBounds(void) const
			{
				int depth = -1;
				int shift = -1;
				auto current = this;
				while (current->Parent) {
					depth++;
					shift++;
					int index = current->GetIndexAtParent();
					for (int i = 0; i < index; i++) shift += current->Parent->Children[i].get_height();
					current = current->Parent;
				}
				auto box = Box(View->Border + (depth + 1) * View->ChildOffset,
					View->Border - View->_scroll->Position + shift * View->ElementHeight,
					View->ControlBoundaries.Right - View->ControlBoundaries.Left - View->Border - (View->_svisible ? View->ScrollSize : 0), 0);
				box.Bottom = box.Top + View->ElementHeight;
				return box;
			}
			TreeView * TreeView::TreeViewItem::GetView(void) const { return View; }
			TreeView::TreeViewItem * TreeView::TreeViewItem::GetParent(void) const { return Parent; }
			int TreeView::TreeViewItem::GetIndexAtParent(void) const
			{
				if (!Parent) return -1;
				for (int i = 0; i < Parent->Children.Length(); i++) if (&Parent->Children[i] == this) return i;
				return -1;
			}
			int TreeView::TreeViewItem::GetChildrenCount(void) const { return Children.Length(); }
			const TreeView::TreeViewItem * TreeView::TreeViewItem::GetChild(int index) const { return &Children[index]; }
			TreeView::TreeViewItem * TreeView::TreeViewItem::GetChild(int index) { return &Children[index]; }
			TreeView::TreeViewItem * TreeView::TreeViewItem::AddItem(const string & text, void * user) { return InsertItem(text, Children.Length(), user); }
			TreeView::TreeViewItem * TreeView::TreeViewItem::AddItem(const string & text, Graphics::IBitmap * image_normal, Graphics::IBitmap * image_grayed, void * user) { return InsertItem(text, image_normal, image_grayed, Children.Length(), user); }
			TreeView::TreeViewItem * TreeView::TreeViewItem::AddItem(IArgumentProvider * provider, void * user) { return InsertItem(provider, Children.Length(), user); }
			TreeView::TreeViewItem * TreeView::TreeViewItem::AddItem(Reflection::Reflected & object, void * user) { return InsertItem(object, Children.Length(), user); }
			TreeView::TreeViewItem * TreeView::TreeViewItem::InsertItem(const string & text, int at, void * user)
			{
				TreeViewItem item(View, this);
				item.User = user;
				item.Reset(text);
				Children.Insert(item, at);
				View->reset_scroll_ranges();
				if (View->_editor) View->ArrangeChildren();
				View->Invalidate();
				return &Children[at];
			}
			TreeView::TreeViewItem * TreeView::TreeViewItem::InsertItem(const string & text, Graphics::IBitmap * image_normal, Graphics::IBitmap * image_grayed, int at, void * user)
			{
				TreeViewItem item(View, this);
				item.User = user;
				item.Reset(text, image_normal, image_grayed);
				Children.Insert(item, at);
				View->reset_scroll_ranges();
				if (View->_editor) View->ArrangeChildren();
				View->Invalidate();
				return &Children[at];
			}
			TreeView::TreeViewItem * TreeView::TreeViewItem::InsertItem(IArgumentProvider * provider, int at, void * user)
			{
				TreeViewItem item(View, this);
				item.User = user;
				item.Reset(provider);
				Children.Insert(item, at);
				View->reset_scroll_ranges();
				if (View->_editor) View->ArrangeChildren();
				View->Invalidate();
				return &Children[at];
			}
			TreeView::TreeViewItem * TreeView::TreeViewItem::InsertItem(Reflection::Reflected & object, int at, void * user)
			{
				TreeViewItem item(View, this);
				item.User = user;
				item.Reset(object);
				Children.Insert(item, at);
				View->reset_scroll_ranges();
				if (View->_editor) View->ArrangeChildren();
				View->Invalidate();
				return &Children[at];
			}
			void TreeView::TreeViewItem::Reset(const string & text)
			{
				ArgumentService::TreeViewSimpleArgumentProvider provider(View, text, 0, 0);
				ViewNormal.SetReference(View->ViewElementNormal->Initialize(&provider));
				ViewDisabled.SetReference(View->ViewElementDisabled->Initialize(&provider));
				View->Invalidate();
			}
			void TreeView::TreeViewItem::Reset(const string & text, Graphics::IBitmap * image_normal, Graphics::IBitmap * image_grayed)
			{
				ArgumentService::TreeViewSimpleArgumentProvider provider(View, text, image_normal, image_grayed);
				ViewNormal.SetReference(View->ViewElementNormal->Initialize(&provider));
				ViewDisabled.SetReference(View->ViewElementDisabled->Initialize(&provider));
				View->Invalidate();
			}
			void TreeView::TreeViewItem::Reset(IArgumentProvider * provider)
			{
				ArgumentService::TreeViewWrapperArgumentProvider wrapper(View, provider);
				ViewNormal.SetReference(View->ViewElementNormal->Initialize(&wrapper));
				ViewDisabled.SetReference(View->ViewElementDisabled->Initialize(&wrapper));
				View->Invalidate();
			}
			void TreeView::TreeViewItem::Reset(Reflection::Reflected & object) { ReflectorArgumentProvider provider(&object); Reset(&provider); }
			void TreeView::TreeViewItem::SwapItems(int i, int j)
			{
				Children.SwapAt(i, j);
				View->_hot = 0;
				View->_is_arrow_hot = false;
				if (View->_editor) View->ArrangeChildren();
				View->Invalidate();
			}
			void TreeView::TreeViewItem::Remove(void)
			{
				if (!Parent) return;
				if (View->_current->is_generalized_children(this)) {
					if (View->_editor) { View->_editor->RemoveFromParent(); View->_editor = 0; }
					View->_current = 0;
				}
				int at = GetIndexAtParent();
				auto view = View;
				if (at != -1) Parent->Children.Remove(at);
				view->_hot = 0;
				view->_is_arrow_hot = false;
				view->reset_scroll_ranges();
				if (view->_editor) view->ArrangeChildren();
				view->Invalidate();
			}

			void TreeView::generate_line_textures(void)
			{
				if (BranchColorNormal.a) {
					_line_texture_normal = new Codec::Frame(8, 8, -1, Codec::PixelFormat::R8G8B8A8, Codec::AlphaMode::Straight, Codec::ScanOrigin::TopDown);
					for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) {
						if ((x + y) & 1) _line_texture_normal->SetPixel(x, y, BranchColorNormal); else _line_texture_normal->SetPixel(x, y, 0);
					}
				} else _line_texture_normal.SetReference(0);
				if (BranchColorDisabled.a) {
					_line_texture_disabled = new Codec::Frame(8, 8, -1, Codec::PixelFormat::R8G8B8A8, Codec::AlphaMode::Straight, Codec::ScanOrigin::TopDown);
					for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) {
						if ((x + y) & 1) _line_texture_disabled->SetPixel(x, y, BranchColorDisabled); else _line_texture_disabled->SetPixel(x, y, 0);
					}
				} else _line_texture_disabled.SetReference(0);
			}
			void TreeView::reset_scroll_ranges(void)
			{
				int page = ControlBoundaries.Bottom - ControlBoundaries.Top - Border - Border;
				int lines_max = ElementHeight * _root.get_height() - 1;
				_scroll->SetRange(0, lines_max);
				_scroll->Show(_svisible = (lines_max >= page));
			}
			void TreeView::scroll_to_current(void)
			{
				if (!_current) return;
				bool reset = false;
				auto current = _current->Parent;
				while (current->Parent) { if (!current->Expanded) { current->Expanded = true; reset = true; } current = current->Parent; }
				if (reset) reset_scroll_ranges();
				auto bounds = _current->GetBounds();
				int page = ControlBoundaries.Bottom - ControlBoundaries.Top;
				if (bounds.Top < Border) _scroll->SetScrollerPosition(_scroll->Position - (Border - bounds.Top));
				else if (bounds.Bottom > page - Border) _scroll->SetScrollerPosition(_scroll->Position + (bounds.Bottom - page + Border));
			}
			void TreeView::select(TreeViewItem * item)
			{
				if (Keyboard::IsKeyPressed(KeyCodes::Control)) {
					if (_current == item && item) {
						_current = 0;
						if (_editor) { _editor->RemoveFromParent(); _editor = 0; }
						Invalidate();
						GetParent()->RaiseEvent(ID, ControlEvent::ValueChange, this);
					}
				} else {
					if (_current != item && item) {
						_current = item;
						if (_editor) { _editor->RemoveFromParent(); _editor = 0; }
						scroll_to_current();
						Invalidate();
						GetParent()->RaiseEvent(ID, ControlEvent::ValueChange, this);
					}
				}
			}
			void TreeView::move_selection(TreeViewItem * to)
			{
				if (to && _current != to) {
					_current = to;
					if (_editor) { _editor->RemoveFromParent(); _editor = 0; }
					scroll_to_current();
					Invalidate();
					GetParent()->RaiseEvent(ID, ControlEvent::ValueChange, this);
				}
			}
			TreeView::TreeView(void) : _root(this, 0)
			{
				_root.Expanded = true;
				ControlPosition = Rectangle::Invalid();
				Reflection::PropertyZeroInitializer Initializer;
				EnumerateProperties(Initializer);
				ResetCache();
			}
			TreeView::TreeView(Template::ControlTemplate * Template) : _root(this, 0)
			{
				_root.Expanded = true;
				if (Template->Properties->GetTemplateClass() != L"TreeView") throw InvalidArgumentException();
				static_cast<Template::Controls::TreeView &>(*this) = static_cast<Template::Controls::TreeView &>(*Template->Properties);
				ResetCache();
			}
			TreeView::~TreeView(void) {}
			void TreeView::Render(Graphics::I2DDeviceContext * device, const Box & at)
			{
				Shape ** back = 0;
				Template::Shape * temp = 0;
				if (Disabled) {
					back = _view_disabled.InnerRef();
					temp = ViewDisabled.Inner();
				} else {
					if (GetFocus() == this) {
						back = _view_focused.InnerRef();
						temp = ViewFocused.Inner();
					} else {
						back = _view_normal.InnerRef();
						temp = ViewNormal.Inner();
					}
				}
				if (*back) {
					(*back)->Render(device, at);
				} else if (temp) {
					ZeroArgumentProvider provider;
					*back = temp->Initialize(&provider);
					(*back)->Render(device, at);
				}
				if (!_view_element_hot && ViewElementHot) {
					ZeroArgumentProvider provider;
					_view_element_hot.SetReference(ViewElementHot->Initialize(&provider));
				}
				if (!_view_element_selected && ViewElementSelected) {
					ZeroArgumentProvider provider;
					_view_element_selected.SetReference(ViewElementSelected->Initialize(&provider));
				}
				if (!_view_node_collapsed_disabled && ViewNodeCollapsedDisabled) {
					ZeroArgumentProvider provider;
					_view_node_collapsed_disabled.SetReference(ViewNodeCollapsedDisabled->Initialize(&provider));
				}
				if (!_view_node_collapsed_normal && ViewNodeCollapsedNormal) {
					ZeroArgumentProvider provider;
					_view_node_collapsed_normal.SetReference(ViewNodeCollapsedNormal->Initialize(&provider));
				}
				if (!_view_node_collapsed_hot && ViewNodeCollapsedHot) {
					ZeroArgumentProvider provider;
					_view_node_collapsed_hot.SetReference(ViewNodeCollapsedHot->Initialize(&provider));
				}
				if (!_view_node_expanded_disabled && ViewNodeOpenedDisabled) {
					ZeroArgumentProvider provider;
					_view_node_expanded_disabled.SetReference(ViewNodeOpenedDisabled->Initialize(&provider));
				}
				if (!_view_node_expanded_normal && ViewNodeOpenedNormal) {
					ZeroArgumentProvider provider;
					_view_node_expanded_normal.SetReference(ViewNodeOpenedNormal->Initialize(&provider));
				}
				if (!_view_node_expanded_hot && ViewNodeOpenedHot) {
					ZeroArgumentProvider provider;
					_view_node_expanded_hot.SetReference(ViewNodeOpenedHot->Initialize(&provider));
				}
				if (!_line_normal && _line_texture_normal) {
					SafePointer<Graphics::IBitmap> texture = device->GetParentFactory()->LoadBitmap(_line_texture_normal);
					_line_normal.SetReference(device->CreateBitmapBrush(texture, Box(0, 0, 8, 8), true));
				}
				if (!_line_disabled && _line_texture_disabled) {
					SafePointer<Graphics::IBitmap> texture = device->GetParentFactory()->LoadBitmap(_line_texture_disabled);
					_line_disabled.SetReference(device->CreateBitmapBrush(texture, Box(0, 0, 8, 8), true));
				}
				Box viewport = Box(at.Left + Border, at.Top + Border, at.Right - Border - (_svisible ? ScrollSize : 0), at.Bottom - Border);
				device->PushClip(viewport);
				int width = at.Right - at.Left - Border - Border - (_svisible ? ScrollSize : 0);
				int height = at.Bottom - at.Top - Border - Border;
				int y = -_scroll->Position;
				if (ViewElementAlternateBackground) {
					int from = _scroll->Position / ElementHeight;
					int to = (_scroll->Position + _scroll->Page) / ElementHeight;
					if (!_view_element_alternate) {
						ZeroArgumentProvider provider;
						_view_element_alternate = ViewElementAlternateBackground->Initialize(&provider);
					}
					for (int i = from; i <= to; i++) if (i & 1) {
						Box item(at.Left + Border, at.Top + Border + i * ElementHeight - _scroll->Position, at.Left + Border + width, 0);
						item.Bottom = item.Top + ElementHeight;
						_view_element_alternate->Render(device, item);
					}
				}
				_root.render(device, 0, y, viewport, !Disabled);
				if (_editor && _editor->IsVisible()) {
					Box pos = _editor->GetPosition();
					Box rect = Box(pos.Left + at.Left, pos.Top + at.Top, pos.Right + at.Left, pos.Bottom + at.Top);
					_editor->Render(device, rect);
				}
				device->PopClip();
				if (_scroll && _svisible) {
					Box pos = _scroll->GetPosition();
					Box rect = Box(pos.Left + at.Left, pos.Top + at.Top, pos.Right + at.Left, pos.Bottom + at.Top);
					_scroll->Render(device, rect);
				}
			}
			void TreeView::ResetCache(void)
			{
				generate_line_textures();
				int pos = (_scroll) ? _scroll->Position : 0;
				if (_editor) _editor->ResetCache();
				if (_scroll) _scroll->RemoveFromParent();
				int page = ControlBoundaries.Bottom - ControlBoundaries.Top - Border - Border;
				int lines_max;
				lines_max = ElementHeight * _root.get_height() - 1;
				SafePointer<VerticalScrollBar> vert = new VerticalScrollBar(this);
				_scroll = vert;
				AddChild(vert);
				_scroll->SetRectangle(Rectangle(Coordinate::Right() - ScrollSize - Border, Border,
					Coordinate::Right() - Border, Coordinate::Bottom() - Border));
				_scroll->Disabled = Disabled;
				_scroll->Line = ElementHeight;
				_scroll->SetPageSilent(page);
				_scroll->SetRangeSilent(0, lines_max);
				_scroll->SetScrollerPositionSilent(pos);
				_scroll->Invisible = !(_svisible = (lines_max >= page));
				_root.reset_cache();
				_view_normal.SetReference(0);
				_view_disabled.SetReference(0);
				_view_focused.SetReference(0);
				_view_element_hot.SetReference(0);
				_view_element_selected.SetReference(0);
				_view_element_alternate.SetReference(0);
				_view_node_collapsed_normal.SetReference(0);
				_view_node_collapsed_disabled.SetReference(0);
				_view_node_collapsed_hot.SetReference(0);
				_view_node_expanded_normal.SetReference(0);
				_view_node_expanded_disabled.SetReference(0);
				_view_node_expanded_hot.SetReference(0);
				_line_normal.SetReference(0);
				_line_disabled.SetReference(0);
			}
			void TreeView::ArrangeChildren(void)
			{
				Box inner = Box(0, 0, ControlBoundaries.Right - ControlBoundaries.Left, ControlBoundaries.Bottom - ControlBoundaries.Top);
				if (_scroll) {
					auto rect = _scroll->GetRectangle();
					if (rect.IsValid()) _scroll->SetPosition(Box(rect, inner));
				}
				if (_editor) {
					auto rect = _editor->GetRectangle();
					_editor->SetPosition(Box(rect, _current->GetBounds()));
				}
			}
			void TreeView::Enable(bool enable) { Disabled = !enable; _hot = 0; _is_arrow_hot = false; if (_scroll) _scroll->Enable(enable); Invalidate(); }
			bool TreeView::IsEnabled(void) { return !Disabled; }
			void TreeView::Show(bool visible) { Invisible = !visible; _hot = 0; _is_arrow_hot = false; Invalidate(); }
			bool TreeView::IsVisible(void) { return !Invisible; }
			bool TreeView::IsTabStop(void) { return true; }
			void TreeView::SetID(int _ID) { ID = _ID; }
			int TreeView::GetID(void) { return ID; }
			void TreeView::SetRectangle(const Rectangle & rect) { ControlPosition = rect; if (GetParent()) GetParent()->ArrangeChildren(); Invalidate(); }
			Rectangle TreeView::GetRectangle(void) { return ControlPosition; }
			void TreeView::SetPosition(const Box & box)
			{
				ControlBoundaries = box;
				int page = ControlBoundaries.Bottom - ControlBoundaries.Top - Border - Border;
				int lines_max = ElementHeight * _root.get_height() - 1;
				_scroll->SetRange(0, lines_max);
				_scroll->SetPage(page);
				_scroll->Show(_svisible = (lines_max >= page));
				ArrangeChildren();
				Invalidate();
			}
			void TreeView::RaiseEvent(int ID, ControlEvent event, Control * sender)
			{
				if (sender == _scroll) {
					Invalidate();
					if (_editor) ArrangeChildren();
				} else GetParent()->RaiseEvent(ID, event, sender);
			}
			void TreeView::FocusChanged(bool got_focus) { Invalidate(); }
			void TreeView::CaptureChanged(bool got_capture) { if (!got_capture) { _hot = 0; _is_arrow_hot = false; Invalidate(); } }
			void TreeView::LeftButtonDown(Point at)
			{
				SetFocus();
				if (_hot && _is_arrow_hot) {
					_hot->Expand(!_hot->IsExpanded());
				} else {
					select(_hot);
				}
			}
			void TreeView::LeftButtonDoubleClick(Point at) { if (!_is_arrow_hot && _hot) GetParent()->RaiseEvent(ID, ControlEvent::DoubleClick, this); }
			void TreeView::RightButtonDown(Point at) { if (!_is_arrow_hot) { SetFocus(); select(_hot); } }
			void TreeView::RightButtonUp(Point at) { if (!_is_arrow_hot && _hot) GetParent()->RaiseEvent(ID, ControlEvent::ContextClick, this); }
			void TreeView::MouseMove(Point at)
			{
				Box element(Border, Border, ControlBoundaries.Right - ControlBoundaries.Left - Border - (_svisible ? ScrollSize : 0), ControlBoundaries.Bottom - ControlBoundaries.Top - Border);
				TreeViewItem * oh = _hot;
				bool ohn = _is_arrow_hot;
				if (element.IsInside(at) && IsHovered()) {
					int y = -_scroll->Position;
					bool is_arrow = false;
					auto item = _root.hit_test(at, element, 0, y, is_arrow);
					_hot = item;
					_is_arrow_hot = is_arrow;

				} else _hot = 0;
				if (oh != _hot) Invalidate();
				if (oh == 0 && _hot != 0) SetCapture();
				else if ((oh != 0 || GetCapture() == this) && _hot == 0) ReleaseCapture();
			}
			void TreeView::ScrollVertically(double delta)
			{
				if (_svisible) {
					_scroll->SetScrollerPosition(_scroll->Position + int(delta * double(_scroll->Line)));
				}
			}
			bool TreeView::KeyDown(int key_code)
			{
				if (key_code == KeyCodes::Down) {
					if (_current) move_selection(_current->get_next());
					else move_selection(_root.get_topmost());
				} else if (key_code == KeyCodes::Up) {
					if (_current) move_selection(_current->get_previous());
					else move_selection(_root.get_topmost());
				} else if (key_code == KeyCodes::Left) {
					if (_current && _current->IsParent()) _current->Expand(false);
				} else if (key_code == KeyCodes::Right) {
					if (_current && _current->IsParent()) _current->Expand(true);
				} else if (key_code == KeyCodes::End) {
					move_selection(_root.get_bottommost());
				} else if (key_code == KeyCodes::Home) {
					move_selection(_root.get_topmost());
				} else if (key_code == KeyCodes::PageUp) {
					if (_current) {
						int page = max((ControlBoundaries.Bottom - ControlBoundaries.Top - Border - Border) / ElementHeight, 1);
						auto current = _current;
						for (int i = 0; i < page; i++) current = current->get_previous();
						move_selection(current);
					} else move_selection(_root.get_topmost());
				} else if (key_code == KeyCodes::PageDown) {
					if (_current) {
						int page = max((ControlBoundaries.Bottom - ControlBoundaries.Top - Border - Border) / ElementHeight, 1);
						auto current = _current;
						for (int i = 0; i < page; i++) current = current->get_next();
						move_selection(current);
					} else move_selection(_root.get_topmost());
				}
				return true;
			}
			Control * TreeView::HitTest(Point at)
			{
				if (!IsEnabled()) return this;
				if (_scroll && _svisible) {
					auto box = _scroll->GetPosition();
					if (box.IsInside(at)) return _scroll->HitTest(Point(at.x - box.Left, at.y - box.Top));
				}
				if (_editor && at.x >= Border && at.y >= Border && at.x < ControlBoundaries.Right - ControlBoundaries.Left - Border && at.y < ControlBoundaries.Bottom - ControlBoundaries.Top - Border) {
					if (!_editor->IsVisible()) return this;
					auto box = _editor->GetPosition();
					if (box.IsInside(at)) return _editor->HitTest(Point(at.x - box.Left, at.y - box.Top));
				}
				return this;
			}
			string TreeView::GetControlClass(void) { return L"TreeView"; }
			TreeView::TreeViewItem * TreeView::GetRootItem(void) { return &_root; }
			void TreeView::ClearItems(void)
			{
				_root.Children.Clear();
				if (_editor) { _editor->RemoveFromParent(); _editor = 0; }
				_current = 0; _hot = 0; _is_arrow_hot = false;
				reset_scroll_ranges();
				Invalidate();
			}
			TreeView::TreeViewItem * TreeView::GetSelectedItem(void) { return _current; }
			void TreeView::SetSelectedItem(TreeViewItem * item, bool scroll_to_view)
			{
				if (!item || item->View == this) {
					if (item && !item->Parent) item = 0;
					if (_editor && _current != item) { _editor->RemoveFromParent(); _editor = 0; }
					_current = item;
				}
				if (scroll_to_view && _current) scroll_to_current();
				Invalidate();
			}
			Control * TreeView::CreateEmbeddedEditor(Template::ControlTemplate * Template, const Rectangle & Position, IControlFactory * Factory)
			{
				CloseEmbeddedEditor();
				if (!_current || !_current->IsAccessible()) return 0;
				SafePointer<ControlGroup> group = new ControlGroup;
				AddChild(group);
				try {
					group->ControlPosition = Position;
					CreateChildrenControls(group, Template, Factory);
				} catch (...) { group->RemoveFromParent(); throw; }
				_editor = group;
				ArrangeChildren();
				Invalidate();
				return group;
			}
			Control * TreeView::GetEmbeddedEditor(void) { return _editor; }
			void TreeView::CloseEmbeddedEditor(void) { if (_editor) { auto editor = _editor; _editor = 0; editor->RemoveFromParent(); Invalidate(); } }
			int TreeView::GetScrollerPosition(void) { return _scroll->Position; }
			void TreeView::SetScrollerPosition(int position) { _scroll->SetScrollerPosition(position); }

			ListView::Element::Element(void) : ViewNormal(0x10), ViewDisabled(0x10) {}
			ListView::Element::Element(ListView * view) : ViewNormal(view->_columns.Length()), ViewDisabled(view->_columns.Length()) {}
			ListView::ListViewColumn::ListViewColumn(int index) : _index(index), _abs_index(index) {}
			ListView::ListViewColumn::ListViewColumn(void) {}

			void ListView::reset_scroll_ranges(void)
			{
				int vpage = ControlBoundaries.Bottom - ControlBoundaries.Top - Border - Border - HeaderHeight;
				int hpage = ControlBoundaries.Right - ControlBoundaries.Left - Border - Border;
				int vspace = ElementHeight * _elements.Length();
				int hspace = (_col_reorder.Length()) ? (_col_reorder.LastElement()->_position_limit + _col_reorder.LastElement()->_width) : 0;
				bool ovv = _vsvisible;
				bool ohv = _hsvisible;
				_hsvisible = (hspace > hpage || (hspace > hpage - VerticalScrollSize && vspace > vpage));
				_vsvisible = (vspace > vpage || (vspace > vpage - HorizontalScrollSize && hspace > hpage));
				_vscroll->SetRange(0, vspace - 1);
				_hscroll->SetRange(0, hspace - 1);
				if (ovv != _vsvisible || ohv != _hsvisible) {
					if (_vsvisible) hpage -= VerticalScrollSize;
					if (_hsvisible) vpage -= HorizontalScrollSize;
					_vscroll->SetPage(vpage);
					_hscroll->SetPage(hpage);
					_vscroll->SetRectangle(Rectangle(Coordinate::Right() - VerticalScrollSize - Border, Border,
						Coordinate::Right() - Border, Coordinate::Bottom() - Border - (_hsvisible ? HorizontalScrollSize : 0)));
					_hscroll->SetRectangle(Rectangle(Border, Coordinate::Bottom() - HorizontalScrollSize - Border,
						Coordinate::Right() - Border - (_vsvisible ? VerticalScrollSize : 0), Coordinate::Bottom() - Border));
				}
				_vscroll->Show(_vsvisible);
				_hscroll->Show(_hsvisible);
			}
			void ListView::scroll_to_current(void)
			{
				if (_current >= 0) {
					int min = ElementHeight * _current;
					int max = min + ElementHeight;
					if (_vscroll->Position > min) _vscroll->SetScrollerPosition(min);
					if (_vscroll->Position + _vscroll->Page < max) _vscroll->SetScrollerPosition(max - _vscroll->Page);
				}
			}
			void ListView::select(int index)
			{
				if (MultiChoose) {
					if (Keyboard::IsKeyPressed(KeyCodes::Control)) {
						if (index != -1) {
							if (_elements[index].Selected) {
								_elements[index].Selected = false;
								if (_current == index) {
									_current = -1;
									for (int i = 0; i < _elements.Length(); i++) if (_elements[i].Selected) { _current = i; break; }
									if (_editor) CloseEmbeddedEditor();
								}
								Invalidate();
								GetParent()->RaiseEvent(ID, ControlEvent::ValueChange, this);
							} else {
								_elements[index].Selected = true;
								_current = index;
								if (_editor) CloseEmbeddedEditor();
								scroll_to_current();
								Invalidate();
								GetParent()->RaiseEvent(ID, ControlEvent::ValueChange, this);
							}
						}
					} else if (Keyboard::IsKeyPressed(KeyCodes::Shift)) {
						if (index != -1) {
							if (_current == -1) {
								_current = index;
								_elements[_current].Selected = true;
								scroll_to_current();
								Invalidate();
								GetParent()->RaiseEvent(ID, ControlEvent::ValueChange, this);
							} else {
								if (_current != index) {
									for (int i = min(_current, index); i <= max(_current, index); i++) _elements[i].Selected = true;
									_current = index;
									if (_editor) CloseEmbeddedEditor();
									scroll_to_current();
									Invalidate();
									GetParent()->RaiseEvent(ID, ControlEvent::ValueChange, this);
								}
							}
						}
					} else {
						if (index != -1) {
							if (_elements[index].Selected) {
								if (_current != index) {
									_current = index;
									if (_editor) CloseEmbeddedEditor();
									scroll_to_current();
									Invalidate();
									GetParent()->RaiseEvent(ID, ControlEvent::ValueChange, this);
								}
							} else {
								_current = index;
								for (int i = 0; i < _elements.Length(); i++) _elements[i].Selected = i == _current;
								if (_editor) CloseEmbeddedEditor();
								scroll_to_current();
								Invalidate();
								GetParent()->RaiseEvent(ID, ControlEvent::ValueChange, this);
							}
						}
					}
				} else {
					if (Keyboard::IsKeyPressed(KeyCodes::Control)) {
						if (_current == index && index != -1) {
							_elements[_current].Selected = false;
							_current = -1;
							if (_editor) CloseEmbeddedEditor();
							Invalidate();
							GetParent()->RaiseEvent(ID, ControlEvent::ValueChange, this);
						}
					} else {
						if (_current != index && index != -1) {
							if (_current != -1) _elements[_current].Selected = false;
							_current = index;
							_elements[_current].Selected = true;
							if (_editor) CloseEmbeddedEditor();
							scroll_to_current();
							Invalidate();
							GetParent()->RaiseEvent(ID, ControlEvent::ValueChange, this);
						}
					}
				}
			}
			void ListView::move_selection(int to)
			{
				int index = max(min(to, _elements.Length() - 1), 0);
				if (!_elements.Length()) return;
				if (_current != index) {
					if (MultiChoose) {
						if (Keyboard::IsKeyPressed(KeyCodes::Shift) && _current != -1) {
							for (int i = min(_current, index); i <= max(_current, index); i++) _elements[i].Selected = true;
							_current = index;
							if (_editor) CloseEmbeddedEditor();
							scroll_to_current();
							Invalidate();
							GetParent()->RaiseEvent(ID, ControlEvent::ValueChange, this);
						} else {
							_current = index;
							for (int i = 0; i < _elements.Length(); i++) _elements[i].Selected = i == _current;
							if (_editor) CloseEmbeddedEditor();
							scroll_to_current();
							Invalidate();
							GetParent()->RaiseEvent(ID, ControlEvent::ValueChange, this);
						}
					} else {
						if (_current != -1) _elements[_current].Selected = false;
						_current = index;
						_elements[_current].Selected = true;
						if (_editor) CloseEmbeddedEditor();
						scroll_to_current();
						Invalidate();
						GetParent()->RaiseEvent(ID, ControlEvent::ValueChange, this);
					}
				}
			}
			ListView::ListView(void) : _columns(0x10), _col_reorder(0x10), _elements(0x10)
			{
				ControlPosition = Rectangle::Invalid();
				Reflection::PropertyZeroInitializer Initializer;
				EnumerateProperties(Initializer);
				ResetCache();
			}
			ListView::ListView(Template::ControlTemplate * Template) : _columns(0x10), _col_reorder(0x10), _elements(0x10)
			{
				if (Template->Properties->GetTemplateClass() != L"ListView") throw InvalidArgumentException();
				static_cast<Template::Controls::ListView &>(*this) = static_cast<Template::Controls::ListView &>(*Template->Properties);
				int index = 0;
				for (int i = 0; i < Template->Children.Length(); i++) {
					if (Template->Children[i].Properties->GetTemplateClass() == L"ListViewColumn") {
						_columns.Append(ListViewColumn(index));
						static_cast<Template::Controls::ListViewColumn &>(_columns.LastElement()) = static_cast<Template::Controls::ListViewColumn &>(*Template->Children[i].Properties);
						_columns.LastElement()._rel_width = Template->Children[i].Properties->ControlPosition.Right - Template->Children[i].Properties->ControlPosition.Left;
						index++;
					}
				}
				for (int i = 0; i < _columns.Length(); i++) _col_reorder << &_columns[i];
				ResetCache();
			}
			ListView::~ListView(void) {}
			void ListView::Render(Graphics::I2DDeviceContext * device, const Box & at)
			{
				Shape ** back = 0;
				Template::Shape * temp = 0;
				Shape ** header = 0;
				Template::Shape * hdr_temp = 0;
				if (Disabled) {
					back = _view_disabled.InnerRef();
					temp = ViewDisabled.Inner();
					header = _view_header_disabled.InnerRef();
					hdr_temp = ViewHeaderDisabled.Inner();
				} else {
					if (GetFocus() == this) {
						back = _view_focused.InnerRef();
						temp = ViewFocused.Inner();
					} else {
						back = _view_normal.InnerRef();
						temp = ViewNormal.Inner();
					}
					header = _view_header_normal.InnerRef();
					hdr_temp = ViewHeaderNormal.Inner();
				}
				if (*back) {
					(*back)->Render(device, at);
				} else if (temp) {
					ZeroArgumentProvider provider;
					*back = temp->Initialize(&provider);
					(*back)->Render(device, at);
				}
				Box hdr_box(at.Left + Border, at.Top + Border, at.Right - Border - (_vsvisible ? VerticalScrollSize : 0), at.Top + Border + HeaderHeight);
				if (*header) {
					(*header)->Render(device, hdr_box);
				} else if (hdr_temp) {
					ZeroArgumentProvider provider;
					*header = hdr_temp->Initialize(&provider);
					(*header)->Render(device, hdr_box);
				}
				if (!_view_element_hot && ViewElementHot) {
					ZeroArgumentProvider provider;
					_view_element_hot.SetReference(ViewElementHot->Initialize(&provider));
				}
				if (!_view_element_selected && ViewElementSelected) {
					ZeroArgumentProvider provider;
					_view_element_selected.SetReference(ViewElementSelected->Initialize(&provider));
				}
				device->PushClip(hdr_box);
				if (Disabled) {
					int x = -_hscroll->Position;
					for (int i = 0; i < _columns.Length(); i++) {
						Box cell(hdr_box.Left + x, hdr_box.Top, hdr_box.Left + x + _col_reorder[i]->_width, hdr_box.Bottom);
						if (_col_reorder[i]->_view_disabled) {
							_col_reorder[i]->_view_disabled->Render(device, cell);
						} else if (ViewColumnHeaderDisabled) {
							ArgumentService::ListViewColumnArgumentProvider provider(this, _col_reorder[i]->Text);
							_col_reorder[i]->_view_disabled.SetReference(ViewColumnHeaderDisabled->Initialize(&provider));
							_col_reorder[i]->_view_disabled->Render(device, cell);
						}
						x += _col_reorder[i]->_width;
					}
				} else {
					int s = _hscroll->Position, x = -s;
					if (_state == 1) {
						for (int i = 0; i < _columns.Length(); i++) {
							Box cell(hdr_box.Left - s + _columns[i]._position, hdr_box.Top,
								hdr_box.Left - s + _columns[i]._position + _columns[i]._width, hdr_box.Bottom);
							if (_cell != i + 1) {
								if (_columns[i]._view_normal) {
									_columns[i]._view_normal->Render(device, cell);
								} else if (ViewColumnHeaderNormal) {
									ArgumentService::ListViewColumnArgumentProvider provider(this, _columns[i].Text);
									_columns[i]._view_normal.SetReference(ViewColumnHeaderNormal->Initialize(&provider));
									_columns[i]._view_normal->Render(device, cell);
								}
							}
						}
						Box cell(hdr_box.Left - s + _columns[_cell - 1]._position, hdr_box.Top,
							hdr_box.Left - s + _columns[_cell - 1]._position + _columns[_cell - 1]._width, hdr_box.Bottom);
						if (_columns[_cell - 1]._view_pressed) {
							_columns[_cell - 1]._view_pressed->Render(device, cell);
						} else if (ViewColumnHeaderPressed) {
							ArgumentService::ListViewColumnArgumentProvider provider(this, _columns[_cell - 1].Text);
							_columns[_cell - 1]._view_pressed.SetReference(ViewColumnHeaderPressed->Initialize(&provider));
							_columns[_cell - 1]._view_pressed->Render(device, cell);
						}
					} else {
						for (int i = 0; i < _columns.Length(); i++) {
							Box cell(hdr_box.Left + x, hdr_box.Top, hdr_box.Left + x + _col_reorder[i]->_width, hdr_box.Bottom);
							Shape ** shape = 0;
							Template::Shape * temp = 0;
							if (_hot == -1 && _cell && _columns[_cell - 1]._index == i) {
								shape = _col_reorder[i]->_view_hot.InnerRef();
								temp = ViewColumnHeaderHot;
							} else {
								shape = _col_reorder[i]->_view_normal.InnerRef();
								temp = ViewColumnHeaderNormal;
							}
							if (*shape) {
								(*shape)->Render(device, cell);
							} else if (temp) {
								ArgumentService::ListViewColumnArgumentProvider provider(this, _col_reorder[i]->Text);
								*shape = temp->Initialize(&provider);
								(*shape)->Render(device, cell);
							}
							x += _col_reorder[i]->_width;
						}
					}
				}
				device->PopClip();
				Box viewport = Box(at.Left + Border, at.Top + Border + HeaderHeight, at.Right - Border - (_vsvisible ? VerticalScrollSize : 0), at.Bottom - Border - (_hsvisible ? HorizontalScrollSize : 0));
				device->PushClip(viewport);
				int from_unclamped = _vscroll->Position / ElementHeight;
				int to_unclamped = (_vscroll->Position + _vscroll->Page) / ElementHeight;
				if (ViewElementAlternateBackground) {
					if (!_view_element_alternate) {
						ZeroArgumentProvider provider;
						_view_element_alternate = ViewElementAlternateBackground->Initialize(&provider);
					}
					for (int i = from_unclamped; i <= to_unclamped; i++) if (i & 1) {
						int y = -_vscroll->Position + i * ElementHeight;
						Box item(viewport.Left, viewport.Top + y, viewport.Right, viewport.Top + y + ElementHeight);
						_view_element_alternate->Render(device, item);
					}
				}
				if (_elements.Length()) {
					int from = max(min(from_unclamped, _elements.Length() - 1), 0);
					int to = max(min(to_unclamped, _elements.Length() - 1), 0);
					if (Disabled) {
						int y = -_vscroll->Position + from * ElementHeight;
						int sx = -_hscroll->Position;
						for (int i = from; i <= to; i++) {
							int x = sx;
							for (int j = 0; j < _columns.Length(); j++) {
								if (!(i == _current && _editor && _columns[_editor_cell]._index == j)) {
									Box cell(viewport.Left + x, viewport.Top + y, viewport.Left + x + _col_reorder[j]->_width, viewport.Top + y + ElementHeight);
									if (_elements[i].ViewDisabled.ElementAt(_col_reorder[j]->_abs_index)) _elements[i].ViewDisabled[_col_reorder[j]->_abs_index].Render(device, cell);
								}
								x += _col_reorder[j]->_width;
							}
							y += ElementHeight;
						}
					} else {
						int y = -_vscroll->Position + from * ElementHeight;
						int sx = -_hscroll->Position;
						int w;
						if (_state == 1) w = _hscroll->RangeMaximal + 1;
						else w = _col_reorder.Length() ? (_col_reorder.LastElement()->_position_limit + _col_reorder.LastElement()->_width) : 0;
						for (int i = from; i <= to; i++) {
							Box item(viewport.Left + sx, viewport.Top + y, viewport.Left + sx + w, viewport.Top + y + ElementHeight);
							if (i == _hot && _view_element_hot) {
								_view_element_hot->Render(device, item);
							} else if (_elements[i].Selected && _view_element_selected) {
								_view_element_selected->Render(device, item);
							}
							int x = sx;
							for (int j = 0; j < _columns.Length(); j++) {
								if (!(i == _current && _editor && _columns[_editor_cell]._index == j)) {
									Box cell(viewport.Left + x, viewport.Top + y, viewport.Left + x + _col_reorder[j]->_width, viewport.Top + y + ElementHeight);
									if (_elements[i].ViewNormal.ElementAt(_col_reorder[j]->_abs_index)) _elements[i].ViewNormal[_col_reorder[j]->_abs_index].Render(device, cell);
								}
								x += _col_reorder[j]->_width;
							}
							y += ElementHeight;
						}
					}
				}
				if (_editor && _editor->IsVisible()) {
					Box pos = _editor->GetPosition();
					Box rect = Box(pos.Left + at.Left, pos.Top + at.Top, pos.Right + at.Left, pos.Bottom + at.Top);
					_editor->Render(device, rect);
				}
				device->PopClip();
				if (_vscroll && _vsvisible) {
					Box pos = _vscroll->GetPosition();
					Box rect = Box(pos.Left + at.Left, pos.Top + at.Top, pos.Right + at.Left, pos.Bottom + at.Top);
					_vscroll->Render(device, rect);
				}
				if (_hscroll && _hsvisible) {
					Box pos = _hscroll->GetPosition();
					Box rect = Box(pos.Left + at.Left, pos.Top + at.Top, pos.Right + at.Left, pos.Bottom + at.Top);
					_hscroll->Render(device, rect);
				}
			}
			void ListView::ResetCache(void)
			{
				int vpos = (_vscroll) ? _vscroll->Position : 0;
				int hpos = (_hscroll) ? _hscroll->Position : 0;
				if (_editor) _editor->ResetCache();
				if (_vscroll) _vscroll->RemoveFromParent();
				if (_hscroll) _hscroll->RemoveFromParent();
				_vscroll = 0;
				_hscroll = 0;
				int vpage = ControlBoundaries.Bottom - ControlBoundaries.Top - Border - Border - HeaderHeight;
				int hpage = ControlBoundaries.Right - ControlBoundaries.Left - Border - Border;
				int vspace = ElementHeight * _elements.Length();
				int hspace = (_col_reorder.Length()) ? (_col_reorder.LastElement()->_position_limit + _col_reorder.LastElement()->_width) : 0;
				_hsvisible = (hspace > hpage || (hspace > hpage - VerticalScrollSize && vspace > vpage));
				_vsvisible = (vspace > vpage || (vspace > vpage - HorizontalScrollSize && hspace > hpage));
				if (_vsvisible) hpage -= VerticalScrollSize;
				if (_hsvisible) vpage -= HorizontalScrollSize;
				SafePointer<VerticalScrollBar> vert = new VerticalScrollBar(this);
				_vscroll = vert;
				AddChild(vert);
				_vscroll->SetRectangle(Rectangle(Coordinate::Right() - VerticalScrollSize - Border, Border,
					Coordinate::Right() - Border, Coordinate::Bottom() - Border - (_hsvisible ? HorizontalScrollSize : 0)));
				_vscroll->Disabled = Disabled;
				_vscroll->Line = ElementHeight;
				_vscroll->SetPageSilent(vpage);
				_vscroll->SetRangeSilent(0, vspace - 1);
				_vscroll->SetScrollerPositionSilent(vpos);
				_vscroll->Invisible = !_vsvisible;
				SafePointer<HorizontalScrollBar> horz = new HorizontalScrollBar(this);
				_hscroll = horz;
				AddChild(horz);
				_hscroll->SetRectangle(Rectangle(Border, Coordinate::Bottom() - HorizontalScrollSize - Border,
					Coordinate::Right() - Border - (_vsvisible ? VerticalScrollSize : 0), Coordinate::Bottom() - Border));
				_hscroll->Disabled = Disabled;
				_hscroll->Line = ElementHeight;
				_hscroll->SetPageSilent(hpage);
				_hscroll->SetRangeSilent(0, hspace - 1);
				_hscroll->SetScrollerPositionSilent(hpos);
				_hscroll->Invisible = !_hsvisible;
				for (int i = 0; i < _elements.Length(); i++) {
					for (int j = 0; j < _columns.Length(); j++) {
						if (_elements[i].ViewNormal.ElementAt(j)) _elements[i].ViewNormal[j].ClearCache();
						if (_elements[i].ViewDisabled.ElementAt(j)) _elements[i].ViewDisabled[j].ClearCache();
					}
				}
				for (int j = 0; j < _columns.Length(); j++) {
					_columns[j]._view_disabled.SetReference(0);
					_columns[j]._view_normal.SetReference(0);
					_columns[j]._view_hot.SetReference(0);
					_columns[j]._view_pressed.SetReference(0);
				}
				_view_normal.SetReference(0);
				_view_disabled.SetReference(0);
				_view_focused.SetReference(0);
				_view_element_hot.SetReference(0);
				_view_element_selected.SetReference(0);
				_view_element_alternate.SetReference(0);
				_view_header_normal.SetReference(0);
				_view_header_disabled.SetReference(0);
			}
			void ListView::ArrangeChildren(void)
			{
				Box inner = Box(0, 0, ControlBoundaries.Right - ControlBoundaries.Left, ControlBoundaries.Bottom - ControlBoundaries.Top);
				if (_vscroll) {
					auto rect = _vscroll->GetRectangle();
					if (rect.IsValid()) _vscroll->SetPosition(Box(rect, inner));
				}
				if (_hscroll) {
					auto rect = _hscroll->GetRectangle();
					if (rect.IsValid()) _hscroll->SetPosition(Box(rect, inner));
				}
				if (_editor) {
					auto svpos = _vscroll ? _vscroll->Position : 0;
					auto shpos = _hscroll ? _hscroll->Position : 0;
					auto item = Box(Border - shpos + _columns[_editor_cell]._position_limit,
						Border + HeaderHeight + ElementHeight * _current - svpos,
						Border - shpos + _columns[_editor_cell]._position_limit + _columns[_editor_cell]._width,
						Border + HeaderHeight + ElementHeight * (_current + 1) - svpos);
					_editor->SetPosition(Box(_editor->GetRectangle(), item));
				}
			}
			void ListView::Enable(bool enable) { Disabled = !enable; _hot = -1; _state = 0; if (_vscroll) _vscroll->Enable(enable); if (_hscroll) _hscroll->Enable(enable); Invalidate(); }
			bool ListView::IsEnabled(void) { return !Disabled; }
			void ListView::Show(bool visible) { Invisible = !visible; _hot = -1; _state = 0; Invalidate(); }
			bool ListView::IsVisible(void) { return !Invisible; }
			bool ListView::IsTabStop(void) { return true; }
			void ListView::SetID(int _ID) { ID = _ID; }
			int ListView::GetID(void) { return ID; }
			void ListView::SetRectangle(const Rectangle & rect) { ControlPosition = rect; if (GetParent()) GetParent()->ArrangeChildren(); Invalidate(); }
			Rectangle ListView::GetRectangle(void) { return ControlPosition; }
			void ListView::SetPosition(const Box & box)
			{
				ControlBoundaries = box;
				int shift = 0;
				for (int i = 0; i < _columns.Length(); i++) {
					if (!_col_reorder[i]->_width && (_col_reorder[i]->_rel_width.Absolute || _col_reorder[i]->_rel_width.Anchor || _col_reorder[i]->_rel_width.Zoom)) {
						_col_reorder[i]->_position = _col_reorder[i]->_position_limit = shift;
						_col_reorder[i]->_width = _col_reorder[i]->_rel_width.Absolute + int(CurrentScaleFactor * _col_reorder[i]->_rel_width.Zoom) +
							int(_col_reorder[i]->_rel_width.Anchor * double(box.Right - box.Left - Border - Border));
						_col_reorder[i]->_rel_width = Coordinate(0, 0.0, 0.0);
						shift += _col_reorder[i]->_width;
					} else {
						shift = _col_reorder[i]->_position_limit + _col_reorder[i]->_width;
					}
				}
				int vpage = ControlBoundaries.Bottom - ControlBoundaries.Top - Border - Border - HeaderHeight;
				int hpage = ControlBoundaries.Right - ControlBoundaries.Left - Border - Border;
				int vspace = ElementHeight * _elements.Length();
				int hspace = (_col_reorder.Length()) ? (_col_reorder.LastElement()->_position_limit + _col_reorder.LastElement()->_width) : 0;
				_hsvisible = (hspace > hpage || (hspace > hpage - VerticalScrollSize && vspace > vpage));
				_vsvisible = (vspace > vpage || (vspace > vpage - HorizontalScrollSize && hspace > hpage));
				if (_vsvisible) hpage -= VerticalScrollSize;
				if (_hsvisible) vpage -= HorizontalScrollSize;
				_vscroll->SetRange(0, vspace - 1);
				_vscroll->SetPage(vpage);
				_vscroll->Show(_vsvisible);
				_hscroll->SetRange(0, hspace - 1);
				_hscroll->SetPage(hpage);
				_hscroll->Show(_hsvisible);
				_vscroll->SetRectangle(Rectangle(Coordinate::Right() - VerticalScrollSize - Border, Border,
					Coordinate::Right() - Border, Coordinate::Bottom() - Border - (_hsvisible ? HorizontalScrollSize : 0)));
				_hscroll->SetRectangle(Rectangle(Border, Coordinate::Bottom() - HorizontalScrollSize - Border,
					Coordinate::Right() - Border - (_vsvisible ? VerticalScrollSize : 0), Coordinate::Bottom() - Border));
				Invalidate();
			}
			void ListView::RaiseEvent(int ID, ControlEvent event, Control * sender)
			{
				if (sender == _vscroll || sender == _hscroll) {
					Invalidate();
					if (_editor) ArrangeChildren();
				} else GetParent()->RaiseEvent(ID, event, sender);
			}
			void ListView::FocusChanged(bool got_focus) { Invalidate(); }
			void ListView::CaptureChanged(bool got_capture) { if (!got_capture) { _hot = -1; _state = 0; _cell = 0; Invalidate(); } }
			void ListView::LeftButtonDown(Point at)
			{
				SetFocus();
				if (_hot != -1 && _cell) {
					_last_cell_id = _columns[_cell - 1].ID;
					select(_hot);
				} else if (_cell) {
					SetCapture();
					if (_stretch) {
						_state = 2;
						_mouse = at.x;
					} else {
						_state = 1;
						_mouse = _mouse_start = at.x;
						GetControlSystem()->SetTimer(this, 20);
					}
					Invalidate();
				}
			}
			void ListView::LeftButtonUp(Point at)
			{
				if (_state) {
					if (_state == 2) {
						reset_scroll_ranges();
					} else if (_state == 1) {
						GetControlSystem()->SetTimer(this, 0);
						int x = -_hscroll->Position + Border;
						int p = (at.x < Border) ? 0 : (_columns.Length() - 1);
						for (int j = 0; j < _columns.Length(); j++) {
							if (x <= at.x && at.x < x + _col_reorder[j]->_width) { p = j; break; }
							x += _col_reorder[j]->_width;
						}
						int f = _columns[_cell - 1]._index;
						if (f > p) {
							for (int i = 0; i < _columns.Length(); i++) if (_columns[i]._index >= p && _columns[i]._index < f) _columns[i]._index++;
						} else if (f < p) {
							for (int i = 0; i < _columns.Length(); i++) if (_columns[i]._index <= p && _columns[i]._index > f) _columns[i]._index--;
						}
						_columns[_cell - 1]._index = p;
						for (int i = 0; i < _columns.Length(); i++) _col_reorder[_columns[i]._index] = &_columns[i];
						int shift = 0;
						for (int i = 0; i < _columns.Length(); i++) {
							_col_reorder[i]->_position_limit = _col_reorder[i]->_position = shift;
							shift += _col_reorder[i]->_width;
						}
						if (_editor) ArrangeChildren();
					}
					_hot = -1; _cell = 0;
					_state = 0;
					ReleaseCapture();
					Invalidate();
				}
			}
			void ListView::LeftButtonDoubleClick(Point at) { if (_hot != -1 && _cell) { _last_cell_id = _columns[_cell - 1].ID; GetParent()->RaiseEvent(ID, ControlEvent::DoubleClick, this); } }
			void ListView::RightButtonDown(Point at)
			{
				SetFocus();
				if (_hot != -1 && _cell) {
					_last_cell_id = _columns[_cell - 1].ID;
					select(_hot);
				}
			}
			void ListView::RightButtonUp(Point at) { if (_hot != -1 && _cell) { _last_cell_id = _columns[_cell - 1].ID; GetParent()->RaiseEvent(ID, ControlEvent::ContextClick, this); } }
			void ListView::MouseMove(Point at)
			{
				Box element(Border, Border, ControlBoundaries.Right - ControlBoundaries.Left - (_vsvisible ? VerticalScrollSize : 0), ControlBoundaries.Bottom - ControlBoundaries.Top - (_hsvisible ? HorizontalScrollSize : 0));
				bool stretch = false;
				if (!_state) {
					int oh = _hot;
					int oc = _cell;
					if (element.IsInside(at) && IsHovered()) {
						if (at.y < Border + HeaderHeight) {
							_hot = -1;
							int x = -_hscroll->Position + Border;
							_cell = 0;
							for (int j = 0; j < _columns.Length(); j++) {
								if (x <= at.x && at.x < x + _col_reorder[j]->_width) {
									_cell = _col_reorder[j]->_abs_index + 1;
									if (at.x > x + _col_reorder[j]->_width - HeaderStretchBarWidth) stretch = true;
									break;
								}
								x += _col_reorder[j]->_width;
							}
						} else {
							int hpage = (_col_reorder.Length()) ? (_col_reorder.LastElement()->_position_limit + _col_reorder.LastElement()->_width) : 0;
							if (at.x < Border + hpage - _hscroll->Position && at.x >= Border) {
								int index = (at.y - Border + _vscroll->Position - HeaderHeight) / ElementHeight;
								if (index < 0 || index >= _elements.Length()) index = -1;
								_hot = index;
								if (index != -1) {
									int x = -_hscroll->Position + Border;
									_cell = 0;
									for (int j = 0; j < _columns.Length(); j++) {
										if (x <= at.x && at.x < x + _col_reorder[j]->_width) {
											_cell = _col_reorder[j]->_abs_index + 1;
											if (at.x > x + _col_reorder[j]->_width - HeaderStretchBarWidth) stretch = true;
											break;
										}
										x += _col_reorder[j]->_width;
									}
								} else _cell = 0;
							} else {
								_hot = -1;
								_cell = 0;
							}
						}
					} else {
						_hot = -1;
						_cell = 0;
					}
					_stretch = stretch;
					if (oh != _hot || oc != _cell) {
						Invalidate();
						if (oh == -1 && oc == 0 && (_hot != -1 || _cell)) SetCapture();
						else if ((oh != -1 || oc || GetCapture() == this) && _hot == -1 && _cell == 0) ReleaseCapture();
					}
				} else {
					if (_state == 2) {
						auto col = &_columns[_cell - 1];
						int ow = col->_width;
						col->_width += at.x - _mouse;
						_mouse = at.x;
						if (col->_width < col->MinimalWidth) {
							_mouse += col->MinimalWidth - col->_width;
							col->_width = col->MinimalWidth;
						}
						int dw = col->_width - ow;
						for (int i = _columns[_cell - 1]._index + 1; i < _columns.Length(); i++) {
							_col_reorder[i]->_position += dw;
							_col_reorder[i]->_position_limit += dw;
						}
						if (_editor) ArrangeChildren();
						Invalidate();
					} else if (_state == 1) {
						int om = _mouse;
						_mouse = at.x;
						int x = -_hscroll->Position + Border;
						int p = (at.x < Border) ? 0 : (_columns.Length() - 1);
						for (int j = 0; j < _columns.Length(); j++) {
							if (x <= at.x && at.x < x + _col_reorder[j]->_width) { p = j; break; }
							x += _col_reorder[j]->_width;
						}
						int f = _columns[_cell - 1]._index;
						int shift = 0;
						for (int i = 0; i < _columns.Length(); i++) {
							if (i < min(p, f)) {
								_col_reorder[i]->_position_limit = shift;
								shift += _col_reorder[i]->_width;
							} else if (i > max(p, f)) {
								_col_reorder[i]->_position_limit = shift;
								shift += _col_reorder[i]->_width;
							} else if (i == p) {
								_col_reorder[f]->_position_limit = shift;
								shift += _col_reorder[f]->_width;
							} else {
								if (p < f) {
									_col_reorder[i - 1]->_position_limit = shift;
									shift += _col_reorder[i - 1]->_width;
								} else {
									_col_reorder[i + 1]->_position_limit = shift;
									shift += _col_reorder[i + 1]->_width;
								}
							}
						}
						if (_mouse != om) {
							_columns[_cell - 1]._position += _mouse - om;
							Invalidate();
						}
					}
				}
			}
			void ListView::ScrollVertically(double delta)
			{
				if (_vsvisible) {
					_vscroll->SetScrollerPosition(_vscroll->Position + int(delta * double(_vscroll->Line)));
				}
			}
			void ListView::ScrollHorizontally(double delta)
			{
				if (_hsvisible) {
					_hscroll->SetScrollerPosition(_hscroll->Position + int(delta * double(_hscroll->Line)));
				}
			}
			bool ListView::KeyDown(int key_code)
			{
				int page = max(_vscroll->Page / ElementHeight, 1);
				if (key_code == KeyCodes::Down) {
					if (_current != -1) move_selection(_current + 1); else move_selection(0);
				} else if (key_code == KeyCodes::Up) {
					if (_current != -1) move_selection(_current - 1); else move_selection(0);
				} else if (key_code == KeyCodes::End) {
					move_selection(_elements.Length() - 1);
				} else if (key_code == KeyCodes::Home) {
					move_selection(0);
				} else if (key_code == KeyCodes::PageDown) {
					if (_current != -1) move_selection(_current + page); else move_selection(0);
				} else if (key_code == KeyCodes::PageUp) {
					if (_current != -1) move_selection(_current - page); else move_selection(0);
				}
				return true;
			}
			void ListView::Timer(void)
			{
				for (int i = 0; i < _columns.Length(); i++) {
					if (i + 1 == _cell) continue;
					if (_columns[i]._position != _columns[i]._position_limit) {
						int pos = _columns[i]._position;
						int d = sgn(_columns[i]._position_limit - _columns[i]._position);
						_columns[i]._position += int(double(d) * CurrentScaleFactor * 20.0);
						if ((_columns[i]._position_limit - _columns[i]._position) * d < 0) _columns[i]._position = _columns[i]._position_limit;
						if (pos != _columns[i]._position) Invalidate();
					}
				}
			}
			Control * ListView::HitTest(Point at)
			{
				if (!IsEnabled()) return this;
				Box element(Border, Border, ControlBoundaries.Right - ControlBoundaries.Left - Border, ControlBoundaries.Bottom - ControlBoundaries.Top - Border);
				if (_vscroll && _vsvisible) {
					element.Right -= VerticalScrollSize;
					auto box = _vscroll->GetPosition();
					if (box.IsInside(at)) return _vscroll->HitTest(Point(at.x - box.Left, at.y - box.Top));
				}
				if (_hscroll && _hsvisible) {
					element.Bottom -= HorizontalScrollSize;
					auto box = _hscroll->GetPosition();
					if (box.IsInside(at)) return _hscroll->HitTest(Point(at.x - box.Left, at.y - box.Top));
				}
				if (_editor && element.IsInside(at)) {
					if (!_editor->IsVisible()) return this;
					auto box = _editor->GetPosition();
					if (box.IsInside(at)) return _editor->HitTest(Point(at.x - box.Left, at.y - box.Top));
				}
				return this;
			}
			void ListView::SetCursor(Point at)
			{
				Box element(Border, Border, ControlBoundaries.Right - ControlBoundaries.Left - (_vsvisible ? VerticalScrollSize : 0), ControlBoundaries.Bottom - ControlBoundaries.Top - (_hsvisible ? HorizontalScrollSize : 0));
				bool stretch = false;
				if (!_state) {
					if (element.IsInside(at) && IsHovered()) {
						if (at.y < Border + HeaderHeight) {
							int x = -_hscroll->Position + Border;
							for (int j = 0; j < _columns.Length(); j++) {
								if (x <= at.x && at.x < x + _col_reorder[j]->_width) {
									if (at.x > x + _col_reorder[j]->_width - HeaderStretchBarWidth) stretch = true;
									break;
								}
								x += _col_reorder[j]->_width;
							}
						}
					}
					if (stretch) {
						SelectCursor(Windows::SystemCursorClass::SizeLeftRight);
					} else {
						SelectCursor(Windows::SystemCursorClass::Arrow);
					}
				} else {
					if (_state == 2) {
						SelectCursor(Windows::SystemCursorClass::SizeLeftRight);
					} else {
						SelectCursor(Windows::SystemCursorClass::Arrow);
					}
				}
			}
			string ListView::GetControlClass(void) { return L"ListView"; }
			void ListView::AddColumn(const string & title, int id, int width, int minimal_width, Template::Shape * cell_normal, Template::Shape * cell_disabled)
			{
				int tw = 0;
				for (int i = 0; i < _columns.Length(); i++) tw += _columns[i]._width;
				_columns.Append(ListViewColumn(_columns.Length()));
				_col_reorder << &_columns.LastElement();
				_columns.LastElement().ID = id;
				_columns.LastElement().MinimalWidth = minimal_width;
				_columns.LastElement().Text = title;
				_columns.LastElement().ViewElementDisabled.SetRetain(cell_disabled);
				_columns.LastElement().ViewElementNormal.SetRetain(cell_normal);
				_columns.LastElement()._position = _columns.LastElement()._position_limit = tw;
				_columns.LastElement()._width = width;
				for (int i = 0; i < _elements.Length(); i++) {
					_elements[i].ViewNormal.Append(0);
					_elements[i].ViewDisabled.Append(0);
				}
				reset_scroll_ranges();
				Invalidate();
			}
			void ListView::OrderColumn(int ID, int index)
			{
				if (index < 0 || index >= _columns.Length()) return;
				int move = -1;
				for (int i = 0; i < _columns.Length(); i++) if (_columns[i].ID == ID) { move = i; break; }
				if (move == -1) return;
				int move_index = _columns[move]._index;
				if (index < move_index) {
					for (int i = 0; i < _columns.Length(); i++) if (_columns[i]._index >= index && _columns[i]._index < move_index) _columns[i]._index++;
				} else if (index > move_index) {
					for (int i = 0; i < _columns.Length(); i++) if (_columns[i]._index > move_index && _columns[i]._index <= index) _columns[i]._index--;
				}
				_columns[move]._index = index;
				for (int i = 0; i < _columns.Length(); i++) _col_reorder[_columns[i]._index] = &_columns[i];
				int shift = 0;
				for (int j = 0; j < _columns.Length(); j++) {
					_col_reorder[j]->_position = _col_reorder[j]->_position_limit = shift;
					shift += _col_reorder[j]->_width;
				}
				if (_editor) ArrangeChildren();
				Invalidate();
			}
			int ListView::GetColumnOrder(int ID)
			{
				for (int i = 0; i < _columns.Length(); i++) if (_columns[i].ID == ID) return _columns[i]._index;
				return -1;
			}
			void ListView::SetColumnTitle(int ID, const string & title)
			{
				for (int i = 0; i < _columns.Length(); i++) if (_columns[i].ID == ID) {
					_columns[i].Text = title;
					_columns[i]._view_disabled.SetReference(0);
					_columns[i]._view_normal.SetReference(0);
					_columns[i]._view_hot.SetReference(0);
					_columns[i]._view_pressed.SetReference(0);
					Invalidate();
					break;
				}
			}
			string ListView::GetColumnTitle(int ID)
			{
				for (int i = 0; i < _columns.Length(); i++) if (_columns[i].ID == ID) return _columns[i].Text;
				return L"";
			}
			void ListView::SetColumnWidth(int ID, int width)
			{
				for (int i = 0; i < _columns.Length(); i++) if (_columns[i].ID == ID) {
					_columns[i]._width = width;
					int shift = 0;
					for (int j = 0; j < _columns.Length(); j++) {
						_col_reorder[j]->_position = _col_reorder[j]->_position_limit = shift;
						shift += _col_reorder[j]->_width;
					}
					reset_scroll_ranges();
					if (_editor) ArrangeChildren();
					Invalidate();
					return;
				}
			}
			int ListView::GetColumnWidth(int ID)
			{
				for (int i = 0; i < _columns.Length(); i++) if (_columns[i].ID == ID) return _columns[i]._width;
				return 0;
			}
			void ListView::SetColumnMinimalWidth(int ID, int width)
			{
				for (int i = 0; i < _columns.Length(); i++) if (_columns[i].ID == ID) {
					_columns[i].MinimalWidth = width;
					return;
				}
			}
			int ListView::GetColumnMinimalWidth(int ID)
			{
				for (int i = 0; i < _columns.Length(); i++) if (_columns[i].ID == ID) return _columns[i].MinimalWidth;
				return 0;
			}
			void ListView::SetColumnNormalCell(int ID, Template::Shape * shape)
			{
				for (int i = 0; i < _columns.Length(); i++) if (_columns[i].ID == ID) {
					_columns[i].ViewElementNormal.SetRetain(shape);
					for (int j = 0; j < _elements.Length(); j++) _elements[j].ViewNormal.SetElement(0, i);
					Invalidate();
					return;
				}
			}
			Template::Shape * ListView::GetColumnNormalCell(int ID)
			{
				for (int i = 0; i < _columns.Length(); i++) if (_columns[i].ID == ID) return _columns[i].ViewElementNormal;
				return 0;
			}
			void ListView::SetColumnDisabledCell(int ID, Template::Shape * shape)
			{
				for (int i = 0; i < _columns.Length(); i++) if (_columns[i].ID == ID) {
					_columns[i].ViewElementDisabled.SetRetain(shape);
					for (int j = 0; j < _elements.Length(); j++) _elements[j].ViewDisabled.SetElement(0, i);
					Invalidate();
					return;
				}
			}
			Template::Shape * ListView::GetColumnDisabledCell(int ID)
			{
				for (int i = 0; i < _columns.Length(); i++) if (_columns[i].ID == ID) return _columns[i].ViewElementDisabled;
				return 0;
			}
			void ListView::SetColumnID(int ID, int NewID)
			{
				for (int i = 0; i < _columns.Length(); i++) if (_columns[i].ID == ID) {
					_columns[i].ID = NewID;
					return;
				}
			}
			Array<int> ListView::GetColumns(void)
			{
				Array<int> result(_columns.Length());
				for (int i = 0; i < _columns.Length(); i++) result << _columns[i].ID;
				return result;
			}
			void ListView::RemoveColumn(int ID)
			{
				for (int i = 0; i < _columns.Length(); i++) if (_columns[i].ID == ID) {
					if (_columns.Length() == 1) { ClearListView(); return; }
					if (_editor && _editor_cell == i) CloseEmbeddedEditor();
					for (int j = 0; j < _columns.Length(); j++) if (_columns[j]._index > _columns[i]._index) _columns[j]._index--;
					if (_editor_cell > i) _editor_cell--;
					_state = _cell = _mouse = _mouse_start = 0;
					_stretch = false;
					_columns.Remove(i);
					_col_reorder.RemoveLast();
					for (int j = 0; j < _columns.Length(); j++) {
						_columns[j]._abs_index = j;
						_col_reorder[_columns[j]._index] = &_columns[j];
					}
					int shift = 0;
					for (int j = 0; j < _columns.Length(); j++) {
						_col_reorder[j]->_position = _col_reorder[j]->_position_limit = shift;
						shift += _col_reorder[j]->_width;
					}
					for (int j = 0; j < _elements.Length(); j++) {
						_elements[j].ViewNormal.Remove(i);
						_elements[j].ViewDisabled.Remove(i);
					}
					if (_editor) ArrangeChildren();
					reset_scroll_ranges();
					Invalidate();
					return;
				}
			}
			void ListView::ClearListView(void)
			{
				CloseEmbeddedEditor();
				_columns.Clear();
				_col_reorder.Clear();
				_elements.Clear();
				_current = _hot = -1;
				_last_cell_id = _state = _cell = _mouse = _mouse_start = 0;
				_stretch = false;
				if (GetCapture() == this) ReleaseCapture();
				reset_scroll_ranges();
				Invalidate();
			}
			void ListView::AddItem(IArgumentProvider * provider, void * user) { InsertItem(provider, _elements.Length(), user); }
			void ListView::AddItem(Reflection::Reflected & object, void * user) { InsertItem(object, _elements.Length(), user); }
			void ListView::InsertItem(IArgumentProvider * provider, int at, void * user)
			{
				if (!_columns.Length()) return;
				ArgumentService::ListViewWrapperArgumentProvider wrapper(this, provider);
				_elements.Insert(Element(this), at);
				auto & e = _elements[at];
				e.User = user;
				for (int i = 0; i < _columns.Length(); i++) {
					SafePointer<Shape> normal = _columns[i].ViewElementNormal->Initialize(&wrapper);
					SafePointer<Shape> disabled = _columns[i].ViewElementDisabled->Initialize(&wrapper);
					e.ViewNormal.Append(normal);
					e.ViewDisabled.Append(disabled);
				}
				e.Selected = false;
				reset_scroll_ranges();
				if (_current >= at) {
					_current++;
					if (_editor) ArrangeChildren();
				}
				Invalidate();
			}
			void ListView::InsertItem(Reflection::Reflected & object, int at, void * user)
			{
				ReflectorArgumentProvider provider(&object);
				InsertItem(&provider, at, user);
			}
			void ListView::ResetItem(int index, IArgumentProvider * provider)
			{
				ArgumentService::ListViewWrapperArgumentProvider wrapper(this, provider);
				_elements[index].ViewNormal.Clear();
				_elements[index].ViewDisabled.Clear();
				for (int i = 0; i < _columns.Length(); i++) {
					SafePointer<Shape> normal = _columns[i].ViewElementNormal->Initialize(&wrapper);
					SafePointer<Shape> disabled = _columns[i].ViewElementDisabled->Initialize(&wrapper);
					_elements[index].ViewNormal.Append(normal);
					_elements[index].ViewDisabled.Append(disabled);
				}
				Invalidate();
			}
			void ListView::ResetItem(int index, Reflection::Reflected & object)
			{
				ReflectorArgumentProvider provider(&object);
				ResetItem(index, &provider);
			}
			void ListView::SwapItems(int i, int j)
			{
				if (_current == i) { _current = j; if (_editor) ArrangeChildren(); }
				else if (_current == j) { _current = i; if (_editor) ArrangeChildren(); }
				_elements.SwapAt(i, j);
				Invalidate();
			}
			void ListView::RemoveItem(int index)
			{
				_elements.Remove(index);
				reset_scroll_ranges();
				if (_current == index) {
					_current = -1;
					if (_editor) CloseEmbeddedEditor();
				} else if (_current > index) {
					_current--;
					if (_editor) ArrangeChildren();
				}
				_hot = -1;
				_state = _cell = _mouse = _mouse_start = 0;
				_stretch = false;
				Invalidate();
			}
			void ListView::ClearItems(void)
			{
				_elements.Clear();
				if (_editor) CloseEmbeddedEditor();
				_current = -1; _hot = -1;
				_state = _cell = _mouse = _mouse_start = 0;
				_stretch = false;
				reset_scroll_ranges();
				Invalidate();
			}
			int ListView::ItemCount(void) { return _elements.Length(); }
			void * ListView::GetItemUserData(int index) { return _elements[index].User; }
			void ListView::SetItemUserData(int index, void * user) { _elements[index].User = user; }
			int ListView::GetSelectedIndex(void) { return _current; }
			void ListView::SetSelectedIndex(int index, bool scroll_to_view)
			{
				if (_current != index && _editor) CloseEmbeddedEditor();
				if (MultiChoose) {
					_current = index;
					for (int i = 0; i < _elements.Length(); i++) _elements[i].Selected = i == _current;
				} else {
					if (_current != -1) _elements[_current].Selected = false;
					_current = index;
					if (_current != -1) _elements[_current].Selected = true;
				}
				if (scroll_to_view && _current >= 0) scroll_to_current();
				Invalidate();
			}
			bool ListView::IsItemSelected(int index) { return _elements[index].Selected; }
			void ListView::SelectItem(int index, bool select)
			{
				if (MultiChoose) {
					_elements[index].Selected = select;
					if (!select && index == _current) {
						_current = -1;
						if (_editor) CloseEmbeddedEditor();
						for (int i = 0; i < _elements.Length(); i++) if (_elements[i].Selected) { _current = i; break; }
					}
				} else {
					if (select) SetSelectedIndex(index);
					else if (index == _current) SetSelectedIndex(-1);
				}
				Invalidate();
			}
			int ListView::GetLastCellID(void) { return _last_cell_id; }
			Control * ListView::CreateEmbeddedEditor(Template::ControlTemplate * Template, int CellID, const Rectangle & Position, IControlFactory * Factory)
			{
				CloseEmbeddedEditor();
				if (_current == -1) return 0;
				int cell = -1;
				for (int i = 0; i < _columns.Length(); i++) if (_columns[i].ID == CellID) { cell = i; break; }
				if (cell == -1) return 0;
				SafePointer<ControlGroup> group = new ControlGroup;
				AddChild(group);
				try {
					group->ControlPosition = Position;
					CreateChildrenControls(group, Template, Factory);
				} catch (...) { group->RemoveFromParent(); throw; }
				_editor = group;
				_editor_cell = cell;
				ArrangeChildren();
				Invalidate();
				return group;
			}
			Control * ListView::GetEmbeddedEditor(void) { return _editor; }
			void ListView::CloseEmbeddedEditor(void) { if (_editor) { auto editor = _editor; _editor = 0; _editor_cell = -1; editor->RemoveFromParent(); Invalidate(); } }
			int ListView::GetScrollerPosition(void) { return _vscroll->Position; }
			void ListView::SetScrollerPosition(int position) { _vscroll->SetScrollerPosition(position); }
		}
	}
}