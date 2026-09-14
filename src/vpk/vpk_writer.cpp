#include "vpk/vpk_writer.h"

#include "vpk/crc32.h"

#include <QtCore/QCryptographicHash>

#include <blake3.h>

#include <array>
#include <format>
#include <fstream>
#include <map>
#include <ranges>
#include <system_error>

namespace VpkWriter {

	namespace {

		constexpr uint32_t VPK_SIGNATURE = 0x55AA1234;
		constexpr uint32_t VPK_VERSION = 2;
		constexpr uint16_t ENTRY_TERMINATOR = 0xFFFF;
		constexpr size_t COPY_BUFFER_SIZE = 1024 * 1024;
		constexpr size_t CHUNK_HASH_SIZE = 16;
		// ChunkHashFraction_t::EHashType in the engine: MD5 = 0, BLAKE3 = 1. cs2_workshop_manager writes
		// BLAKE3 and the game refuses the VPK as corrupt when the chunk hashes do not verify.
		constexpr uint16_t CHUNK_HASH_TYPE_BLAKE3 = 1;
		// An empty path and an empty extension are encoded as a space in the VPK tree.
		constexpr const char* EMPTY_TREE_NAME = " ";
		// The _dir.vpk tail of Valve's unsigned workshop VPKs: signature, a one and 12 zeros.
		constexpr std::array<uint8_t, 20> SIGNATURE_SECTION = {0x34, 0x12, 0xAA, 0x55, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

		struct TreeEntry {
			std::string extension;
			std::string directory;
			std::string name;
			uint32_t crc{0};
			uint16_t archiveIndex{0};
			uint32_t offset{0};
			uint32_t length{0};
		};

		template<typename T>
		void AppendValue(std::string& buffer, T value) {
			buffer.append(reinterpret_cast<const char*>(&value), sizeof(value));
		}

		void AppendString(std::string& buffer, const std::string& text) {
			buffer.append(text);
			buffer.push_back('\0');
		}

		// path/to/file.ext -> ("path/to", "file", "ext"). Root files and no extension — a space.
		void SplitTreePath(const std::string& relativePath, std::string& directory, std::string& name, std::string& extension) {
			const size_t slash = relativePath.rfind('/');
			directory = slash == std::string::npos ? EMPTY_TREE_NAME : relativePath.substr(0, slash);
			const std::string fileName = slash == std::string::npos ? relativePath : relativePath.substr(slash + 1);
			const size_t dot = fileName.rfind('.');
			if (dot == std::string::npos || dot == 0) {
				name = fileName;
				extension = EMPTY_TREE_NAME;
			} else {
				name = fileName.substr(0, dot);
				extension = fileName.substr(dot + 1);
			}
		}

		std::filesystem::path ArchivePath(const std::filesystem::path& outputDirectory, const std::string& baseName, uint32_t archiveIndex) {
			return outputDirectory / std::format("{}_{:03d}.vpk", baseName, archiveIndex);
		}

		bool RemoveStaleVpks(const std::filesystem::path& outputDirectory, const std::string& baseName, std::string& errorMessage) {
			std::error_code errorCode;
			for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(outputDirectory, errorCode)) {
				const std::string fileName = entry.path().filename().string();
				if (!entry.is_regular_file() || !fileName.starts_with(baseName + "_") || !fileName.ends_with(".vpk")) {
					continue;
				}
				if (!std::filesystem::remove(entry.path(), errorCode)) {
					errorMessage = std::format("cannot delete existing {}: {}", entry.path().string(), errorCode.message());
					return false;
				}
			}
			return true;
		}

		// The tree is grouped by extension, then by folder; first-appearance order, like Valve.
		std::string BuildTree(const std::vector<TreeEntry>& entries) {
			std::vector<std::string> extensionOrder;
			std::map<std::string, std::vector<std::string>> directoryOrderByExtension;
			std::map<std::string, std::map<std::string, std::vector<const TreeEntry*>>> grouped;

			for (const TreeEntry& entry : entries) {
				if (!grouped.contains(entry.extension)) {
					extensionOrder.push_back(entry.extension);
				}
				std::map<std::string, std::vector<const TreeEntry*>>& byDirectory = grouped[entry.extension];
				if (!byDirectory.contains(entry.directory)) {
					directoryOrderByExtension[entry.extension].push_back(entry.directory);
				}
				byDirectory[entry.directory].push_back(&entry);
			}

			// cs2_workshop_manager walks its tree from the last inserted node to the first at every level,
			// so extensions, directories and names all come out in reverse order of first appearance.
			std::string tree;
			for (const std::string& extension : extensionOrder | std::views::reverse) {
				AppendString(tree, extension);
				for (const std::string& directory : directoryOrderByExtension[extension] | std::views::reverse) {
					AppendString(tree, directory);
					for (const TreeEntry* entry : grouped[extension][directory] | std::views::reverse) {
						AppendString(tree, entry->name);
						AppendValue(tree, entry->crc);
						AppendValue<uint16_t>(tree, 0); // preload bytes
						AppendValue(tree, entry->archiveIndex);
						AppendValue(tree, entry->offset);
						AppendValue(tree, entry->length);
						AppendValue(tree, ENTRY_TERMINATOR);
					}
					tree.push_back('\0');
				}
				tree.push_back('\0');
			}
			tree.push_back('\0');
			return tree;
		}

