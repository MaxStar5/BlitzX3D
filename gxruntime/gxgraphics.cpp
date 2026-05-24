#include "std.h"
#include "gxgraphics.h"
#include "gxruntime.h"
#include "../gxruntime/gxutf8.h"

extern gxRuntime* gx_runtime;
static Debugger* debugger;

gxGraphics::gxGraphics(gxRuntime* rt, IDirect3DDevice8* dev, IDirect3DSurface8* front, IDirect3DSurface8* back, bool d3d) : runtime(rt), dir3dDev(dev), frontBuffer(front), backBuffer(back), gfx_lost(false), dummy_mesh(0) {

	if (dir3dDev) dir3dDev->AddRef();
	if (frontBuffer) frontBuffer->AddRef();
	if (backBuffer) backBuffer->AddRef();

	front_canvas = new gxCanvas(this, frontBuffer, 0);
	MessageBoxA(NULL, "front_canvas created", "Debug", MB_OK);

	if (!backBuffer) {
		MessageBoxA(NULL, "backBuffer is NULL!", "Error", MB_OK);
		return;
	}

	D3DSURFACE_DESC testDesc;
	HRESULT hr = backBuffer->GetDesc(&testDesc);
	if (FAILED(hr)) {
		char buf[256];
		sprintf(buf, "backBuffer->GetDesc failed: 0x%08X", hr);
		MessageBoxA(NULL, buf, "Error", MB_OK);
	}

	back_canvas = new gxCanvas(this, backBuffer, 0);
	MessageBoxA(NULL, "back_canvas created", "Debug", MB_OK);

	front_canvas->cls();
	back_canvas->cls();

	FT_Init_FreeType(&ftLibrary);

	HMODULE ntdllModule = GetModuleHandleW(L"ntdll.dll");
	running_on_wine = ntdllModule && GetProcAddress(ntdllModule, "wine_get_version");

	def_font = running_on_wine ? nullptr : this->loadFont(UTF8::getSystemFontFile("Courier"), 12);
	front_canvas->setFont(def_font);
	back_canvas->setFont(def_font);

	D3DCAPS8 caps;
	if (dir3dDev && SUCCEEDED(dir3dDev->GetDeviceCaps(&caps))) {
		// simple for now, we should probably enumerate!!
		zbuffFmt = D3DFMT_D16;
	}
	else {
		zbuffFmt = D3DFMT_UNKNOWN;
	}

	// todo: gamma
}

gxGraphics::~gxGraphics() {
	if (_gamma) _gamma->Release();
	while (scene_set.size()) freeScene(*scene_set.begin());
	while (movie_set.size()) closeMovie(*movie_set.begin());
	while (font_set.size()) freeFont(*font_set.begin());
	while (canvas_set.size()) freeCanvas(*canvas_set.begin());
	/*
	std::set<std::set<std::any>*>::iterator custom_set_it;
	for (custom_set_it = custom_set.begin(); custom_set_it != custom_set.end(); ++custom_set_it) {
		while ((*custom_set_it)->size()) {
			(*custom_set_it)->erase((*custom_set_it)->begin());
			delete* custom_set_it;
		}
	}
	*/

	for (auto it = font_res.begin(); it != font_res.end(); ++it) RemoveFontResource((*it).c_str());
	font_res.clear();

	delete back_canvas;
	delete front_canvas;

	FT_Done_FreeType(ftLibrary);

	if (dir3dDev) dir3dDev->Release();
	if (frontBuffer && frontBuffer != backBuffer) frontBuffer->Release();
	if (backBuffer) backBuffer->Release();
}

void gxGraphics::setGamma(int r, int g, int b, float dr, float dg, float db) {
	_gammaRamp.red[r & 255] = dr * 257.0f;
	_gammaRamp.green[g & 255] = dg * 257.0f;
	_gammaRamp.blue[b & 255] = db * 257.0f;
}

void gxGraphics::updateGamma(bool calibrate) {
	if (!_gamma) return;
	_gamma->SetGammaRamp(calibrate ? DDSGR_CALIBRATE : 0, &_gammaRamp);
}

