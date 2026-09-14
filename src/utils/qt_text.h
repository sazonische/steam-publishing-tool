#pragma once

namespace QtText {
	inline QString FromStringView(std::string_view text) {
		return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
	}
} // namespace QtText
