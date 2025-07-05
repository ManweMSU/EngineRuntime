#include "Registry.h"

#include "../Miscellaneous/DynamicString.h"

namespace Engine
{
	namespace Storage
	{
		ENGINE_PACKED_STRUCTURE(EngineRegistryHeader)
			char Signature[8];  // "ecs.1.0"
			uint32 SignatureEx; // 0x80000004
			uint32 Version;     // 0
			uint32 DataOffset;
			uint32 DataSize;
			uint32 RootOffset;
		ENGINE_END_PACKED_STRUCTURE
		struct RegistryStringOptimizationPair {
			uint32 base_offset;
			int length;
		};

		class RegistryValue
		{
		public:
			RegistryValueType type;
			union
			{
#ifdef ENGINE_X64
				struct {
					int32 value_int32;
					uint32 unused0[3];
				};
				struct {
					float value_float;
					uint32 unused1[3];
				};
				struct {
					uint8 * value_binary;
					uint64 value_binary_size;
				};
				struct {
					int64 value_int64;
					uint64 unused3;
				};
				struct {
					double value_double;
					uint64 unused4;
				};
				uint32 value[4];
#else
				struct {
					int32 value_int32;
					uint32 unused0;
				};
				struct {
					float value_float;
					uint32 unused1;
				};
				struct {
					uint8 * value_binary;
					uint32 value_binary_size;
				};
				struct {
					int64 value_int64;
				};
				struct {
					double value_double;
				};
				uint32 value[2];
#endif
			};