void gxGraphics::getGamma(int r, int g, int b, float* dr, float* dg, float* db) {
	*dr = _gammaRamp.red[r & 255] / 257.0f;
	*dg = _gammaRamp.green[g & 255] / 257.0f;
	*db = _gammaRamp.blue[b & 255] / 257.0f;
}

bool gxGraphics::restore() {
	if (!dir3dDev) return false;

	HRESULT hr = dir3dDev->TestCooperativeLevel();
	if (hr == D3DERR_DEVICELOST) return false;

	if (hr == D3DERR_DEVICENOTRESET) {
		if (present_params.Windowed) {
			present_params.Flags |= D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
		}

		hr = dir3dDev->Reset(&present_params);
		if (FAILED(hr)) return false;

		IDirect3DSurface8* newBack = nullptr;
		hr = dir3dDev->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &newBack);
		if (FAILED(hr) || !newBack) return false;

		if (runtime->backBuffer) runtime->backBuffer->Release();
		runtime->backBuffer = newBack;
		newBack->AddRef();

		if (back_canvas && back_canvas->surf) {
			back_canvas->surf->Release();
			back_canvas->surf = newBack;
			newBack->AddRef();
		}
		if (front_canvas && front_canvas->surf == back_canvas->surf) {
			front_canvas->surf->Release();
			front_canvas->surf = newBack;
			newBack->AddRef();
		}

		for (auto it = canvas_set.begin(); it != canvas_set.end(); ++it) {
			(*it)->restore();
		}

		for (auto it = mesh_set.begin(); it != mesh_set.end(); ++it) {
			(*it)->restore();
		}

		for (auto font : font_set) {
			for (auto atlas : font->atlases) {
				atlas->restore();
			}
			if (font->tempCanvas) {
				font->tempCanvas->restore();
			}
		}

		InvalidateRect(runtime->hwnd, nullptr, FALSE);
	}
	return true;
}

gxCanvas* gxGraphics::getFrontCanvas()const {
	return front_canvas;
}

gxCanvas* gxGraphics::getBackCanvas()const {
	return back_canvas;
}

gxFont* gxGraphics::getDefaultFont()const {
	return def_font;
}

void gxGraphics::vwait() { // stubby stbu stub
	// dirDraw->WaitForVerticalBlank(DDWAITVB_BLOCKBEGIN, 0);
}

void gxGraphics::flip(bool v) {
	runtime->flip(v);
	if (frontBuffer != backBuffer && dir3dDev) {
		dir3dDev->CopyRects(backBuffer, nullptr, 0, frontBuffer, nullptr);
	}
}

void gxGraphics::copy(gxCanvas* dest, int dx, int dy, int dw, int dh, gxCanvas* src, int sx, int sy, int sw, int sh) {
	RECT r = { dx,dy,dx + dw,dy + dh };
	ddUtil::copy(dest->getSurface(), dx, dy, dw, dh, src->getSurface(), sx, sy, sw, sh);
	dest->damage(r);
}

int gxGraphics::getScanLine() const { return 0; }

int gxGraphics::getAvailVidmem() const { return 0; }

int gxGraphics::getTotalVidmem() const { return 0; }

gxMovie* gxGraphics::openMovie(const std::string& file, int flags) {
	return 0;
}

gxMovie* gxGraphics::verifyMovie(gxMovie* m) {
	return movie_set.count(m) ? m : 0;
}

void gxGraphics::closeMovie(gxMovie* m) {
	if (movie_set.erase(m)) delete m;
}

gxCanvas* gxGraphics::createCanvas(int w, int h, int flags) {
	IDirect3DTexture8* tex = ddUtil::createSurface(w, h, flags, this);
	if (!tex) return 0;
	gxCanvas* c = new gxCanvas(this, tex, flags);
	canvas_set.insert(c);
	c->cls();
	return c;
}

gxCanvas* gxGraphics::loadCanvas(const std::string& f, int flags) {
	IDirect3DTexture8* tex = ddUtil::loadSurface(f, flags, this);
	if (!tex) return 0;
	gxCanvas* c = new gxCanvas(this, tex, flags);
	canvas_set.insert(c);
	return c;
}

