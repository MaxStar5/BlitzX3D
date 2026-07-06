#include "std.h"
#include "gxeffect.h"
#include "gxgraphics.h"

gxEffect::gxEffect(gxGraphics* gfx, ID3DXEffect* e)
    : graphics(gfx), effect(e) {
    effect->AddRef();
}

gxEffect::~gxEffect() {
    if (effect) effect->Release();
}

void gxEffect::onLostDevice() {
    if (effect) effect->OnLostDevice();
}

void gxEffect::onResetDevice() {
    if (effect) effect->OnResetDevice();
}

D3DXHANDLE gxEffect::getHandle(const std::string& name) {
    auto it = handleCache.find(name);
    if (it != handleCache.end()) return it->second;
    D3DXHANDLE h = effect->GetParameterByName(nullptr, name.c_str());
    handleCache[name] = h;
    return h;
}

bool gxEffect::setFloat(const std::string& name, float value) {
    D3DXHANDLE h = getHandle(name);
    if (!h) return false;
    return SUCCEEDED(effect->SetFloat(h, value));
}

bool gxEffect::setVector(const std::string& name, const float vec[4]) {
    D3DXHANDLE h = getHandle(name);
    if (!h) return false;
    return SUCCEEDED(effect->SetFloatArray(h, vec, 4));
}

bool gxEffect::setMatrix(const std::string& name, const D3DXMATRIX& mat) {
    D3DXHANDLE h = getHandle(name);
    if (!h) return false;
    return SUCCEEDED(effect->SetMatrix(h, &mat));
}

bool gxEffect::setTexture(const std::string& name, IDirect3DBaseTexture9* tex) {
    D3DXHANDLE h = getHandle(name);
    if (!h) return false;
    return SUCCEEDED(effect->SetTexture(h, tex));
}

void gxEffect::setAutoMatrices(const D3DXMATRIX& world,
    const D3DXMATRIX& view,
    const D3DXMATRIX& proj) {
    D3DXMATRIX wv = world * view;
    D3DXMATRIX wvp = wv * proj;

    setMatrix("World", world);
    setMatrix("View", view);
    setMatrix("Projection", proj);
    setMatrix("WorldView", wv);
    setMatrix("WorldViewProj", wvp);
}

bool gxEffect::begin(UINT* passes) {
    return SUCCEEDED(effect->Begin(passes, 0));
}

bool gxEffect::beginPass(UINT pass) {
    return SUCCEEDED(effect->BeginPass(pass));
}

bool gxEffect::endPass() {
    return SUCCEEDED(effect->EndPass());
}

bool gxEffect::end() {
    return SUCCEEDED(effect->End());
}