			RegistryValue(void) : RegistryValue(RegistryValueType::Integer) {}
			RegistryValue(RegistryValueType value_type)
			{
				type = value_type;
				ZeroMemory(value, sizeof(value));
			}
			RegistryValue(const RegistryValue & src)
			{
				type = src.type;
				if (type == RegistryValueType::String || type == RegistryValueType::Binary) {
					value_binary_size = src.value_binary_size;
					value_binary = reinterpret_cast<uint8 *>(malloc(value_binary_size));
					if (!value_binary && value_binary_size) throw OutOfMemoryException();
					MemoryCopy(value_binary, src.value_binary, value_binary_size);
				} else {
					MemoryCopy(value, src.value, sizeof(value));
				}
			}
			RegistryValue(RegistryValue && src)
			{
				type = src.type;
				MemoryCopy(value, src.value, sizeof(value));
				ZeroMemory(src.value, sizeof(value));
			}
			~RegistryValue(void) { if (type == RegistryValueType::String || type == RegistryValueType::Binary) free(value_binary); }
			RegistryValue & operator = (const RegistryValue & src)
			{
				if (this == &src) return *this;
				if (type == RegistryValueType::String || type == RegistryValueType::Binary) free(value_binary);
				type = src.type;
				if (type == RegistryValueType::String || type == RegistryValueType::Binary) {
					value_binary_size = src.value_binary_size;
					value_binary = reinterpret_cast<uint8 *>(malloc(value_binary_size));
					if (!value_binary && value_binary_size) throw OutOfMemoryException();
					MemoryCopy(value_binary, src.value_binary, value_binary_size);
				} else {
					MemoryCopy(value, src.value, sizeof(value));
				}
				return *this;
			}
			RegistryValue & operator = (RegistryValue && src)
			{
				if (this == &src) return *this;
				if (type == RegistryValueType::String || type == RegistryValueType::Binary) free(value_binary);
				type = src.type;
				MemoryCopy(value, src.value, sizeof(value));
				ZeroMemory(src.value, sizeof(value));
				return *this;
			}
			void SetBinarySize(int size)
			{
				if (type == RegistryValueType::String || type == RegistryValueType::Binary) {
					uint8 * reset = reinterpret_cast<uint8 *>(realloc(value_binary, size));
					if (!reset && size) throw OutOfMemoryException();
					value_binary = reset;
					value_binary_size = size;
				}
			}
			void Set(int32 new_value)
			{
				if (type == RegistryValueType::Integer) value_int32 = new_value;
				else if (type == RegistryValueType::Float) value_float = float(new_value);
				else if (type == RegistryValueType::Boolean) value_int32 = new_value ? 1 : 0;
				else if (type == RegistryValueType::String) Set(string(new_value));
				else if (type == RegistryValueType::LongInteger) value_int64 = new_value;
				else if (type == RegistryValueType::LongFloat) value_double = double(new_value);
				else if (type == RegistryValueType::Color) value_int32 = new_value;
				else if (type == RegistryValueType::Time) value_int64 = new_value;
				else if (type == RegistryValueType::Binary) Set(&new_value, sizeof(new_value));
			}
			void Set(float new_value)
			{
				if (type == RegistryValueType::Integer) value_int32 = int32(new_value);
				else if (type == RegistryValueType::Float) value_float = new_value;
				else if (type == RegistryValueType::Boolean) value_int32 = new_value ? 1 : 0;
				else if (type == RegistryValueType::String) Set(string(new_value));
				else if (type == RegistryValueType::LongInteger) value_int64 = int64(new_value);
				else if (type == RegistryValueType::LongFloat) value_double = double(new_value);
				else if (type == RegistryValueType::Color) value_int32 = int32(new_value);
				else if (type == RegistryValueType::Time) value_int64 = int64(new_value);
				else if (type == RegistryValueType::Binary) Set(&new_value, sizeof(new_value));
			}
			void Set(bool new_value)
			{
				if (type == RegistryValueType::Integer) value_int32 = new_value ? 1 : 0;
				else if (type == RegistryValueType::Float) value_float = new_value ? 1.0f : 0.0f;
				else if (type == RegistryValueType::Boolean) value_int32 = new_value ? 1 : 0;
				else if (type == RegistryValueType::String) Set(string(new_value));
				else if (type == RegistryValueType::LongInteger) value_int64 = new_value ? 1 : 0;
				else if (type == RegistryValueType::LongFloat) value_double = new_value ? 1.0 : 0.0;
				else if (type == RegistryValueType::Color) value_int32 = new_value ? 1 : 0;
				else if (type == RegistryValueType::Time) value_int64 = new_value ? 1 : 0;
				else if (type == RegistryValueType::Binary) Set(&new_value, sizeof(new_value));
			}
			void Set(const string & new_value)
			{
				if (type == RegistryValueType::Integer) value_int32 = new_value.ToInt32();
				else if (type == RegistryValueType::Float) value_float = new_value.ToFloat();
				else if (type == RegistryValueType::Boolean) value_int32 = new_value.ToBoolean();
				else if (type == RegistryValueType::String || type == RegistryValueType::Binary) {
					if (new_value.Length()) {
						SetBinarySize((new_value.GetEncodedLength(Encoding::UTF16) + 1) * 2);
						new_value.Encode(value_binary, Encoding::UTF16, true);
					} else SetBinarySize(0);
				} else if (type == RegistryValueType::LongInteger) value_int64 = new_value.ToInt64();
				else if (type == RegistryValueType::LongFloat) value_double = new_value.ToDouble();
				else if (type == RegistryValueType::Color) value_int32 = new_value.ToUInt32();
				else if (type == RegistryValueType::Time) value_int64 = new_value.ToUInt64();
			}
			void Set(int64 new_value)
			{
				if (type == RegistryValueType::Integer) value_int32 = int32(new_value);
				else if (type == RegistryValueType::Float) value_float = float(new_value);
				else if (type == RegistryValueType::Boolean) value_int32 = new_value ? 1 : 0;
				else if (type == RegistryValueType::String) Set(string(new_value));
				else if (type == RegistryValueType::LongInteger) value_int64 = new_value;
				else if (type == RegistryValueType::LongFloat) value_double = double(new_value);
				else if (type == RegistryValueType::Color) value_int32 = int32(new_value);
				else if (type == RegistryValueType::Time) value_int64 = new_value;
				else if (type == RegistryValueType::Binary) Set(&new_value, sizeof(new_value));
			}
			void Set(double new_value)
			{
				if (type == RegistryValueType::Integer) value_int32 = int32(new_value);
				else if (type == RegistryValueType::Float) value_float = float(new_value);
				else if (type == RegistryValueType::Boolean) value_int32 = new_value ? 1 : 0;
				else if (type == RegistryValueType::String) Set(string(new_value));
				else if (type == RegistryValueType::LongInteger) value_int64 = int64(new_value);
				else if (type == RegistryValueType::LongFloat) value_double = new_value;
				else if (type == RegistryValueType::Color) value_int32 = int32(new_value);
				else if (type == RegistryValueType::Time) value_int64 = int64(new_value);
				else if (type == RegistryValueType::Binary) Set(&new_value, sizeof(new_value));
			}
			void Set(Color new_value) { Set(int32(new_value.Value)); }
			void Set(Time new_value) { Set(int64(new_value.Ticks)); }
			void Set(const void * new_value, int size)
			{
				if (type == RegistryValueType::Integer) {
					if (size >= 4) value_int32 = *reinterpret_cast<const int32 *>(new_value);
					else value_int32 = 0;
				} else if (type == RegistryValueType::Float) {
					if (size >= 4) value_float = *reinterpret_cast<const float *>(new_value);
					else value_float = 0.0;
				} else if (type == RegistryValueType::Boolean) {
					if (size >= 1) value_int32 = (*reinterpret_cast<const uint8 *>(new_value)) ? 1 : 0;
					else value_int32 = 0;
				} else if (type == RegistryValueType::String) {
					DynamicString result;
					for (int i = 0; i < size; i++) {
						result += string(uint32(reinterpret_cast<const uint8 *>(new_value)[i]), L"0123456789ABCDEF", 2);
					}
					Set(result.ToString());
				} else if (type == RegistryValueType::LongInteger) {
					if (size >= 8) value_int64 = *reinterpret_cast<const int64 *>(new_value);
					else value_int64 = 0;
				} else if (type == RegistryValueType::LongFloat) {
					if (size >= 8) value_double = *reinterpret_cast<const double *>(new_value);
					else value_double = 0.0;
				} else if (type == RegistryValueType::Color) {
					if (size >= 4) value_int32 = *reinterpret_cast<const int32 *>(new_value);
					else value_int32 = 0;
				} else if (type == RegistryValueType::Time) {
					if (size >= 8) value_int64 = *reinterpret_cast<const int64 *>(new_value);
					else value_int64 = 0;
				} else if (type == RegistryValueType::Binary) {
					SetBinarySize(size);
					MemoryCopy(value_binary, new_value, size);
				}
			}
			int32 GetInteger(void) const
			{
				if (type == RegistryValueType::Integer || type == RegistryValueType::Boolean ||
					type == RegistryValueType::Color) return value_int32;
				else if (type == RegistryValueType::Float) return int32(value_float);
				else if (type == RegistryValueType::String) return value_binary_size ? string(value_binary, -1, Encoding::UTF16).ToInt32() : 0;
				else if (type == RegistryValueType::LongInteger || type == RegistryValueType::Time) return int32(value_int64);
				else if (type == RegistryValueType::LongFloat) return int32(value_double);
				else if (type == RegistryValueType::Binary) {
					if (value_binary_size >= 4) return *reinterpret_cast<const int32 *>(value_binary);
					else return 0;
				} else return 0;
			}
			float GetFloat(void) const
			{
				if (type == RegistryValueType::Integer || type == RegistryValueType::Boolean ||
					type == RegistryValueType::Color) return float(value_int32);
				else if (type == RegistryValueType::Float) return value_float;
				else if (type == RegistryValueType::String) return value_binary_size ? string(value_binary, -1, Encoding::UTF16).ToFloat() : 0.0f;
				else if (type == RegistryValueType::LongInteger || type == RegistryValueType::Time) return float(value_int64);
				else if (type == RegistryValueType::LongFloat) return float(value_double);
				else if (type == RegistryValueType::Binary) {
					if (value_binary_size >= 4) return *reinterpret_cast<const float *>(value_binary);
					else return 0.0f;
				} else return 0.0f;
			}
			bool GetBoolean(void) const
			{
				if (type == RegistryValueType::Integer || type == RegistryValueType::Boolean ||
					type == RegistryValueType::Color) return value_int32 ? true : false;
				else if (type == RegistryValueType::Float) return value_float ? true : false;
				else if (type == RegistryValueType::String) return value_binary_size ? string(value_binary, -1, Encoding::UTF16).ToBoolean() : false;
				else if (type == RegistryValueType::LongInteger || type == RegistryValueType::Time) return value_int64 ? true : false;
				else if (type == RegistryValueType::LongFloat) return value_double ? true : false;
				else if (type == RegistryValueType::Binary) {
					if (value_binary_size >= 1) return *reinterpret_cast<const bool *>(value_binary);
					else return false;
				} else return false;
			}
			string GetString(void) const
			{
				if (type == RegistryValueType::Integer) return string(value_int32);
				else if (type == RegistryValueType::Float) return string(value_float);
				else if (type == RegistryValueType::Boolean) return string(value_int32 ? true : false);
				else if (type == RegistryValueType::String) return value_binary_size ? string(value_binary, -1, Encoding::UTF16) : L"";
				else if (type == RegistryValueType::LongInteger) return string(value_int64);
				else if (type == RegistryValueType::LongFloat) return string(value_double);
				else if (type == RegistryValueType::Color) {
					Color val = Color(uint32(value_int32));
					return L"(" + string(val.r) + L", " + string(val.g) + L", " + string(val.b) + L", " + string(val.a) + L")";
				} else if (type == RegistryValueType::Time) return Time(uint64(value_int64)).ToString();
				else if (type == RegistryValueType::Binary) {
					DynamicString result;
					for (uint i = 0; i < value_binary_size; i++) {
						result += string(uint32(value_binary[i]), L"0123456789ABCDEF", 2);
					}
					return result.ToString();
				} else return L"";
			}
			int64 GetLongInteger(void) const
			{
				if (type == RegistryValueType::Integer || type == RegistryValueType::Boolean ||
					type == RegistryValueType::Color) return int64(value_int32);
				else if (type == RegistryValueType::Float) return int64(value_float);
				else if (type == RegistryValueType::String) return value_binary_size ? string(value_binary, -1, Encoding::UTF16).ToInt64() : 0;
				else if (type == RegistryValueType::LongInteger || type == RegistryValueType::Time) return value_int64;
				else if (type == RegistryValueType::LongFloat) return int64(value_double);
				else if (type == RegistryValueType::Binary) {
					if (value_binary_size >= 8) return *reinterpret_cast<const int64 *>(value_binary);
					else return 0;
				} else return 0;
			}
			double GetLongFloat(void) const
			{
				if (type == RegistryValueType::Integer || type == RegistryValueType::Boolean ||
					type == RegistryValueType::Color) return double(value_int32);
				else if (type == RegistryValueType::Float) return double(value_float);
				else if (type == RegistryValueType::String) return value_binary_size ? string(value_binary, -1, Encoding::UTF16).ToDouble() : 0.0;
				else if (type == RegistryValueType::LongInteger || type == RegistryValueType::Time) return double(value_int64);
				else if (type == RegistryValueType::LongFloat) return value_double;
				else if (type == RegistryValueType::Binary) {
					if (value_binary_size >= 8) return *reinterpret_cast<const double *>(value_binary);
					else return 0.0;
				} else return 0.0;
			}
			Color GetColor(void) const { return Color(uint32(GetInteger())); }
			Time GetTime(void) const { return Time(uint64(GetLongInteger())); }
			int GetBinarySize() const
			{
				if (type == RegistryValueType::Integer) return 4;
				else if (type == RegistryValueType::Float) return 4;
				else if (type == RegistryValueType::Boolean) return 1;
				else if (type == RegistryValueType::String) return int(value_binary_size);
				else if (type == RegistryValueType::LongInteger) return 8;
				else if (type == RegistryValueType::LongFloat) return 8;
				else if (type == RegistryValueType::Color) return 4;
				else if (type == RegistryValueType::Time) return 8;
				else if (type == RegistryValueType::Binary) return int(value_binary_size);
				else return 0;
			}
			void GetBinary(void * buffer) const
			{
				if (type == RegistryValueType::String || type == RegistryValueType::Binary) {
					MemoryCopy(buffer, value_binary, value_binary_size);
				} else {
					MemoryCopy(buffer, value, GetBinarySize());
				}
			}
		};
		class RegistryNodeImplementation : public RegistryNode
		{
			Array<string> NodeNames;
			Array<string> ValueNames;
			ObjectArray<RegistryNodeImplementation> Nodes;
			Array<RegistryValue> Values;
		public:
			RegistryNodeImplementation(void) : NodeNames(0x10), ValueNames(0x10), Nodes(0x10), Values(0x10) {}
			virtual ~RegistryNodeImplementation(void) override {}

