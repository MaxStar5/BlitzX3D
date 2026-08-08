#include "stdafx.h"
#include "flamegraph.h"
#include "profiler.h"
#include "prefs.h"
#include <algorithm>

IMPLEMENT_DYNAMIC(FlameGraphPanel, CWnd)

BEGIN_MESSAGE_MAP(FlameGraphPanel, CWnd)
	ON_WM_SIZE()
	ON_WM_PAINT()
	ON_WM_MOUSEMOVE()
	ON_WM_MOUSELEAVE()
END_MESSAGE_MAP()

FlameGraphPanel::FlameGraphPanel() : profiler(nullptr), totalSamples(0), maxDepth(0), lastBuildTime(0) {
	tooltipText.reserve(256);
}

FlameGraphPanel::~FlameGraphPanel() {
}

BOOL FlameGraphPanel::PreCreateWindow(CREATESTRUCT& cs) {
	static CString cls;
	if (cls.IsEmpty()) {
		cls = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, 0, (HBRUSH)(COLOR_WINDOW + 1), 0);
	}
	cs.lpszClass = cls;
	cs.style |= WS_CLIPCHILDREN;
	return CWnd::PreCreateWindow(cs);
}

void FlameGraphPanel::setProfiler(const Profiler* prof) {
	profiler = prof;
	lastBuildTime = 0;
	refresh();
}

void FlameGraphPanel::refresh() {
	if (!IsWindow(m_hWnd)) return;
	if (profiler) {
		DWORD now = GetTickCount();
		if (now - lastBuildTime > 500) {
			buildTree();
			lastBuildTime = now;
		}
	}
	else {
		root.reset();
		totalSamples = 0;
		maxDepth = 0;
	}
	Invalidate();
}

void FlameGraphPanel::OnSize(UINT nType, int cx, int cy) {
	CWnd::OnSize(nType, cx, cy);
	GetClientRect(&clientRect);
}

