#pragma once

#include <string>
#include <vector>

class Texture 
{
public:
	void setupTexture(const char* texturePath, bool flip = true);
	unsigned int setupCubeMap(std::vector<std::string> faces);
	void bind(unsigned int slot) const;
	void unbind() const;
    unsigned int getID() const { return ID; }

private:
	unsigned int ID = 0;
	int Width = 0, Height = 0, BPP = 0;
};