			RegistryNodeImplementation * FindNode(const string & name) const
			{
				for (int i = 0; i < NodeNames.Length(); i++) if (string::CompareIgnoreCase(name, NodeNames[i]) == 0) {
					return Nodes.ElementAt(i);
				}
				return 0;
			}
			int FindNodeIndex(const string & name) const
			{
				for (int i = 0; i < NodeNames.Length(); i++) if (string::CompareIgnoreCase(name, NodeNames[i]) == 0) {
					return i;
				}
				return -1;
			}
			RegistryValue * FindValue(const string & name)
			{
				for (int i = 0; i < ValueNames.Length(); i++) if (string::CompareIgnoreCase(name, ValueNames[i]) == 0) {
					return &Values[i];
				}
				return 0;
			}
			const RegistryValue * FindValue(const string & name) const
			{
				for (int i = 0; i < ValueNames.Length(); i++) if (string::CompareIgnoreCase(name, ValueNames[i]) == 0) {
					return &Values[i];
				}
				return 0;
			}
			int FindValueIndex(const string & name) const
			{
				for (int i = 0; i < ValueNames.Length(); i++) if (string::CompareIgnoreCase(name, ValueNames[i]) == 0) {
					return i;
				}
				return -1;
			}
			RegistryNodeImplementation * ResolveNodeByPath(const string & path)
			{
				auto result = this;
				int pos = 0;
				while (pos < path.Length()) {
					int ep = pos;
					while (ep < path.Length() && path[ep] != L'\\' && path[ep] != L'/') ep++;
					string name = path.Fragment(pos, ep - pos);
					if (name.Length()) {
						result = result->FindNode(name);
						if (!result) return 0;
					}
					pos = ep + 1;
				}
				return result;
			}
			const RegistryNodeImplementation * ResolveNodeByPath(const string & path) const
			{
				auto result = this;
				int pos = 0;
				while (pos < path.Length()) {
					int ep = pos;
					while (ep < path.Length() && path[ep] != L'\\' && path[ep] != L'/') ep++;
					string name = path.Fragment(pos, ep - pos);
					if (name.Length()) {
						result = result->FindNode(name);
						if (!result) return 0;
					}
					pos = ep + 1;
				}
				return result;
			}
			RegistryValue * ResolveValueByPath(const string & path)
			{
				auto at = ResolveNodeByPath(IO::Path::GetDirectory(path));
				if (at) {
					return at->FindValue(IO::Path::GetFileName(path));
				} else return 0;
			}
			const RegistryValue * ResolveValueByPath(const string & path) const
			{
				auto at = ResolveNodeByPath(IO::Path::GetDirectory(path));
				if (at) {
					return at->FindValue(IO::Path::GetFileName(path));
				} else return 0;
			}
			int CreateRawNode(const string & name, RegistryNodeImplementation * attach = 0)
			{
				if (!name.Length() || name.FindFirst(L'\\') != -1 || name.FindFirst(L'/') != -1) return -1;
				int at = NodeNames.Length();
				for (int i = 0; i < NodeNames.Length(); i++) {
					int comp = string::CompareIgnoreCase(name, NodeNames[i]);
					if (!comp) return i;
					if (comp < 0) {
						at = i;
						break;
					}
				}
				if (attach) {
					Nodes.Insert(attach, at);
				} else {
					SafePointer<RegistryNodeImplementation> node = new RegistryNodeImplementation;
					Nodes.Insert(node, at);
				}
				try { NodeNames.Insert(name, at); } catch (...) { Nodes.Remove(at); throw; }
				return at;
			}
			int CreateRawValue(const string & name, RegistryValueType type)
			{
				if (!name.Length() || name.FindFirst(L'\\') != -1 || name.FindFirst(L'/') != -1) return -1;
				if (static_cast<int>(type) < 1 || static_cast<int>(type) > 9) return -1;
				int at = ValueNames.Length();
				for (int i = 0; i < ValueNames.Length(); i++) {
					int comp = string::CompareIgnoreCase(name, ValueNames[i]);
					if (!comp) return -1;
					if (comp < 0) {
						at = i;
						break;
					}
				}
				Values.Insert(RegistryValue(type), at);
				try { ValueNames.Insert(name, at); } catch (...) { Values.Remove(at); throw; }
				return at;
			}

