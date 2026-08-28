#ifndef GXEFFECT_H
#define GXEFFECT_H

#include <d3d9.h>
#include <d3dx9.h>
#include <string>
#include <unordered_map>

class gxGraphics;

class gxEffect {
public:
    gxEffect(gxGraphics* gfx, ID3DXEffect* effect);
    ~gxEffect();

    void onLostDevice();
    void onResetDevice();

    bool setFloat(const std::string& name, float value);
    bool setVector(const std::string& name, const float vec[4]);
    bool setMatrix(const std::string& name, const D3DXMATRIX& mat);
    void setMatrixBySemantic(const char* semantic, const D3DXMATRIX& mat);
    void setAutoMatrices(const D3DXMATRIX& world, const D3DXMATRIX& view, const D3DXMATRIX& proj);
    bool setTexture(const std::string& name, IDirect3DBaseTexture9* tex);

    bool begin(UINT* passes);
    bool beginPass(UINT pass);
    bool endPass();
    bool end();

    ID3DXEffect* getEffect() const { return effect; }

private:
    gxGraphics* graphics;
    ID3DXEffect* effect;
    std::unordered_map<std::string, D3DXHANDLE> handleCache;

    D3DXHANDLE getHandle(const std::string& name);
};

#endif