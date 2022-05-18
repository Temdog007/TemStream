/******************************************************************************
	Copyright (C) 2022 by Temitope Alaga <temdog007@yaoo.com>
	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

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

	SDL_Texture *operator->()
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

	SDL_Surface *operator->()
	{
		return surface;
	}

	void swap(SDL_SurfaceWrapper &);
};
} // namespace TemStream