			virtual const Array<string>& GetSubnodes(void) const override { return NodeNames; }
			virtual const Array<string>& GetValues(void) const override { return ValueNames; }
			virtual void CreateNode(const string & path) override
			{
				auto at = ResolveNodeByPath(IO::Path::GetDirectory(path));
				if (at) {
					if (at->CreateRawNode(IO::Path::GetFileName(path)) == -1) throw InvalidArgumentException();
				} else throw InvalidArgumentException();
			}
			virtual void RemoveNode(const string & path) override
			{
				auto at = ResolveNodeByPath(IO::Path::GetDirectory(path));
				if (at) {
					int index = at->FindNodeIndex(IO::Path::GetFileName(path));
					if (index >= 0) {
						at->NodeNames.Remove(index);
						at->Nodes.Remove(index);
					} else throw InvalidArgumentException();
				} else throw InvalidArgumentException();
			}
			virtual void RenameNode(const string & node, const string & name) override
			{
				auto remove_at = ResolveNodeByPath(IO::Path::GetDirectory(node));
				auto attach_at = ResolveNodeByPath(IO::Path::GetDirectory(name));
				if (remove_at && attach_at) {
					int index = remove_at->FindNodeIndex(IO::Path::GetFileName(node));
					int index_no = attach_at->FindNodeIndex(IO::Path::GetFileName(name));
					if (remove_at == attach_at && index == index_no && index != -1) {
						remove_at->NodeNames[index] = IO::Path::GetFileName(name);
					} else if (index >= 0 && index_no == -1) {
						auto ptr = remove_at->Nodes.ElementAt(index);
						ptr->Retain();
						remove_at->NodeNames.Remove(index);
						remove_at->Nodes.Remove(index);
						attach_at->CreateRawNode(IO::Path::GetFileName(name), ptr);
						ptr->Release();
					} else throw InvalidArgumentException();
				} else throw InvalidArgumentException();
			}
			virtual RegistryNode * OpenNode(const string & path) override
			{
				auto node = ResolveNodeByPath(path);
				if (node) {
					node->Retain();
					return node;
				} else return 0;
			}
			virtual void CreateValue(const string & path, RegistryValueType type) override
			{
				auto at = ResolveNodeByPath(IO::Path::GetDirectory(path));
				if (at) {
					if (at->CreateRawValue(IO::Path::GetFileName(path), type) == -1) throw InvalidArgumentException();
				} else throw InvalidArgumentException();
			}
			virtual void RemoveValue(const string & path) override
			{
				auto at = ResolveNodeByPath(IO::Path::GetDirectory(path));
				if (at) {
					int index = at->FindValueIndex(IO::Path::GetFileName(path));
					if (index >= 0) {
						at->ValueNames.Remove(index);
						at->Values.Remove(index);
					} else throw InvalidArgumentException();
				} else throw InvalidArgumentException();
			}
			virtual void RenameValue(const string & name_from, const string & name_to) override
			{
				auto remove_at = ResolveNodeByPath(IO::Path::GetDirectory(name_from));
				auto attach_at = ResolveNodeByPath(IO::Path::GetDirectory(name_to));
				if (remove_at && attach_at) {
					int index = remove_at->FindValueIndex(IO::Path::GetFileName(name_from));
					int index_no = attach_at->FindValueIndex(IO::Path::GetFileName(name_to));
					if (remove_at == attach_at && index == index_no && index != -1) {
						remove_at->ValueNames[index] = IO::Path::GetFileName(name_to);
					} else if (index >= 0 && index_no == -1) {
						int index_ins = attach_at->CreateRawValue(IO::Path::GetFileName(name_to), remove_at->Values[index].type);
						if (index_ins <= index) index++;
						attach_at->Values[index_ins] = remove_at->Values[index];
						remove_at->ValueNames.Remove(index);
						remove_at->Values.Remove(index);
					} else throw InvalidArgumentException();
				} else throw InvalidArgumentException();
			}
			virtual RegistryValueType GetValueType(const string & path) const override
			{
				auto storage = ResolveValueByPath(path);
				if (storage) {
					return storage->type;
				} else return RegistryValueType::Unknown;
			}
			virtual int32 GetValueInteger(const string & path) const override
			{
				auto storage = ResolveValueByPath(path);
				return storage ? storage->GetInteger() : 0;
			}
			virtual float GetValueFloat(const string & path) const override
			{
				auto storage = ResolveValueByPath(path);
				return storage ? storage->GetFloat() : 0.0f;
			}
			virtual bool GetValueBoolean(const string & path) const override
			{
				auto storage = ResolveValueByPath(path);
				return storage ? storage->GetBoolean() : false;
			}
			virtual string GetValueString(const string & path) const override
			{
				auto storage = ResolveValueByPath(path);
				return storage ? storage->GetString() : L"";
			}
			virtual int64 GetValueLongInteger(const string & path) const override
			{
				auto storage = ResolveValueByPath(path);
				return storage ? storage->GetLongInteger() : 0;
			}
			virtual double GetValueLongFloat(const string & path) const override
			{
				auto storage = ResolveValueByPath(path);
				return storage ? storage->GetLongFloat() : 0.0;
			}
			virtual Color GetValueColor(const string & path) const override
			{
				auto storage = ResolveValueByPath(path);
				return storage ? storage->GetColor() : Color(0);
			}
			virtual Time GetValueTime(const string & path) const override
			{
				auto storage = ResolveValueByPath(path);
				return storage ? storage->GetTime() : Time(0);
			}
			virtual void GetValueBinary(const string & path, void * buffer) const override
			{
				auto storage = ResolveValueByPath(path);
				if (storage) storage->GetBinary(buffer);
			}
			virtual int GetValueBinarySize(const string & path) const override
			{
				auto storage = ResolveValueByPath(path);
				return storage ? storage->GetBinarySize() : 0;
			}
			virtual void SetValue(const string & path, int32 value) override
			{
				auto storage = ResolveValueByPath(path);
				if (storage) storage->Set(value);
			}
			virtual void SetValue(const string & path, float value) override
			{
				auto storage = ResolveValueByPath(path);
				if (storage) storage->Set(value);
			}
			virtual void SetValue(const string & path, bool value) override
			{
				auto storage = ResolveValueByPath(path);
				if (storage) storage->Set(value);
			}
			virtual void SetValue(const string & path, const string & value) override
			{
				auto storage = ResolveValueByPath(path);
				if (storage) storage->Set(value);
			}
			virtual void SetValue(const string & path, const widechar * value) { SetValue(path, string(value)); }
			virtual void SetValue(const string & path, const char * value) override { SetValue(path, string(value)); }
			virtual void SetValue(const string & path, int64 value) override
			{
				auto storage = ResolveValueByPath(path);
				if (storage) storage->Set(value);
			}
			virtual void SetValue(const string & path, double value) override
			{
				auto storage = ResolveValueByPath(path);
				if (storage) storage->Set(value);
			}
			virtual void SetValue(const string & path, Color value) override
			{
				auto storage = ResolveValueByPath(path);
				if (storage) storage->Set(value);
			}
			virtual void SetValue(const string & path, Time value) override
			{
				auto storage = ResolveValueByPath(path);
				if (storage) storage->Set(value);
			}
			virtual void SetValue(const string & path, const void * value, int size) override
			{
				auto storage = ResolveValueByPath(path);
				if (storage) storage->Set(value, size);
			}

