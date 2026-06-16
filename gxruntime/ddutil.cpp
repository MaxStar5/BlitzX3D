#include "std.h"
#include "ddutil.h"
#include "asmcoder.h"
#include "gxcanvas.h"
#include "gxgraphics.h"
#include "gxruntime.h"

extern gxRuntime* gx_runtime;

#include "../freeimage/freeimage.h"

static AsmCoder asm_coder;

static thread_local std::string g_lastImageError;

const std::string& ddUtil::getLastImageError() {
    return g_lastImageError;
}

PixelFormat::~PixelFormat() {
    if (plot_code) VirtualFree(plot_code, 0, MEM_RELEASE);
}

void PixelFormat::setFormat(D3DFORMAT fmt) {
    if (plot_code) VirtualFree(plot_code, 0, MEM_RELEASE);
    plot_code = (char*)VirtualAlloc(0, 128, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    point_code = plot_code + 64;

    switch (fmt) {
    case D3DFMT_A8R8G8B8:
        depth = 32; amask = 0xff000000; rmask = 0x00ff0000; gmask = 0x0000ff00; bmask = 0x000000ff; break;
    case D3DFMT_X8R8G8B8:
        depth = 32; amask = 0;          rmask = 0x00ff0000; gmask = 0x0000ff00; bmask = 0x000000ff; break;
    case D3DFMT_R5G6B5:
        depth = 16; amask = 0;          rmask = 0xf800;     gmask = 0x07e0;     bmask = 0x001f;     break;
    case D3DFMT_A1R5G5B5:
        depth = 16; amask = 0x8000;     rmask = 0x7c00;     gmask = 0x03e0;     bmask = 0x001f;     break;
    case D3DFMT_A4R4G4B4:
        depth = 16; amask = 0xf000;     rmask = 0x0f00;     gmask = 0x00f0;     bmask = 0x000f;     break;
    default:
        depth = 32; amask = 0xff000000; rmask = 0x00ff0000; gmask = 0x0000ff00; bmask = 0x000000ff; break;
    }

    pitch = depth / 8;
    argbfill = 0;
    if (!amask) argbfill |= 0xff000000;
    if (!rmask) argbfill |= 0x00ff0000;
    if (!gmask) argbfill |= 0x0000ff00;
    if (!bmask) argbfill |= 0x000000ff;

    calcShifts(amask, &ashr, &ashl); ashr += 24;
    calcShifts(rmask, &rshr, &rshl); rshr += 16;
    calcShifts(gmask, &gshr, &gshl); gshr += 8;
    calcShifts(bmask, &bshr, &bshl);
    plot = (Plot)(void*)plot_code;
    point = (Point)(void*)point_code;
    asm_coder.CodePlot(plot_code, depth, amask, rmask, gmask, bmask);
    asm_coder.CodePoint(point_code, depth, amask, rmask, gmask, bmask);
}

static void adjustTexSize(int* width, int* height, IDirect3DDevice8* dev) {
    D3DCAPS8 caps;
    if (FAILED(dev->GetDeviceCaps(&caps))) { *width = *height = 256; return; }

    int w = *width, h = *height;

    for (w = 1; w < *width; w <<= 1) {}
    for (h = 1; h < *height; h <<= 1) {}

    if (caps.TextureCaps & D3DPTEXTURECAPS_SQUAREONLY) {
        if (w > h) h = w; else w = h;
    }

    if (int maxAsp = caps.MaxTextureAspectRatio) {
        int asp = w > h ? w / h : h / w;
        if (asp > maxAsp) { if (w > h) h = w / maxAsp; else w = h / maxAsp; }
    }

    if (caps.MaxTextureWidth && w > (int)caps.MaxTextureWidth)  w = caps.MaxTextureWidth;
    if (caps.MaxTextureHeight && h > (int)caps.MaxTextureHeight) h = caps.MaxTextureHeight;
    *width = w; *height = h;
}

void ddUtil::buildMipMaps(IDirect3DTexture8* tex) {
    if (!tex) return;
    DWORD levels = tex->GetLevelCount();
    if (levels <= 1) return;

    for (DWORD mip = 0; mip + 1 < levels; ++mip) {
        D3DLOCKED_RECT src_lr, dst_lr;
        D3DSURFACE_DESC src_desc, dst_desc;
        tex->GetLevelDesc(mip, &src_desc);
        tex->GetLevelDesc(mip + 1, &dst_desc);

        if (FAILED(tex->LockRect(mip, &src_lr, nullptr, D3DLOCK_READONLY))) break;
        if (FAILED(tex->LockRect(mip + 1, &dst_lr, nullptr, 0))) { tex->UnlockRect(mip); break; }

        PixelFormat src_fmt(src_desc.Format);
        PixelFormat dst_fmt(dst_desc.Format);

        unsigned char* src_p = (unsigned char*)src_lr.pBits;
        unsigned char* dst_p = (unsigned char*)dst_lr.pBits;

        for (UINT y = 0; y < dst_desc.Height; ++y) {
            unsigned char* src_t = src_p + (y * 2) * src_lr.Pitch;
            unsigned char* dst_t = dst_p + y * dst_lr.Pitch;
            for (UINT x = 0; x < dst_desc.Width; ++x) {
                unsigned char* p0 = src_t + x * 2 * src_fmt.getPitch();
                unsigned char* p1 = p0 + src_fmt.getPitch();
                unsigned char* p2 = p0 + src_lr.Pitch;
                unsigned char* p3 = p2 + src_fmt.getPitch();
                unsigned c0 = src_fmt.getPixel(p0), c1 = src_fmt.getPixel(p1);
                unsigned c2 = src_fmt.getPixel(p2), c3 = src_fmt.getPixel(p3);
                unsigned argb =
                    ((c0 & 0xfcfcfcfc) >> 2) + ((c1 & 0xfcfcfcfc) >> 2) +
                    ((c2 & 0xfcfcfcfc) >> 2) + ((c3 & 0xfcfcfcfc) >> 2);
                argb += (((c0 & 0x03030303) + (c1 & 0x03030303) +
                    (c2 & 0x03030303) + (c3 & 0x03030303)) >> 2) & 0x03030303;
                dst_fmt.setPixel(dst_t + x * dst_fmt.getPitch(), argb);
            }
        }
        tex->UnlockRect(mip + 1);
        tex->UnlockRect(mip);
    }
}

void ddUtil::copy(IDirect3DSurface8* dest_surf, int dx, int dy, int dw, int dh,
    IDirect3DSurface8* src_surf, int sx, int sy, int sw, int sh) {
    D3DLOCKED_RECT src_lr, dst_lr;
    D3DSURFACE_DESC src_desc, dst_desc;
    src_surf->GetDesc(&src_desc);
    dest_surf->GetDesc(&dst_desc);

    if (FAILED(src_surf->LockRect(&src_lr, nullptr, D3DLOCK_READONLY))) return;
    if (FAILED(dest_surf->LockRect(&dst_lr, nullptr, 0))) { src_surf->UnlockRect(); return; }

    PixelFormat src_fmt(src_desc.Format);
    PixelFormat dst_fmt(dst_desc.Format);

    unsigned char* src_p = (unsigned char*)src_lr.pBits + sy * src_lr.Pitch + sx * src_fmt.getPitch();
    unsigned char* dst_p = (unsigned char*)dst_lr.pBits + dy * dst_lr.Pitch + dx * dst_fmt.getPitch();

    for (int y = 0; y < dh; ++y) {
        unsigned char* src_row = src_p + src_lr.Pitch * (y * sh / dh);
        unsigned char* dst_row = dst_p + dst_lr.Pitch * y;
        for (int x = 0; x < dw; ++x) {
            dst_fmt.setPixel(dst_row + x * dst_fmt.getPitch(),
                src_fmt.getPixel(src_row + src_fmt.getPitch() * (x * sw / dw)));
        }
    }

    dest_surf->UnlockRect();
    src_surf->UnlockRect();
}

static void buildMask(FIBITMAP* fib, BYTE* bits, int pitch, int w, int h) {
    for (int y = 0; y < h; ++y) {
        BYTE* src = FreeImage_GetScanLine(fib, h - 1 - y);
        DWORD* dst = (DWORD*)(bits + y * pitch);
        for (int x = 0; x < w; ++x) {
            RGBQUAD* p = (RGBQUAD*)(src + x * 4);
            unsigned rgb = ((unsigned)p->rgbRed << 16) | ((unsigned)p->rgbGreen << 8) | p->rgbBlue;
            dst[x] = rgb ? (0xff000000 | rgb) : 0;
        }
    }
}

static void buildAlpha(FIBITMAP* fib, BYTE* bits, int pitch, int w, int h, bool whiten) {
    for (int y = 0; y < h; ++y) {
        BYTE* src = FreeImage_GetScanLine(fib, h - 1 - y);
        DWORD* dst = (DWORD*)(bits + y * pitch);
        for (int x = 0; x < w; ++x) {
            RGBQUAD* p = (RGBQUAD*)(src + x * 4);
            unsigned alpha = ((unsigned)p->rgbRed + p->rgbGreen + p->rgbBlue) / 3;
            unsigned argb = (alpha << 24) | ((unsigned)p->rgbRed << 16) | ((unsigned)p->rgbGreen << 8) | p->rgbBlue;
            if (whiten) argb |= 0xffffff;
            dst[x] = argb;
        }
    }
}

IDirect3DTexture8* ddUtil::createSurface(int width, int height, int flags, gxGraphics* gfx) {
    IDirect3DDevice8* dev = gfx->dir3dDev;

    bool isTexture = (flags & gxCanvas::CANVAS_TEXTURE) != 0;
    bool hasMips = (flags & gxCanvas::CANVAS_TEX_MIPMAP) != 0 && isTexture;
    bool isAlpha = (flags & gxCanvas::CANVAS_TEX_ALPHA) != 0;

    if (isTexture) adjustTexSize(&width, &height, dev);

    D3DFORMAT fmt = (isAlpha || (flags & gxCanvas::CANVAS_TEX_MASK)) ? D3DFMT_A8R8G8B8 : D3DFMT_X8R8G8B8;
    if (flags & gxCanvas::CANVAS_TEX_HICOLOR) fmt = D3DFMT_A4R4G4B4;

    UINT mipLevels = hasMips ? 0 : 1;
    IDirect3DTexture8* tex = nullptr;
    if (FAILED(dev->CreateTexture(width, height, mipLevels, 0, fmt, D3DPOOL_MANAGED, &tex))) return nullptr;
    return tex;
}

IDirect3DTexture8* ddUtil::loadSurface(const std::string& file, int flags, gxGraphics* gfx,
    int* outLogicalW, int* outLogicalH) {
    g_lastImageError.clear();

    FREE_IMAGE_FORMAT fif = FreeImage_GetFileType(file.c_str(), 0);
    if (fif == FIF_UNKNOWN) fif = FreeImage_GetFIFFromFilename(file.c_str());
    if (fif == FIF_UNKNOWN) {
        g_lastImageError = "Unsupported or unknown image format: " + file;
        return nullptr;
    }

    FIBITMAP* fib = FreeImage_Load(fif, file.c_str(), 0);
    if (!fib) {
        g_lastImageError = "FreeImage_Load failed for: " + file;
        return nullptr;
    }

    int bpp = FreeImage_GetBPP(fib);
    FIBITMAP* fib32 = nullptr;

    if (bpp == 32) {
        // manually create a copy since FreeImage_Clone is unavailable in this ver
        int width = FreeImage_GetWidth(fib);
        int height = FreeImage_GetHeight(fib);

        fib32 = FreeImage_Allocate(width, height, 32, 0xFF, 0xFF00, 0xFF0000);
        if (!fib32) {
            g_lastImageError = "FreeImage_Allocate failed for 32-bit image: " + file;
            FreeImage_Unload(fib);
            return nullptr;
        }
        for (int y = 0; y < height; y++) {
            memcpy(FreeImage_GetScanLine(fib32, y), FreeImage_GetScanLine(fib, y), width * 4);
        }
    }
    else {
        fib32 = FreeImage_ConvertTo32Bits(fib);
        if (!fib32) {
            g_lastImageError = "FreeImage_ConvertTo32Bits failed for: " + file;
            FreeImage_Unload(fib);
            return nullptr;
        }
    }

    FreeImage_Unload(fib);

    int w = FreeImage_GetWidth(fib32);
    int h = FreeImage_GetHeight(fib32);

    if (outLogicalW) *outLogicalW = w;
    if (outLogicalH) *outLogicalH = h;

    int adjW = w, adjH = h;
    if (!(flags & gxCanvas::CANVAS_NONDISPLAY)) adjustTexSize(&adjW, &adjH, gfx->dir3dDev);

    bool hasMask = (flags & gxCanvas::CANVAS_TEX_MASK) != 0;
    bool hasAlpha = (flags & gxCanvas::CANVAS_TEX_ALPHA) != 0;
    bool hasMips = (flags & gxCanvas::CANVAS_TEX_MIPMAP) != 0;

    D3DFORMAT fmt = (hasMask || hasAlpha) ? D3DFMT_A8R8G8B8 : D3DFMT_X8R8G8B8;
    UINT mipLevels = hasMips ? 0 : 1;

    IDirect3DDevice8* dev = gfx->dir3dDev;
    IDirect3DTexture8* tex = nullptr;
    HRESULT hr = dev->CreateTexture(adjW, adjH, mipLevels, 0, fmt, D3DPOOL_MANAGED, &tex);
    if (FAILED(hr) || !tex) {
        g_lastImageError = "CreateTexture failed for " + file + " (HRESULT " + std::to_string(hr) + ")";
        FreeImage_Unload(fib32);
        return nullptr;
    }

    D3DLOCKED_RECT lr;
    if (FAILED(tex->LockRect(0, &lr, nullptr, 0))) {
        g_lastImageError = "LockRect failed for " + file;
        tex->Release();
        FreeImage_Unload(fib32);
        return nullptr;
    }

    BYTE* bits = (BYTE*)lr.pBits;
    for (int y = 0; y < h && y < adjH; ++y) {
        BYTE* src = FreeImage_GetScanLine(fib32, h - 1 - y);
        DWORD* dst = (DWORD*)(bits + y * lr.Pitch);
        for (int x = 0; x < w && x < adjW; ++x) {
            RGBQUAD* p = (RGBQUAD*)(src + x * 4);
            DWORD argb = ((DWORD)p->rgbReserved << 24) |
                ((DWORD)p->rgbRed << 16) |
                ((DWORD)p->rgbGreen << 8) |
                (DWORD)p->rgbBlue;
            if (hasMask) {
                unsigned rgb = argb & 0xffffff;
                argb = rgb ? (0xff000000 | rgb) : 0;
            }
            else if (hasAlpha) {
                unsigned lum = (((argb >> 16) & 0xff) + ((argb >> 8) & 0xff) + (argb & 0xff)) / 3;
                argb = (lum << 24) | (argb & 0xffffff) | 0xffffff;
            }
            dst[x] = argb;
        }

        // zero padding so it never bleeds into bilinear samples
        if (w < adjW) memset(dst + w, 0, (adjW - w) * sizeof(DWORD));
    }

    for (int y = h; y < adjH; ++y) memset(bits + y * lr.Pitch, 0, adjW * sizeof(DWORD));

    tex->UnlockRect(0);
    FreeImage_Unload(fib32);

    if (hasMips) buildMipMaps(tex);

    return tex;
}