		QByteArray Md5(const char* data, size_t size) {
			return QCryptographicHash::hash(QByteArrayView(data, static_cast<qsizetype>(size)), QCryptographicHash::Md5);
		}

		// The first 16 bytes of the BLAKE3 output — BLAKE3 is an XOF, so a 16-byte digest is a prefix of the 32-byte one.
		std::array<uint8_t, CHUNK_HASH_SIZE> Blake3Truncated(const char* data, size_t size) {
			blake3_hasher hasher;
			blake3_hasher_init(&hasher);
			blake3_hasher_update(&hasher, data, size);
			std::array<uint8_t, CHUNK_HASH_SIZE> digest{};
			blake3_hasher_finalize(&hasher, digest.data(), digest.size());
			return digest;
		}

		// Archive checksum section: one ChunkHashFraction_t per hashChunkSize piece, laid out as in
		// Valve's files: u16 archive index, u16 hash type, u32 offset, u32 length, 16 bytes of BLAKE3.
		// Verified against cs2_workshop_manager output: identical for every chunk of a 167-chunk map.
		bool BuildChunkHashSection(const std::filesystem::path& outputDirectory, const std::string& baseName, uint32_t archiveCount, uint32_t chunkSize, std::string& section, std::string& errorMessage) {
			std::vector<char> buffer(chunkSize);
			for (uint32_t archiveIndex = 0; archiveIndex < archiveCount; ++archiveIndex) {
				std::ifstream archive(ArchivePath(outputDirectory, baseName, archiveIndex), std::ios::binary);
				if (!archive) {
					errorMessage = std::format("cannot reopen archive {}", archiveIndex);
					return false;
				}
				uint32_t offset = 0;
				while (archive) {
					archive.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
					const auto readCount = static_cast<uint32_t>(archive.gcount());
					if (readCount == 0) {
						break;
					}
					AppendValue<uint16_t>(section, static_cast<uint16_t>(archiveIndex));
					AppendValue<uint16_t>(section, CHUNK_HASH_TYPE_BLAKE3);
					AppendValue(section, offset);
					AppendValue(section, readCount);
					const std::array<uint8_t, CHUNK_HASH_SIZE> digest = Blake3Truncated(buffer.data(), readCount);
					section.append(reinterpret_cast<const char*>(digest.data()), digest.size());
					offset += readCount;
				}
			}
			return true;
		}

	} // namespace