			static void ValidateMemoryRange(int size, int data_at, int of_length)
			{
				if (data_at < 0 || data_at > size || size - data_at < of_length) throw InvalidFormatException();
			}
			static void ValidateMemoryString(const uint8 * data, int size, int data_at)
			{
				if (data_at < 0 || data_at > size || size - data_at < 2) throw InvalidFormatException();
				int pos = data_at;
				while (size - pos >= 2 && *reinterpret_cast<const uint16 *>(data + pos)) pos += 2;
				if (size - data_at < 2) throw InvalidFormatException();
			}
			void FillFromMemoryData(const uint8 * data, int size, int data_at)
			{
				if (data_at < 0 || data_at > size || size - data_at < 8) throw InvalidFormatException();
				int32 NodeCount, ValueCount;
				NodeCount = *reinterpret_cast<const int32 *>(data + data_at);
				ValueCount = *reinterpret_cast<const int32 *>(data + data_at + 4);
				if (size - data_at < 8 + 8 * NodeCount + 4 * ValueCount) throw InvalidFormatException();
				for (int i = 0; i < NodeCount; i++) {
					int32 NodeOffset = *reinterpret_cast<const int32 *>(data + data_at + 8 + 4 * i);
					int32 NodeNameOffset = *reinterpret_cast<const int32 *>(data + data_at + 8 + 4 * NodeCount + 4 * i);
					ValidateMemoryString(data, size, NodeNameOffset);
					SafePointer<RegistryNodeImplementation> subnode = new RegistryNodeImplementation;
					subnode->FillFromMemoryData(data, size, NodeOffset);
					CreateRawNode(string(data + NodeNameOffset, -1, Encoding::UTF16), subnode);
				}
				for (int i = 0; i < ValueCount; i++) {
					int32 ValueOffset = *reinterpret_cast<const int32 *>(data + data_at + 8 + 8 * NodeCount + 4 * i);
					ValidateMemoryRange(size, ValueOffset, 8);
					int32 ValueNameOffset = *reinterpret_cast<const int32 *>(data + ValueOffset);
					int32 ValueTypeCode = *reinterpret_cast<const int32 *>(data + ValueOffset + 4);
					ValidateMemoryString(data, size, ValueNameOffset);
					int index = CreateRawValue(string(data + ValueNameOffset, -1, Encoding::UTF16), static_cast<RegistryValueType>(ValueTypeCode));
					auto & value = Values[index];
					if (value.type == RegistryValueType::Integer || value.type == RegistryValueType::Float || value.type == RegistryValueType::Boolean || value.type == RegistryValueType::Color) {
						ValidateMemoryRange(size, ValueOffset, 12);
						int32 short_value = *reinterpret_cast<const int32 *>(data + ValueOffset + 8);
						value.value_int32 = short_value;
					} else if (value.type == RegistryValueType::LongInteger || value.type == RegistryValueType::Time || value.type == RegistryValueType::LongFloat) {
						ValidateMemoryRange(size, ValueOffset, 16);
						int64 long_value = *reinterpret_cast<const int64 *>(data + ValueOffset + 8);
						value.value_int64 = long_value;
					} else if (value.type == RegistryValueType::String) {
						ValidateMemoryRange(size, ValueOffset, 12);
						int32 offset = *reinterpret_cast<const int32 *>(data + ValueOffset + 8);
						ValidateMemoryString(data, size, offset);
						value.Set(string(data + offset, -1, Encoding::UTF16));
					} else if (value.type == RegistryValueType::Binary) {
						ValidateMemoryRange(size, ValueOffset, 16);
						int32 size_bin = *reinterpret_cast<const int32 *>(data + ValueOffset + 8);
						int32 offset = *reinterpret_cast<const int32 *>(data + ValueOffset + 12);
						ValidateMemoryRange(size, offset, size_bin);
						value.Set(data + offset, size_bin);
					} else value.type = RegistryValueType::Unknown;
				}
			}
			void FillFromNode(const RegistryNodeImplementation * src)
			{
				for (int i = 0; i < src->NodeNames.Length(); i++) {
					NodeNames << src->NodeNames[i];
					RegistryNodeImplementation * node = new RegistryNodeImplementation;
					node->FillFromNode(src->Nodes.ElementAt(i));
					Nodes.Append(node);
					node->Release();
				}
				for (int i = 0; i < src->ValueNames.Length(); i++) {
					ValueNames << src->ValueNames[i];
					Values << src->Values[i];
				}
			}
			static void WriteString(Streaming::MemoryStream * stream, Array<RegistryStringOptimizationPair> & opt,
				const uint8 * string, int length, uint32 & result_offset)
			{
				for (int i = 0; i < opt.Length(); i++) {
					if (opt[i].length < length) continue;
					bool match = true;
					for (int j = 0; j < length; j++) {
						if (string[j] != reinterpret_cast<uint8 *>(stream->GetBuffer())[opt[i].base_offset + opt[i].length - length + j]) {
							match = false;
							break;
						}
					}
					if (match) {
						result_offset = opt[i].base_offset + opt[i].length - length;
						return;
					}
				}
				result_offset = uint32(stream->Seek(0, Streaming::Current));
				stream->Write(string, length);
				opt << RegistryStringOptimizationPair{ result_offset, length };
			}
			int Serialize(Streaming::MemoryStream * stream, Array<RegistryStringOptimizationPair> & opt) const
			{
				int node_base = int(stream->Seek(0, Streaming::Current));
				int32 node_count = NodeNames.Length();
				int32 value_count = ValueNames.Length();
				Array<uint32> node_data_offset(0x10);
				Array<uint32> node_name_offset(0x10);
				Array<uint32> value_data_offset(0x10);
				node_data_offset.SetLength(node_count);
				node_name_offset.SetLength(node_count);
				value_data_offset.SetLength(value_count);
				ZeroMemory(node_data_offset.GetBuffer(), node_count * 4);
				ZeroMemory(node_name_offset.GetBuffer(), node_count * 4);
				ZeroMemory(value_data_offset.GetBuffer(), value_count * 4);
				stream->Write(&node_count, 4);
				stream->Write(&value_count, 4);
				stream->Write(node_data_offset.GetBuffer(), node_count * 4);
				stream->Write(node_name_offset.GetBuffer(), node_count * 4);
				stream->Write(value_data_offset.GetBuffer(), value_count * 4);
				for (int i = 0; i < Nodes.Length(); i++) {
					node_data_offset[i] = Nodes[i].Serialize(stream, opt);
				}
				for (int i = 0; i < NodeNames.Length(); i++) {
					SafePointer< Array<uint8> > name = NodeNames[i].EncodeSequence(Encoding::UTF16, true);
					WriteString(stream, opt, name->GetBuffer(), name->Length(), node_name_offset[i]);
				}
				for (int i = 0; i < Values.Length(); i++) {
					SafePointer< Array<uint8> > name = ValueNames[i].EncodeSequence(Encoding::UTF16, true);
					uint32 name_offset = 0;
					WriteString(stream, opt, name->GetBuffer(), name->Length(), name_offset);
					uint32 data_offset = 0;
					int32 type = static_cast<int32>(Values[i].type);
					if (Values[i].type == RegistryValueType::Binary) {
						data_offset = int32(stream->Seek(0, Streaming::Current));
						stream->Write(Values[i].value_binary, uint32(Values[i].value_binary_size));
					} else if (Values[i].type == RegistryValueType::String) {
						if (Values[i].value_binary_size == 0) {
							uint16 word = 0;
							WriteString(stream, opt, reinterpret_cast<uint8 *>(&word), 2, data_offset);
						} else {
							WriteString(stream, opt, Values[i].value_binary, int(Values[i].value_binary_size), data_offset);
						}
					}
					value_data_offset[i] = int32(stream->Seek(0, Streaming::Current));
					stream->Write(&name_offset, 4);
					stream->Write(&type, 4);
					if (Values[i].type == RegistryValueType::Integer || Values[i].type == RegistryValueType::Float ||
						Values[i].type == RegistryValueType::Boolean || Values[i].type == RegistryValueType::Color) {
						stream->Write(&Values[i].value_int32, 4);
					} else if (Values[i].type == RegistryValueType::LongInteger || Values[i].type == RegistryValueType::Time ||
						Values[i].type == RegistryValueType::LongFloat) {
						stream->Write(&Values[i].value_int64, 8);
					} else if (Values[i].type == RegistryValueType::String) {
						stream->Write(&data_offset, 4);
					} else if (Values[i].type == RegistryValueType::Binary) {
						stream->Write(&Values[i].value_binary_size, 4);
						stream->Write(&data_offset, 4);
					}
				}
				auto rev = stream->Seek(0, Streaming::Current);
				stream->Seek(node_base + 8, Streaming::Begin);
				stream->Write(node_data_offset.GetBuffer(), node_count * 4);
				stream->Write(node_name_offset.GetBuffer(), node_count * 4);
				stream->Write(value_data_offset.GetBuffer(), value_count * 4);
				stream->Seek(rev, Streaming::Begin);
				return node_base;
			}
		};
		class RegistryImplementation : public Registry
		{
			SafePointer<RegistryNodeImplementation> Root;
		public:
			RegistryImplementation(void) { Root.SetReference(new RegistryNodeImplementation); }
			RegistryImplementation(RegistryNodeImplementation * node) { Root.SetReference(node); }
			virtual ~RegistryImplementation(void) override {}
			virtual Registry * Clone(void) const override { return CreateRegistryFromNode(Root); }
			virtual void Save(Streaming::Stream * stream) const override
			{
				Streaming::MemoryStream buffer(0x10000);
				Array<RegistryStringOptimizationPair> opt(0x100);
				int32 root_offset = Root->Serialize(&buffer, opt);
				EngineRegistryHeader hdr;
				MemoryCopy(hdr.Signature, "ecs.1.0", 8);
				hdr.SignatureEx = 0x80000004;
				hdr.Version = 0;
				hdr.DataOffset = sizeof(hdr);
				hdr.DataSize = int32(buffer.Length());
				hdr.RootOffset = root_offset;
				buffer.Seek(0, Streaming::Begin);
				stream->Write(&hdr, sizeof(hdr));
				buffer.CopyTo(stream);
			}

