#pragma once

#include "core/addon_library.h"

// Byte distribution of the included files, not upload progress.
class CContentSizeBar : public QWidget {
	Q_OBJECT

public:
	explicit CContentSizeBar(QWidget* parent = nullptr);
	void SetManifest(const AddonManifest& manifest);
	QSize sizeHint() const override;

protected:
	void paintEvent(QPaintEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void leaveEvent(QEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;
	void focusOutEvent(QFocusEvent* event) override;

private:
	struct Segment {
		QString extension;
		uint64_t size{0};
		size_t files{0};
		QColor color;
	};

	QString DescribeSegment(int index) const;
	double SegmentWidth(uint64_t size) const;
	void SelectSegment(int index);

	std::vector<Segment> _segments;
	uint64_t _totalSize{0};
	int _selectedSegment{-1};
};
