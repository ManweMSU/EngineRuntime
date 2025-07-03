#pragma once

#include "../EngineBase.h"
#include "../Streaming.h"
#include "../Graphics/GraphicsBase.h"

namespace Engine
{
	namespace Storage
	{
		enum class RegistryValueType { Unknown = 0,
			Integer = 1, Float = 2, Boolean = 3, String = 4,
			LongInteger = 5, LongFloat = 6, Color = 7, Time = 8, Binary = 9};
		class RegistryNode : public Object
		{
		public:
			virtual const Array<string> & GetSubnodes(void) const = 0;
			virtual const Array<string> & GetValues(void) const = 0;

			virtual void CreateNode(const string & path) = 0;
			virtual void RemoveNode(const string & path) = 0;
			virtual void RenameNode(const string & node, const string & name) = 0;
			virtual RegistryNode * OpenNode(const string & path) = 0;

			virtual void CreateValue(const string & path, RegistryValueType type) = 0;
			virtual void RemoveValue(const string & path) = 0;
			virtual void RenameValue(const string & name_from, const string & name_to) = 0;
			virtual RegistryValueType GetValueType(const string & path) const = 0;

			virtual int32 GetValueInteger(const string & path) const = 0;
			virtual float GetValueFloat(const string & path) const = 0;
			virtual bool GetValueBoolean(const string & path) const = 0;
			virtual string GetValueString(const string & path) const = 0;
			virtual int64 GetValueLongInteger(const string & path) const = 0;
			virtual double GetValueLongFloat(const string & path) const = 0;
			virtual Color GetValueColor(const string & path) const = 0;
			virtual Time GetValueTime(const string & path) const = 0;
			virtual void GetValueBinary(const string & path, void * buffer) const = 0;
			virtual int GetValueBinarySize(const string & path) const = 0;

			virtual void SetValue(const string & path, int32 value) = 0;
			virtual void SetValue(const string & path, float value) = 0;
			virtual void SetValue(const string & path, bool value) = 0;
			virtual void SetValue(const string & path, const string & value) = 0;
			virtual void SetValue(const string & path, const widechar * value) = 0;
			virtual void SetValue(const string & path, const char * value) = 0;
			virtual void SetValue(const string & path, int64 value) = 0;
			virtual void SetValue(const string & path, double value) = 0;
			virtual void SetValue(const string & path, Color value) = 0;
			virtual void SetValue(const string & path, Time value) = 0;
			virtual void SetValue(const string & path, const void * value, int size) = 0;
		};
		class Registry : public RegistryNode
		{
		public:
			virtual Registry * Clone(void) const = 0;
			virtual void Save(Streaming::Stream * stream) const = 0;
		};

		Registry * CreateRegistry(void);
		Registry * LoadRegistry(Streaming::Stream * source) noexcept;
		Registry * CreateRegistryFromNode(RegistryNode * node);

		RegistryNode * CreateMergedNode(const ObjectArray<RegistryNode> & nodes);
	}
}