			virtual const Array<string>& GetSubnodes(void) const override { return Root->GetSubnodes(); }
			virtual const Array<string>& GetValues(void) const override { return Root->GetValues(); }
			virtual void CreateNode(const string & path) override { Root->CreateNode(path); }
			virtual void RemoveNode(const string & path) override { Root->RemoveNode(path); }
			virtual void RenameNode(const string & node, const string & name) override { Root->RenameNode(node, name); }
			virtual RegistryNode * OpenNode(const string & path) override { return Root->OpenNode(path); }
			virtual void CreateValue(const string & path, RegistryValueType type) override { Root->CreateValue(path, type); }
			virtual void RemoveValue(const string & path) override { Root->RemoveValue(path); }
			virtual void RenameValue(const string & name_from, const string & name_to) override { Root->RenameValue(name_from, name_to); }
			virtual RegistryValueType GetValueType(const string & path) const override { return Root->GetValueType(path); }
			virtual int32 GetValueInteger(const string & path) const override { return Root->GetValueInteger(path); }
			virtual float GetValueFloat(const string & path) const override { return Root->GetValueFloat(path); }
			virtual bool GetValueBoolean(const string & path) const override { return Root->GetValueBoolean(path); }
			virtual string GetValueString(const string & path) const override { return Root->GetValueString(path); }
			virtual int64 GetValueLongInteger(const string & path) const override { return Root->GetValueLongInteger(path); }
			virtual double GetValueLongFloat(const string & path) const override { return Root->GetValueLongFloat(path); }
			virtual Color GetValueColor(const string & path) const override { return Root->GetValueColor(path); }
			virtual Time GetValueTime(const string & path) const override { return Root->GetValueTime(path); }
			virtual void GetValueBinary(const string & path, void * buffer) const override { Root->GetValueBinary(path, buffer); }
			virtual int GetValueBinarySize(const string & path) const override { return Root->GetValueBinarySize(path); }
			virtual void SetValue(const string & path, int32 value) override { Root->SetValue(path, value); }
			virtual void SetValue(const string & path, float value) override { Root->SetValue(path, value); }
			virtual void SetValue(const string & path, bool value) override { Root->SetValue(path, value); }
			virtual void SetValue(const string & path, const string & value) override { Root->SetValue(path, value); }
			virtual void SetValue(const string & path, const widechar * value) override { Root->SetValue(path, value); }
			virtual void SetValue(const string & path, const char * value) override { Root->SetValue(path, value); }
			virtual void SetValue(const string & path, int64 value) override { Root->SetValue(path, value); }
			virtual void SetValue(const string & path, double value) override { Root->SetValue(path, value); }
			virtual void SetValue(const string & path, Color value) override { Root->SetValue(path, value); }
			virtual void SetValue(const string & path, Time value) override { Root->SetValue(path, value); }
			virtual void SetValue(const string & path, const void * value, int size) override { Root->SetValue(path, value, size); }
		};

