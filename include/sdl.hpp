#pragma once

#include <main.hpp>

namespace TemStream
{
class SDL_TextureWrapper
{
  protected:
	SDL_Texture *texture;

  public:
	SDL_TextureWrapper(SDL_Texture *);
	SDL_TextureWrapper(const SDL_TextureWrapper &) = delete;
	SDL_TextureWrapper(SDL_TextureWrapper &&);
	virtual ~SDL_TextureWrapper();

	SDL_TextureWrapper &operator=(const SDL_TextureWrapper &) = delete;
	SDL_TextureWrapper &operator=(SDL_TextureWrapper &&);

	SDL_Texture *&operator*()
	{
		return texture;
	}

	void swap(SDL_TextureWrapper &);
};
class SDL_SurfaceWrapper
{
  protected:
	SDL_Surface *surface;

  public:
	SDL_SurfaceWrapper(SDL_Surface *);
	SDL_SurfaceWrapper(const SDL_SurfaceWrapper &) = delete;
	SDL_SurfaceWrapper(SDL_SurfaceWrapper &&);
	virtual ~SDL_SurfaceWrapper();

	SDL_SurfaceWrapper &operator=(const SDL_SurfaceWrapper &) = delete;
	SDL_SurfaceWrapper &operator=(SDL_SurfaceWrapper &&);

	SDL_Surface *&operator*()
	{
		return surface;
	}

	void swap(SDL_SurfaceWrapper &);
};
} // namespace TemStream