	std::optional<WriteResult> Write(
		const std::filesystem::path& addonDirectory,
		const AddonManifest& manifest,
		const std::filesystem::path& outputDirectory,
		const std::string& baseName,
		const WriteOptions& options,
		const ProgressCallback& progress,
		std::string& errorMessage
	) {
		if (!manifest.errorMessage.empty()) {
			errorMessage = manifest.errorMessage;
			return std::nullopt;
		}
		if (manifest.files.empty()) {
			errorMessage = "nothing to pack: the manifest is empty";
			return std::nullopt;
		}

		std::error_code errorCode;
		std::filesystem::create_directories(outputDirectory, errorCode);
		if (errorCode) {
			errorMessage = std::format("cannot create {}: {}", outputDirectory.string(), errorCode.message());
			return std::nullopt;
		}
		if (!RemoveStaleVpks(outputDirectory, baseName, errorMessage)) {
			return std::nullopt;
		}

		WriteResult result;
		std::vector<TreeEntry> entries;
		entries.reserve(manifest.files.size());
		std::vector<char> buffer(COPY_BUFFER_SIZE);

		uint32_t archiveIndex = 0;
		uint64_t archiveSize = 0;
		uint64_t bytesDone = 0;
		std::ofstream archive(ArchivePath(outputDirectory, baseName, archiveIndex), std::ios::binary | std::ios::trunc);
		if (!archive) {
			errorMessage = std::format("cannot create archive 0 in {}", outputDirectory.string());
			return std::nullopt;
		}
		result.writtenFiles.push_back(ArchivePath(outputDirectory, baseName, archiveIndex));

		for (size_t fileIndex = 0; fileIndex < manifest.files.size(); ++fileIndex) {
			const AddonFileEntry& file = manifest.files[fileIndex];
			const std::filesystem::path sourcePath = addonDirectory / file.relativePath;

			std::ifstream source(sourcePath, std::ios::binary);
			if (!source) {
				errorMessage = std::format("cannot read {}", sourcePath.string());
				return std::nullopt;
			}
			if (archiveSize >= options.archiveSizeLimit) {
				archive.close();
				++archiveIndex;
				archiveSize = 0;
				archive.open(ArchivePath(outputDirectory, baseName, archiveIndex), std::ios::binary | std::ios::trunc);
				if (!archive) {
					errorMessage = std::format("cannot create archive {}", archiveIndex);
					return std::nullopt;
				}
				result.writtenFiles.push_back(ArchivePath(outputDirectory, baseName, archiveIndex));
			}

			TreeEntry entry;
			SplitTreePath(file.relativePath.generic_string(), entry.directory, entry.name, entry.extension);
			entry.archiveIndex = static_cast<uint16_t>(archiveIndex);
			entry.offset = static_cast<uint32_t>(archiveSize);

			uint32_t crc = 0;
			uint64_t written = 0;
			while (source) {
				source.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
				const auto readCount = static_cast<size_t>(source.gcount());
				if (readCount == 0) {
					break;
				}
				crc = Crc32::Update(crc, std::span<const std::byte>(reinterpret_cast<const std::byte*>(buffer.data()), readCount));
				archive.write(buffer.data(), static_cast<std::streamsize>(readCount));
				if (!archive) {
					errorMessage = std::format("cannot write to archive {} (disk full?)", archiveIndex);
					return std::nullopt;
				}
				written += readCount;
				bytesDone += readCount;
				if (progress && !progress(fileIndex, manifest.files.size(), bytesDone, manifest.totalSize)) {
					errorMessage = WRITE_CANCELLED_MESSAGE;
					return std::nullopt;
				}
			}
			if (source.bad() || written != file.size) {
				errorMessage = std::format("{} could not be read completely or changed since the file list was built; refresh and try again", file.relativePath.generic_string());
				return std::nullopt;
			}
			if (written > 0xFFFFFFFFULL) {
				errorMessage = std::format("{} is larger than 4 GB, VPK cannot store it", file.relativePath.generic_string());
				return std::nullopt;
			}

			entry.crc = crc;
			entry.length = static_cast<uint32_t>(written);
			archiveSize += written;
			entries.push_back(std::move(entry));
		}
		archive.close();
		result.archiveCount = archiveIndex + 1;
		result.totalBytes = bytesDone;
		if (progress && !progress(manifest.files.size(), manifest.files.size(), bytesDone, manifest.totalSize)) {
			errorMessage = WRITE_CANCELLED_MESSAGE;
			return std::nullopt;
		}

		const std::string tree = BuildTree(entries);
		std::string chunkHashSection;
		if (!BuildChunkHashSection(outputDirectory, baseName, result.archiveCount, options.hashChunkSize, chunkHashSection, errorMessage)) {
			return std::nullopt;
		}

		std::string header;
		AppendValue(header, VPK_SIGNATURE);
		AppendValue(header, VPK_VERSION);
		AppendValue(header, static_cast<uint32_t>(tree.size()));
		AppendValue<uint32_t>(header, 0); // no data in _dir
		AppendValue(header, static_cast<uint32_t>(chunkHashSection.size()));
		AppendValue<uint32_t>(header, 48);
		AppendValue(header, static_cast<uint32_t>(SIGNATURE_SECTION.size()));

		// OtherMD5 stays plain MD5 (verified against Valve's files): the tree, the chunk hash section and
		// the "whole file" = everything before this checksum.
		const QByteArray treeMd5 = Md5(tree.data(), tree.size());
		const QByteArray archiveSectionMd5 = Md5(chunkHashSection.data(), chunkHashSection.size());
		std::string wholeFile = header + tree + chunkHashSection;
		wholeFile.append(treeMd5.constData(), 16);
		wholeFile.append(archiveSectionMd5.constData(), 16);
		const QByteArray wholeFileMd5 = Md5(wholeFile.data(), wholeFile.size());

		const std::filesystem::path directoryPath = outputDirectory / std::format("{}_dir.vpk", baseName);
		std::ofstream directoryFile(directoryPath, std::ios::binary | std::ios::trunc);
		if (!directoryFile) {
			errorMessage = std::format("cannot create {}", directoryPath.string());
			return std::nullopt;
		}
		directoryFile.write(wholeFile.data(), static_cast<std::streamsize>(wholeFile.size()));
		directoryFile.write(wholeFileMd5.constData(), 16);
		directoryFile.write(reinterpret_cast<const char*>(SIGNATURE_SECTION.data()), static_cast<std::streamsize>(SIGNATURE_SECTION.size()));
		if (!directoryFile) {
			errorMessage = std::format("cannot write {}", directoryPath.string());
			return std::nullopt;
		}
		result.writtenFiles.insert(result.writtenFiles.begin(), directoryPath);

		LogMessage(LOG_INFO, "VPK written: %s (%zu files, %u archive(s), %llu bytes)\n", directoryPath.string().c_str(), entries.size(), result.archiveCount, static_cast<unsigned long long>(result.totalBytes));
		return result;
	}

} // namespace VpkWriter
