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

#include <main.hpp>

namespace TemStream
{
SDL_TextureWrapper::SDL_TextureWrapper(SDL_Texture *texture) : texture(texture)
{
}
SDL_TextureWrapper::SDL_TextureWrapper(SDL_TextureWrapper &&w) : texture(nullptr)
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