		void ReplicateRegistry(RegistryNode * node_from, RegistryNode * node_to)
		{
			for (int i = 0; i < node_from->GetValues().Length(); i++) {
				auto & v = node_from->GetValues()[i];
				auto type = node_from->GetValueType(v);
				node_to->CreateValue(v, type);
				if (type == RegistryValueType::Integer) {
					node_to->SetValue(v, node_from->GetValueInteger(v));
				} else if (type == RegistryValueType::Float) {
					node_to->SetValue(v, node_from->GetValueFloat(v));
				} else if (type == RegistryValueType::Boolean) {
					node_to->SetValue(v, node_from->GetValueBoolean(v));
				} else if (type == RegistryValueType::String) {
					node_to->SetValue(v, node_from->GetValueString(v));
				} else if (type == RegistryValueType::LongInteger) {
					node_to->SetValue(v, node_from->GetValueLongInteger(v));
				} else if (type == RegistryValueType::LongFloat) {
					node_to->SetValue(v, node_from->GetValueLongFloat(v));
				} else if (type == RegistryValueType::Color) {
					node_to->SetValue(v, node_from->GetValueColor(v));
				} else if (type == RegistryValueType::Time) {
					node_to->SetValue(v, node_from->GetValueTime(v));
				} else if (type == RegistryValueType::Binary) {
					DataBlock block(0x100);
					block.SetLength(node_from->GetValueBinarySize(v));
					node_from->GetValueBinary(v, block.GetBuffer());
					node_to->SetValue(v, block.GetBuffer(), block.Length());
				}
			}
			for (int i = 0; i < node_from->GetSubnodes().Length(); i++) {
				auto & n = node_from->GetSubnodes()[i];
				SafePointer<RegistryNode> node = node_from->OpenNode(n);
				if (node) {
					node_to->CreateNode(n);
					SafePointer<RegistryNode> to = node_to->OpenNode(n);
					if (to) ReplicateRegistry(node, to);
				}
			}
		}
		Registry * CreateRegistry(void) { return new RegistryImplementation; }
		Registry * LoadRegistry(Streaming::Stream * source) noexcept
		{
			try {
				EngineRegistryHeader hdr;
				source->Read(&hdr, sizeof(hdr));
				if (MemoryCompare(hdr.Signature, "ecs.1.0", 8) != 0 || hdr.SignatureEx != 0x80000004 || hdr.Version != 0 || hdr.DataOffset > 0x7FFFFFFFU || hdr.DataSize > 0x7FFFFFFFU) throw InvalidFormatException();
				Array<uint8> data(hdr.DataSize);
				data.SetLength(hdr.DataSize);
				source->Seek(hdr.DataOffset, Streaming::Begin);
				source->Read(data.GetBuffer(), hdr.DataSize);
				SafePointer<RegistryNodeImplementation> root = new RegistryNodeImplementation;
				root->FillFromMemoryData(data.GetBuffer(), data.Length(), hdr.RootOffset);
				SafePointer<RegistryImplementation> reg = new RegistryImplementation(root);
				root->Retain();
				reg->Retain();
				return reg;
			} catch (...) { return 0; }
		}
		Registry * CreateRegistryFromNode(RegistryNode * node)
		{
			SafePointer<Registry> reg = CreateRegistry();
			ReplicateRegistry(node, reg);
			reg->Retain();
			return reg;
		}
		
