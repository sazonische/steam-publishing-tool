#include "ui/content_size_bar.h"

#include <QtCore/QLocale>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>

#include <algorithm>
#include <array>
#include <map>

namespace {
	constexpr int BAR_HEIGHT = 22;
	// Colours sampled from the original Workshop Manager content bar.
	constexpr std::array<const char*, 9> SEGMENT_COLORS = {"#996ac8", "#6a6ac8", "#6a99c8", "#6ac8c8", "#6ac899", "#6ac86a", "#99c86a", "#c8c86a", "#c8996a"};
} // namespace

CContentSizeBar::CContentSizeBar(QWidget* parent) : QWidget(parent) {
	setMouseTracking(true);
	setFocusPolicy(Qt::StrongFocus);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	setAccessibleName(tr("Upload content by file type"));
	setMinimumWidth(200);
}

QSize CContentSizeBar::sizeHint() const {
	return QSize(400, BAR_HEIGHT + fontMetrics().height() + 10);
}

void CContentSizeBar::SetManifest(const AddonManifest& manifest) {
	_segments.clear();
	_totalSize = 0;
	_selectedSegment = -1;
	std::map<QString, Segment> groups;
	if (manifest.errorMessage.empty()) {
		for (const AddonFileEntry& file : manifest.files) {
			const QString extension = QString::fromStdWString(file.relativePath.extension().wstring()).toLower();
			Segment& segment = groups[extension];
			segment.extension = extension.isEmpty() ? tr("No extension") : extension;
			segment.size += file.size;
			++segment.files;
			_totalSize += file.size;
		}
	}
	for (auto& [extension, segment] : groups) {
		_segments.push_back(std::move(segment));
	}
	std::ranges::sort(_segments, [](const Segment& left, const Segment& right) {
		return left.size != right.size ? left.size < right.size : left.extension < right.extension;
	});
	QStringList descriptions;
	for (int index = 0; index < static_cast<int>(_segments.size()); ++index) {
		_segments[static_cast<size_t>(index)].color = QColor(SEGMENT_COLORS[static_cast<size_t>(index) % SEGMENT_COLORS.size()]);
		descriptions.append(DescribeSegment(index));
	}
	setToolTip(descriptions.isEmpty() ? QString() : descriptions.join('\n') + tr("\nSmall file types have a minimum visible width; percentages show their actual size."));
	setAccessibleDescription(tr("Use the left and right arrow keys to browse file types. ") + descriptions.join(QStringLiteral("; ")));
	setVisible(!_segments.empty());
	update();
}

QString CContentSizeBar::DescribeSegment(int index) const {
	const Segment& segment = _segments[static_cast<size_t>(index)];
	const double percent = _totalSize == 0 ? 0.0 : 100.0 * static_cast<double>(segment.size) / static_cast<double>(_totalSize);
	return tr("%1 · %n file(s) · %2 · %3%", nullptr, static_cast<int>(segment.files))
		.arg(segment.extension, QLocale().formattedDataSize(static_cast<qint64>(segment.size)), QLocale().toString(percent, 'f', 2));
}

double CContentSizeBar::SegmentWidth(uint64_t size) const {
	if (_segments.empty() || _totalSize == 0) {
		return 0.0;
	}
	const double minimumWidth = std::min(10.0, static_cast<double>(width()) / _segments.size());
	const double remainingWidth = std::max(0.0, width() - minimumWidth * _segments.size());
	return minimumWidth + remainingWidth * static_cast<double>(size) / static_cast<double>(_totalSize);
}

void CContentSizeBar::paintEvent(QPaintEvent* event) {
	Q_UNUSED(event);
	QPainter painter(this);
	const QRectF bar(0, 0, width(), BAR_HEIGHT);
	painter.fillRect(bar, QColor("#262627"));
	double left = 0.0;
	for (int index = 0; index < static_cast<int>(_segments.size()) && _totalSize > 0; ++index) {
		const Segment& segment = _segments[static_cast<size_t>(index)];
		const double segmentWidth = SegmentWidth(segment.size);
		QColor color = _selectedSegment == index ? segment.color.lighter(128) : segment.color;
		if (!isEnabled()) {
			color = QColor("#555555");
		}
		painter.fillRect(QRectF(left, 0, segmentWidth, BAR_HEIGHT), color);
		left += segmentWidth;
	}

	if (hasFocus()) {
		painter.setPen(QPen(QColor("#91b8df"), 1));
		painter.drawRect(bar.adjusted(0.5, 0.5, -0.5, -0.5));
	}
	QString text;
	if (_selectedSegment >= 0) {
		text = DescribeSegment(_selectedSegment);
	} else {
		size_t files = 0;
		for (const Segment& segment : _segments) {
			files += segment.files;
		}
		text = tr("%n file(s) · %1 total", nullptr, static_cast<int>(files)).arg(QLocale().formattedDataSize(static_cast<qint64>(_totalSize)));
	}
	painter.setPen(QColor("#b6b6b7"));
	painter.drawText(QRect(0, BAR_HEIGHT + 6, width(), fontMetrics().height()), Qt::AlignLeft | Qt::AlignVCenter, fontMetrics().elidedText(text, Qt::ElideRight, width()));
}

void CContentSizeBar::SelectSegment(int index) {
	if (_selectedSegment != index) {
		_selectedSegment = index;
		update();
	}
}

void CContentSizeBar::mouseMoveEvent(QMouseEvent* event) {
	int selected = -1;
	if (_totalSize > 0 && event->position().y() < BAR_HEIGHT) {
		double right = 0.0;
		for (int index = 0; index < static_cast<int>(_segments.size()); ++index) {
			right += SegmentWidth(_segments[static_cast<size_t>(index)].size);
			if (event->position().x() < right) {
				selected = index;
				break;
			}
		}
	}
	SelectSegment(selected);
}

void CContentSizeBar::leaveEvent(QEvent* event) {
	QWidget::leaveEvent(event);
	SelectSegment(-1);
}

void CContentSizeBar::keyPressEvent(QKeyEvent* event) {
	if (!_segments.empty() && (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right)) {
		const int count = static_cast<int>(_segments.size());
		SelectSegment(_selectedSegment < 0 ? (event->key() == Qt::Key_Right ? 0 : count - 1) : (_selectedSegment + (event->key() == Qt::Key_Right ? 1 : count - 1)) % count);
		event->accept();
		return;
	}
	QWidget::keyPressEvent(event);
}

void CContentSizeBar::focusOutEvent(QFocusEvent* event) {
	QWidget::focusOutEvent(event);
	SelectSegment(-1);
}
