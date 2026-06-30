#include "std.h"
#include "gxmesh.h"
#include "gxgraphics.h"

#include "gxruntime.h"

extern gxRuntime* gx_runtime;

gxMesh::gxMesh(gxGraphics* g, IDirect3DVertexBuffer9* vs, IDirect3DIndexBuffer9* is,
    int max_vs, int max_ts) :
    graphics(g), vertex_buff(vs), index_buff(is),
    locked_verts(nullptr), locked_indices(nullptr),
    max_verts(max_vs), max_tris(max_ts), mesh_dirty(false) {
}

gxMesh::~gxMesh() {
    unlock();
    if (vertex_buff) { vertex_buff->Release(); vertex_buff = nullptr; }
    if (index_buff) { index_buff->Release();  index_buff = nullptr; }
}

bool gxMesh::lock(bool all) {
    if (locked_verts && locked_indices) return true;

    // lock vert buffer
    if (!locked_verts) {
        DWORD vflags = D3DLOCK_NOSYSLOCK | (all ? D3DLOCK_DISCARD : D3DLOCK_NOOVERWRITE);
        void* ptr = nullptr;
        if (FAILED(vertex_buff->Lock(0, 0, &ptr, vflags))) {
            static dxVertex err_verts[32768];
            locked_verts = err_verts;
        }
        else {
            locked_verts = reinterpret_cast<dxVertex*>(ptr);
        }
    }

    // lock index buffer
    if (!locked_indices) {
        DWORD iflags = D3DLOCK_NOSYSLOCK | (all ? D3DLOCK_DISCARD : D3DLOCK_NOOVERWRITE);
        void* ptr = nullptr;
        if (FAILED(index_buff->Lock(0, 0, &ptr, iflags))) {
            static WORD err_indices[32768 * 3];
            locked_indices = err_indices;
        }
        else {
            locked_indices = reinterpret_cast<WORD*>(ptr);
        }
    }

    mesh_dirty = false;
    return true;
}

void gxMesh::unlock() {
    if (locked_verts) {
        vertex_buff->Unlock();
        locked_verts = nullptr;
    }
    if (locked_indices) {
        index_buff->Unlock();
        locked_indices = nullptr;
    }
}

void gxMesh::backup() {
	unlock();
}

void gxMesh::restore() {
	mesh_dirty = true;
}

void gxMesh::render(int first_vert, int vert_cnt, int first_tri, int tri_cnt) {
    unlock();

    IDirect3DDevice9* dev = graphics->dir3dDev;

    dev->SetStreamSource(0, vertex_buff, 0, sizeof(dxVertex));
    dev->SetFVF(VTXFMT);
    dev->SetIndices(index_buff);
    dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, first_vert, 0, vert_cnt, first_tri * 3, tri_cnt);
}