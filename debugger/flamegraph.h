#ifndef FLAMEGRAPH_H
#define FLAMEGRAPH_H

#include "stdafx.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

class Profiler;

class FlameGraphPanel : public CWnd {
	DECLARE_DYNAMIC(FlameGraphPanel)
public:
	FlameGraphPanel();
	virtual ~FlameGraphPanel();

	void setProfiler(const Profiler* prof);
	void refresh();

protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs) override;

	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnPaint();
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnMouseLeave();

	DECLARE_MESSAGE_MAP()

private:
	struct RectNode {
		std::string name;
		int samples;
		CRect rect;
		std::vector<std::unique_ptr<RectNode>> children;
		RectNode* parent;
		RectNode(const std::string& n, int s) : name(n), samples(s), parent(nullptr) {}
	};

	const Profiler* profiler;
	std::unique_ptr<RectNode> root;
	CRect clientRect;
	int totalSamples;
	int maxDepth;
	std::unordered_map<std::string, int> colorMap;
	CPoint hoverPos;
	std::string tooltipText;
	DWORD lastBuildTime;

	void buildTree();
	void buildRectTree(RectNode* node, int depth, int& x, int y, int width);
	COLORREF getColor(const std::string& name);
	void drawNode(CDC& dc, RectNode* node, int depth);
	void showTooltip(const CPoint& pt);
	void hideTooltip();
};

#endif
