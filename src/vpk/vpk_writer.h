#pragma once

#include "core/addon_library.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

// VPK v2 writer in the cs2_workshop_manager layout: <base>_dir.vpk with only the tree and
// checksums, data in <base>_000.vpk, <base>_001.vpk… in pieces of about 100 MiB.
namespace VpkWriter {

	struct WriteOptions {
		// Like Valve: a file is appended to the current archive, and if the archive is then at or above
		// the limit, the next file opens a new one. So an archive may slightly exceed the limit.
		uint64_t archiveSizeLimit{100ULL * 1024 * 1024};
		uint32_t hashChunkSize{1024 * 1024}; // one BLAKE3-128 entry per this many archive bytes
	};

	struct WriteResult {
		uint32_t archiveCount{0};
		uint64_t totalBytes{0};
		std::vector<std::filesystem::path> writtenFiles;
	};

	// Return false to abort; Write then fails with WRITE_CANCELLED_MESSAGE and leaves the partial archives behind.
	using ProgressCallback = std::function<bool(size_t filesDone, size_t filesTotal, uint64_t bytesDone, uint64_t bytesTotal)>;
	constexpr const char* WRITE_CANCELLED_MESSAGE = "cancelled";

	// Old <base>_*.vpk in outputDirectory are removed: otherwise a tail from the previous publish
	// would sit next to the new chunks and Steam would upload it along with the folder.
	std::optional<WriteResult> Write(
		const std::filesystem::path& addonDirectory,
		const AddonManifest& manifest,
		const std::filesystem::path& outputDirectory,
		const std::string& baseName,
		const WriteOptions& options,
		const ProgressCallback& progress,
		std::string& errorMessage
	);

} // namespace VpkWriter
