#ifndef IMAGE_UTIL_H
#define IMAGE_UTIL_H

bool openImage(const char* img);
bool makeExe(int entry, bool laa);
bool replaceRsrc(int type, int id, int land, void* data, int data_sz);
bool addSection(const char* name, const void* data, int size);
void closeImage();

#endif