void FlameGraphPanel::OnPaint() {
	if (!IsWindow(m_hWnd)) return;
	CPaintDC dc(this);
	CRect rect;
	GetClientRect(&rect);
	dc.FillSolidRect(rect, prefs.rgb_bkgrnd);

	if (!root || root->children.empty()) {
		dc.SetTextColor(prefs.rgb_default);
		dc.SetBkMode(TRANSPARENT);
		dc.DrawText(L"No samples collected", rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		return;
	}

	CDC memDC;
	memDC.CreateCompatibleDC(&dc);
	CBitmap bmp;
	bmp.CreateCompatibleBitmap(&dc, rect.Width(), rect.Height());
	memDC.SelectObject(&bmp);
	memDC.FillSolidRect(rect, prefs.rgb_bkgrnd);

	int yOffset = 20;
	int availableHeight = rect.Height() - yOffset;
	int depthHeight = maxDepth > 0 ? availableHeight / maxDepth : 20;
	if (depthHeight < 5) depthHeight = 5;

	int xPos = 0;
	int totalWidth = rect.Width();
	for (auto& child : root->children) {
		int childWidth = (int)((double)child->samples / totalSamples * totalWidth);
		if (childWidth < 1) childWidth = 1;
		buildRectTree(child.get(), 0, xPos, yOffset, childWidth);
		xPos += childWidth;
	}

	for (auto& child : root->children) {
		drawNode(memDC, child.get(), 0);
	}

	dc.BitBlt(0, 0, rect.Width(), rect.Height(), &memDC, 0, 0, SRCCOPY);

	if (!tooltipText.empty()) {
		CRect ttRect;
		GetClientRect(&ttRect);
		CFont* oldFont = dc.SelectObject(&prefs.debugFont);
		dc.SetBkColor(prefs.rgb_bkgrnd);
		dc.SetTextColor(prefs.rgb_default);
		dc.SetBkMode(OPAQUE);
		dc.DrawText(tooltipText.c_str(), ttRect, DT_LEFT | DT_TOP | DT_NOPREFIX);
		dc.SelectObject(oldFont);
	}
}

void FlameGraphPanel::buildTree() {
	if (!profiler) {
		root.reset();
		totalSamples = 0;
		maxDepth = 0;
		return;
	}

	auto samples = profiler->getStackSamples(); // copy
	if (samples.empty()) {
		root.reset();
		totalSamples = 0;
		maxDepth = 0;
		return;
	}

	root.reset(new RectNode("root", 0));
	totalSamples = 0;
	maxDepth = 0;

	for (const auto& stack : samples) {
		RectNode* current = root.get();
		int depth = 0;
		for (const std::string& func : stack) {
			RectNode* child = nullptr;
			for (auto& c : current->children) {
				if (c->name == func) {
					child = c.get();
					break;
				}
			}
			if (!child) {
				std::unique_ptr<RectNode> newNode(new RectNode(func, 0));
				newNode->parent = current;
				child = newNode.get();
				current->children.push_back(std::move(newNode));
			}
			current = child;
			++depth;
		}
		if (depth > maxDepth) maxDepth = depth;
		++current->samples;
		++totalSamples;
	}
}

void FlameGraphPanel::buildRectTree(RectNode* node, int depth, int& x, int y, int width) {
	node->rect = CRect(x, y, x + width, y + 20);
	if (node->children.empty()) return;
	int childWidth = width / (int)node->children.size();
	if (childWidth < 1) childWidth = 1;
	int childX = x;
	for (auto& child : node->children) {
		buildRectTree(child.get(), depth + 1, childX, y + 20, childWidth);
		childX += childWidth;
	}
}

COLORREF FlameGraphPanel::getColor(const std::string& name) {
	auto it = colorMap.find(name);
	if (it != colorMap.end()) return it->second;
	static int hue = 0;
	hue += 50;
	if (hue >= 360) hue %= 360;
	int r, g, b;
	double h = hue / 360.0;
	double s = 0.7, v = 0.8;
	int i = (int)(h * 6);
	double f = h * 6 - i;
	double p = v * (1 - s);
	double q = v * (1 - f * s);
	double t = v * (1 - (1 - f) * s);
	switch (i % 6) {
	case 0: r = (int)(v * 255); g = (int)(t * 255); b = (int)(p * 255); break;
	case 1: r = (int)(q * 255); g = (int)(v * 255); b = (int)(p * 255); break;
	case 2: r = (int)(p * 255); g = (int)(v * 255); b = (int)(t * 255); break;
	case 3: r = (int)(p * 255); g = (int)(q * 255); b = (int)(v * 255); break;
	case 4: r = (int)(t * 255); g = (int)(p * 255); b = (int)(v * 255); break;
	case 5: r = (int)(v * 255); g = (int)(p * 255); b = (int)(q * 255); break;
	}
	COLORREF color = RGB(r, g, b);
	colorMap[name] = color;
	return color;
}

void FlameGraphPanel::drawNode(CDC& dc, RectNode* node, int depth) {
	CRect r = node->rect;
	if (r.Width() < 2) r.right = r.left + 2;
	COLORREF color = getColor(node->name);
	dc.FillSolidRect(r, color);
	dc.DrawEdge(r, EDGE_RAISED, BF_RECT);
	if (r.Width() > 50) {
		dc.SetTextColor(RGB(0, 0, 0));
		dc.SetBkMode(TRANSPARENT);
		CString label(node->name.c_str());
		dc.DrawText(label, r, DT_LEFT | DT_TOP | DT_NOPREFIX | DT_END_ELLIPSIS);
	}
	for (auto& child : node->children) {
		drawNode(dc, child.get(), depth + 1);
	}
}

void FlameGraphPanel::OnMouseMove(UINT nFlags, CPoint point) {
	CWnd::OnMouseMove(nFlags, point);
	hoverPos = point;
	showTooltip(point);
}

void FlameGraphPanel::OnMouseLeave() {
	hideTooltip();
}

void FlameGraphPanel::showTooltip(const CPoint& pt) {
	if (!root) return;
	RectNode* current = root.get();
	RectNode* found = nullptr;
	while (current) {
		bool hitChild = false;
		for (auto& child : current->children) {
			if (child->rect.PtInRect(pt)) {
				found = child.get();
				current = child.get();
				hitChild = true;
				break;
			}
		}
		if (!hitChild) break;
	}
	if (found) {
		char buf[256];
		sprintf(buf, "%s: %d samples (%.1f%%)", found->name.c_str(), found->samples, 100.0 * found->samples / totalSamples);
		tooltipText = buf;
	}
	else {
		tooltipText.clear();
	}
	Invalidate();
}

void FlameGraphPanel::hideTooltip() {
	tooltipText.clear();
	Invalidate();
}
