#ifndef GXSHADERCOMPAT_H
#define GXSHADERCOMPAT_H

#include <string>

struct IDirect3DDevice9;

bool convertShaderSource(IDirect3DDevice9* dev, const std::string& filename, std::string& out);

#endif