		class RegistryMergedNodeImplementation : public RegistryNode
		{
			ObjectArray<RegistryNode> _nodes;
			Array<string> _cached_values;
			Array<string> _cached_nodes;
			void ResetCache(void)
			{
				for (auto & node : _nodes) {
					auto & vals = node.GetValues();
					auto & nodes = node.GetSubnodes();
					for (auto & e : vals) {
						bool present = false;
						for (auto & f : _cached_values) if (string::CompareIgnoreCase(f, e) == 0) { present = true; break; }
						if (!present) _cached_values.Append(e);
					}
					for (auto & e : nodes) {
						bool present = false;
						for (auto & f : _cached_nodes) if (string::CompareIgnoreCase(f, e) == 0) { present = true; break; }
						if (!present) _cached_nodes.Append(e);
					}
				}
			}
		public:
			RegistryMergedNodeImplementation(const ObjectArray<RegistryNode> & nodes) : _nodes(nodes), _cached_values(0x20), _cached_nodes(0x20) { ResetCache(); }
			virtual ~RegistryMergedNodeImplementation(void) override {}

			virtual const Array<string> & GetSubnodes(void) const override { return _cached_nodes; }
			virtual const Array<string> & GetValues(void) const override { return _cached_values; }
			virtual void CreateNode(const string & path) override { throw Exception(); }
			virtual void RemoveNode(const string & path) override { throw Exception(); }
			virtual void RenameNode(const string & node, const string & name) override { throw Exception(); }
			virtual RegistryNode * OpenNode(const string & path) override
			{
				ObjectArray<RegistryNode> new_nodes(0x10);
				for (auto & node : _nodes) {
					SafePointer<RegistryNode> local = node.OpenNode(path);
					if (local) new_nodes.Append(local);
				}
				if (new_nodes.Length()) return new RegistryMergedNodeImplementation(new_nodes);
				else return 0;
			}
			virtual void CreateValue(const string & path, RegistryValueType type) override { throw Exception(); }
			virtual void RemoveValue(const string & path) override { throw Exception(); }
			virtual void RenameValue(const string & name_from, const string & name_to) override { throw Exception(); }
			virtual RegistryValueType GetValueType(const string & path) const override
			{
				for (auto & node : _nodes) {
					auto type = node.GetValueType(path);
					if (type != RegistryValueType::Unknown) return type;
				}
				return RegistryValueType::Unknown;
			}
			virtual int32 GetValueInteger(const string & path) const override
			{
				for (auto & node : _nodes) {
					auto type = node.GetValueType(path);
					if (type != RegistryValueType::Unknown) return node.GetValueInteger(path);
				}
				return 0;
			}
			virtual float GetValueFloat(const string & path) const override
			{
				for (auto & node : _nodes) {
					auto type = node.GetValueType(path);
					if (type != RegistryValueType::Unknown) return node.GetValueFloat(path);
				}
				return 0.0f;
			}
			virtual bool GetValueBoolean(const string & path) const override
			{
				for (auto & node : _nodes) {
					auto type = node.GetValueType(path);
					if (type != RegistryValueType::Unknown) return node.GetValueBoolean(path);
				}
				return false;
			}
			virtual string GetValueString(const string & path) const override
			{
				for (auto & node : _nodes) {
					auto type = node.GetValueType(path);
					if (type != RegistryValueType::Unknown) return node.GetValueString(path);
				}
				return L"";
			}
			virtual int64 GetValueLongInteger(const string & path) const override
			{
				for (auto & node : _nodes) {
					auto type = node.GetValueType(path);
					if (type != RegistryValueType::Unknown) return node.GetValueLongInteger(path);
				}
				return 0;
			}
			virtual double GetValueLongFloat(const string & path) const override
			{
				for (auto & node : _nodes) {
					auto type = node.GetValueType(path);
					if (type != RegistryValueType::Unknown) return node.GetValueLongFloat(path);
				}
				return 0.0;
			}
			virtual Color GetValueColor(const string & path) const override
			{
				for (auto & node : _nodes) {
					auto type = node.GetValueType(path);
					if (type != RegistryValueType::Unknown) return node.GetValueColor(path);
				}
				return 0;
			}
			virtual Time GetValueTime(const string & path) const override
			{
				for (auto & node : _nodes) {
					auto type = node.GetValueType(path);
					if (type != RegistryValueType::Unknown) return node.GetValueTime(path);
				}
				return 0;
			}
			virtual void GetValueBinary(const string & path, void * buffer) const override
			{
				for (auto & node : _nodes) {
					auto type = node.GetValueType(path);
					if (type != RegistryValueType::Unknown) { node.GetValueBinary(path, buffer); return; }
				}
			}
			virtual int GetValueBinarySize(const string & path) const override
			{
				for (auto & node : _nodes) {
					auto type = node.GetValueType(path);
					if (type != RegistryValueType::Unknown) return node.GetValueBinarySize(path);
				}
				return 0;
			}
			virtual void SetValue(const string & path, int32 value) override { throw Exception(); }
			virtual void SetValue(const string & path, float value) override { throw Exception(); }
			virtual void SetValue(const string & path, bool value) override { throw Exception(); }
			virtual void SetValue(const string & path, const string & value) override { throw Exception(); }
			virtual void SetValue(const string & path, const widechar * value) override { throw Exception(); }
			virtual void SetValue(const string & path, const char * value) override { throw Exception(); }
			virtual void SetValue(const string & path, int64 value) override { throw Exception(); }
			virtual void SetValue(const string & path, double value) override { throw Exception(); }
			virtual void SetValue(const string & path, Color value) override { throw Exception(); }
			virtual void SetValue(const string & path, Time value) override { throw Exception(); }
			virtual void SetValue(const string & path, const void * value, int size) override { throw Exception(); }
		};
		RegistryNode * CreateMergedNode(const ObjectArray<RegistryNode> & nodes) { return new RegistryMergedNodeImplementation(nodes); }
	}
}