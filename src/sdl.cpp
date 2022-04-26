#include <main.hpp>

namespace TemStream
{
SDL_TextureWrapper::SDL_TextureWrapper(SDL_Texture *texture) : texture(texture)
{
}
SDL_TextureWrapper::SDL_TextureWrapper(SDL_TextureWrapper &&w) : texture(w.texture)
{
	swap(w);
}
SDL_TextureWrapper::~SDL_TextureWrapper()
{
	SDL_DestroyTexture(texture);
	texture = nullptr;
}
void SDL_TextureWrapper::swap(SDL_TextureWrapper &w)
{
	std::swap(texture, w.texture);
}
SDL_TextureWrapper &SDL_TextureWrapper::operator=(SDL_TextureWrapper &&w)
{
	swap(w);
	return *this;
}

SDL_SurfaceWrapper::SDL_SurfaceWrapper(SDL_Surface *surface) : surface(surface)
{
}
SDL_SurfaceWrapper::SDL_SurfaceWrapper(SDL_SurfaceWrapper &&w) : surface(w.surface)
{
	swap(w);
}
SDL_SurfaceWrapper::~SDL_SurfaceWrapper()
{
	SDL_FreeSurface(surface);
	surface = nullptr;
}
void SDL_SurfaceWrapper::swap(SDL_SurfaceWrapper &w)
{
	std::swap(surface, w.surface);
}
SDL_SurfaceWrapper &SDL_SurfaceWrapper::operator=(SDL_SurfaceWrapper &&w)
{
	swap(w);
	return *this;
}
} // namespace TemStream