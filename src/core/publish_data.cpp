#include "core/publish_data.h"

namespace PublishDataFile {

	namespace {

		std::string EscapeValue(const std::string& value) {
			std::string escaped;
			escaped.reserve(value.size());
			for (const char character : value) {
				if (character == '"' || character == '\\') {
					escaped.push_back('\\');
				}
				escaped.push_back(character);
			}
			return escaped;
		}

	} // namespace

	bool Write(const std::filesystem::path& directory, const PublishData& publishData, std::string& errorMessage) {
		// Valve's format: "MM/DD/YYYY HH:MM:SS " with a trailing space — reproduced as is.
		const QString readableTime = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(publishData.publishTime)).toString("MM/dd/yyyy HH:mm:ss ");

		const std::string text = std::format(
			"\"publish_data\"\n{{\n\t\"title\"\t\t\"{}\"\n\t\"source_folder\"\t\t\"{}\"\n\t\"publish_time\"\t\t\"{}\"\n\t\"publish_time_readable\"\t\t\"{}\"\n}}\n",
			EscapeValue(publishData.title),
			EscapeValue(publishData.sourceFolder),
			publishData.publishTime,
			readableTime.toStdString()
		);

		const std::filesystem::path filePath = directory / FILE_NAME;
		QSaveFile saveFile(QString::fromStdWString(filePath.wstring()));
		if (!saveFile.open(QIODevice::WriteOnly)) {
			errorMessage = std::format("cannot open {} for writing: {}", PathText::ToUtf8(filePath), saveFile.errorString().toStdString());
			return false;
		}
		saveFile.write(text.data(), static_cast<qint64>(text.size()));
		if (!saveFile.commit()) {
			errorMessage = std::format("cannot write {}: {}", PathText::ToUtf8(filePath), saveFile.errorString().toStdString());
			return false;
		}
		return true;
	}

} // namespace PublishDataFile