gxCanvas* gxGraphics::verifyCanvas(gxCanvas* c) {
	return canvas_set.count(c) || c == front_canvas || c == back_canvas ? c : 0;
}

void gxGraphics::freeCanvas(gxCanvas* c) {
	if (canvas_set.erase(c)) delete c;
}

int gxGraphics::getWidth()const {
	return front_canvas->getWidth();
}

int gxGraphics::getHeight()const {
	return front_canvas->getHeight();
}

int gxGraphics::getDepth()const {
	return front_canvas->getDepth();
}

gxFont* gxGraphics::loadFont(std::string f, int height, bool bold, bool italic, bool underlined) {
	std::string t;
	int n = f.find('.');
	if (n == std::string::npos) {
		t = fullfilename(f);
		if (!font_res.count(t) && AddFontResource(t.c_str())) font_res.insert(t);
		t = filenamefile(f.substr(0, n));
	}
	else {
		t = f;
	}

	gxFont* newFont = new gxFont(ftLibrary, this, f, height, bold, italic, underlined);
	font_set.emplace(newFont);
	return newFont;
}

gxFont* gxGraphics::verifyFont(gxFont* f) {
	return font_set.count(f) ? f : 0;
}

void gxGraphics::freeFont(gxFont* f) {
	if (font_set.erase(f)) delete f;
}

//////////////
// 3D STUFF //
//////////////

gxScene* gxGraphics::createScene(int flags) {
	if (scene_set.size()) return 0;
	if (!dir3dDev) return 0;

	D3DFORMAT depthFormats[] = { D3DFMT_D24S8, D3DFMT_D24X8, D3DFMT_D16, D3DFMT_D32 };
	bool zOk = false;
	for (int i = 0; i < 4; ++i) {
		zbuffFmt = depthFormats[i];
		if (back_canvas->attachZBuffer()) {
			zOk = true;
			break;
		}
	}
	if (!zOk) {
		MessageBoxA(NULL, "createScene: Failed to attach any Z-buffer", "Error", MB_OK);
		return 0;
	}

	gxScene* scene = new gxScene(this, back_canvas);
	scene_set.insert(scene);
	return scene;
}

gxScene* gxGraphics::verifyScene(gxScene* s) { return scene_set.count(s) ? s : 0; }

void gxGraphics::freeScene(gxScene* scene) {
	if (!scene_set.erase(scene)) return;
	dummy_mesh = 0;
	while (mesh_set.size()) freeMesh(*mesh_set.begin());
	back_canvas->releaseZBuffer();
	delete scene;
}

void gxGraphics::adoptCanvas(gxCanvas* c) {
	canvas_set.insert(c);
}

gxMesh* gxGraphics::createMesh(int max_verts, int max_tris, int flags) {
	static const DWORD VTXFMT = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX2 |
		D3DFVF_TEXCOORDSIZE2(0) | D3DFVF_TEXCOORDSIZE2(1);
	DWORD usage = D3DUSAGE_WRITEONLY;
	IDirect3DVertexBuffer8* vb = nullptr;
	if (FAILED(dir3dDev->CreateVertexBuffer(max_verts * sizeof(gxMesh::dxVertex), usage, VTXFMT, D3DPOOL_MANAGED, &vb)))
		return nullptr;
	IDirect3DIndexBuffer8* ib = nullptr;
	if (FAILED(dir3dDev->CreateIndexBuffer(max_tris * 3 * sizeof(WORD), usage, D3DFMT_INDEX16, D3DPOOL_MANAGED, &ib))) {
		vb->Release();
		return nullptr;
	}
	gxMesh* mesh = new gxMesh(this, vb, ib, max_verts, max_tris);
	mesh_set.insert(mesh);
	return mesh;
}

gxMesh* gxGraphics::verifyMesh(gxMesh* m) {
	return mesh_set.count(m) ? m : 0;
}

void gxGraphics::freeMesh(gxMesh* mesh) {
	if (mesh_set.erase(mesh)) delete mesh;
}