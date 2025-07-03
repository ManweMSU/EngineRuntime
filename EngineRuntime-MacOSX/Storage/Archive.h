#pragma once

#include "../Streaming.h"
#include "Chain.h"

namespace Engine
{
	namespace Storage
	{
		typedef int ArchiveFile;
		enum class ArchiveStream { Native, ForceDecompressed, MetadataBased };
		enum class ArchiveMetadataUsage { LoadMetadata, IgnoreMetadata };
		namespace NewArchiveFlags
		{
			enum NewArchiveFormat { UseFormat64 = 0, UseFormat32 = 1 };
			enum NewArchiveMetadata { NoMetadata = 0, CreateMetadata = 2 };
		};
		class Archive : public Object
		{
		public:
			virtual int GetFileCount(void) = 0;
			virtual ArchiveFile FindArchiveFile(const string & name) = 0;
			virtual ArchiveFile FindArchiveFile(const widechar * type, uint32 file_id) = 0;
			virtual Streaming::Stream * QueryFileStream(ArchiveFile file) = 0;
			virtual Streaming::Stream * QueryFileStream(ArchiveFile file, ArchiveStream stream) = 0;
			virtual string GetFileName(ArchiveFile file) = 0;
			virtual string GetFileType(ArchiveFile file) = 0;
			virtual uint32 GetFileID(ArchiveFile file) = 0;
			virtual uint32 GetFileCustomData(ArchiveFile file) = 0;
			virtual bool HasMetadata(void) = 0;
			virtual Time GetFileCreationTime(ArchiveFile file) = 0;
			virtual Time GetFileAccessTime(ArchiveFile file) = 0;
			virtual Time GetFileAlterTime(ArchiveFile file) = 0;
			virtual bool IsFileCompressed(ArchiveFile file) = 0;
			virtual string GetFileAttribute(ArchiveFile file, const string & key) = 0;
			virtual Array<string> * GetFileAttributes(ArchiveFile file) = 0;
		};
		class NewArchive : public Object
		{
		public:
			virtual int GetFileCount(void) = 0;
			virtual bool IsLongFormat(void) = 0;
			virtual bool HasMetadata(void) = 0;
			virtual void SetFileName(ArchiveFile file, const string & name) = 0;
			virtual void SetFileType(ArchiveFile file, const string & type) = 0;
			virtual void SetFileID(ArchiveFile file, uint32 id) = 0;
			virtual void SetFileCustom(ArchiveFile file, uint32 custom) = 0;
			virtual void SetFileCreationTime(ArchiveFile file, Time time) = 0;
			virtual void SetFileAccessTime(ArchiveFile file, Time time) = 0;
			virtual void SetFileAlterTime(ArchiveFile file, Time time) = 0;
			virtual void SetFileAttribute(ArchiveFile file, const string & key, const string & value) = 0;
			virtual void SetFileData(ArchiveFile file, const void * data, int length) = 0;
			virtual void SetFileData(ArchiveFile file, Streaming::Stream * source) = 0;
			virtual void SetFileData(ArchiveFile file, Streaming::Stream * source, MethodChain chain, CompressionQuality quality, ThreadPool * pool = 0) = 0;
			virtual void SetFileData(ArchiveFile file, Streaming::Stream * source, MethodChain chain, CompressionQuality quality, ThreadPool * pool, uint32 block_size) = 0;
			virtual void SetFileCompressionFlag(ArchiveFile file, bool flag) = 0;
			virtual void Finalize(void) = 0;
		};
		Archive * OpenArchive(Streaming::Stream * at) noexcept;
		Archive * OpenArchive(Streaming::Stream * at, ArchiveMetadataUsage metadata_usage) noexcept;
		NewArchive * CreateArchive(Streaming::Stream * at, int num_files, uint flags = NewArchiveFlags::UseFormat64 | NewArchiveFlags::CreateMetadata);
	}
}