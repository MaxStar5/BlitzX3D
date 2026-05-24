#ifndef GXMESH_H
#define GXMESH_H

#include <d3d8.h>

class gxGraphics;

class gxMesh {
public:
    struct dxVertex {
        float coords[3];
        float normal[3];
        unsigned argb;
        float tex_coords[4];   // 2 sets x 2 floats
    };

    gxMesh(gxGraphics* graphics, IDirect3DVertexBuffer8* verts, IDirect3DIndexBuffer8* indices,
        int max_verts, int max_tris);
    ~gxMesh();

    int maxVerts() const { return max_verts; }
    int maxTris()  const { return max_tris; }

    bool dirty() const { return mesh_dirty; }

    void render(int first_vert, int vert_cnt, int first_tri, int tri_cnt);

    void backup();
    void restore();

    static const DWORD VTXFMT = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX2 | D3DFVF_TEXCOORDSIZE2(0) | D3DFVF_TEXCOORDSIZE2(1);

private:
    gxGraphics* graphics;
    IDirect3DVertexBuffer8* vertex_buff;
    IDirect3DIndexBuffer8* index_buff;

    int  max_verts, max_tris;
    bool mesh_dirty;
    dxVertex* locked_verts;
    WORD* locked_indices;

    /***** GX INTERFACE *****/
public:
    bool lock(bool all);
    void unlock();

    void setVertex(int n, const void* v) {
        memcpy(locked_verts + n, v, sizeof(dxVertex));
    }
    void setVertex(int n, const float coords[3], const float normal[3], const float tex_coords[2][2]) {
        dxVertex* t = locked_verts + n;
        memcpy(t->coords, coords, 12);
        memcpy(t->normal, normal, 12);
        t->argb = 0xffffffff;
        memcpy(t->tex_coords, tex_coords, 16);
    }
    void setVertex(int n, const float coords[3], const float normal[3], unsigned argb, const float tex_coords[2][2]) {
        dxVertex* t = locked_verts + n;
        memcpy(t->coords, coords, 12);
        memcpy(t->normal, normal, 12);
        t->argb = argb;
        memcpy(t->tex_coords, tex_coords, 16);
    }
    void setTriangle(int n, int v0, int v1, int v2) {
        locked_indices[n * 3] = (WORD)v0;
        locked_indices[n * 3 + 1] = (WORD)v1;
        locked_indices[n * 3 + 2] = (WORD)v2;
    }
};

#endif