#include "ui/theme.h"

namespace Theme {

	namespace {

		constexpr const char* THEME_RESOURCE_PATH = ":/theme/source2_tools.qss";

		// Source 2 tools palette — game/core/tools/stylesheets/qtabbedstyle_colors.vsc.
		// The values are copied as is so the tool looks like the native Workshop Manager.
		QPalette BuildSource2ToolsPalette() {
			QPalette palette;
			const QColor windowColor(54, 54, 54);
			const QColor baseColor(38, 38, 39);
			const QColor alternateBaseColor(55, 55, 55);
			const QColor textColor(182, 182, 183);
			const QColor inactiveTextColor(152, 152, 153);
			const QColor disabledTextColor(106, 106, 108);
			const QColor brightTextColor(200, 200, 200);
			const QColor buttonColor(73, 73, 73);
			const QColor highlightColor(79, 82, 89);
			const QColor linkColor(96, 127, 193);
			const QColor toolTipBaseColor(198, 198, 198);

			palette.setColor(QPalette::Window, windowColor);
			palette.setColor(QPalette::WindowText, textColor);
			palette.setColor(QPalette::Base, baseColor);
			palette.setColor(QPalette::AlternateBase, alternateBaseColor);
			palette.setColor(QPalette::Text, textColor);
			palette.setColor(QPalette::BrightText, brightTextColor);
			palette.setColor(QPalette::Button, buttonColor);
			palette.setColor(QPalette::ButtonText, textColor);
			palette.setColor(QPalette::Light, QColor(110, 110, 110));
			palette.setColor(QPalette::Midlight, QColor(91, 91, 91));
			palette.setColor(QPalette::Mid, QColor(58, 58, 58));
			palette.setColor(QPalette::Dark, QColor(42, 42, 42));
			palette.setColor(QPalette::Shadow, Qt::black);
			palette.setColor(QPalette::Highlight, highlightColor);
			palette.setColor(QPalette::HighlightedText, Qt::white);
			palette.setColor(QPalette::Link, linkColor);
			palette.setColor(QPalette::LinkVisited, linkColor);
			palette.setColor(QPalette::ToolTipBase, toolTipBaseColor);
			palette.setColor(QPalette::ToolTipText, Qt::black);
			palette.setColor(QPalette::PlaceholderText, QColor(182, 182, 183, 128));

			palette.setColor(QPalette::Inactive, QPalette::WindowText, inactiveTextColor);
			palette.setColor(QPalette::Inactive, QPalette::Text, inactiveTextColor);
			palette.setColor(QPalette::Inactive, QPalette::Highlight, QColor(66, 69, 76));
			palette.setColor(QPalette::Inactive, QPalette::HighlightedText, inactiveTextColor);

			palette.setColor(QPalette::Disabled, QPalette::Text, disabledTextColor);
			palette.setColor(QPalette::Disabled, QPalette::ButtonText, inactiveTextColor);
			palette.setColor(QPalette::Disabled, QPalette::WindowText, inactiveTextColor);
			return palette;
		}

	} // namespace

	void Apply(QApplication& application) {
		// Fusion is the only Qt style that honestly honours the palette and QSS on Windows.
		application.setStyle(QStyleFactory::create("Fusion"));
		application.setPalette(BuildSource2ToolsPalette());

		QFile themeFile(THEME_RESOURCE_PATH);
		if (!themeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
			LogMessage(LOG_WARN, "Theme stylesheet %s not found in resources\n", THEME_RESOURCE_PATH);
			return;
		}
		application.setStyleSheet(QString::fromUtf8(themeFile.readAll()));
	}

} // namespace Theme
