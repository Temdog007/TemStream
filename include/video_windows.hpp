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

#include <WinUser.h>

namespace TemStream
{
class WindowsScreenshot : public Screenshot
{
  public:
	ByteList bytes;

	uint8_t *getData() override
	{
		return bytes.data();
	}

	void getData(ByteList &dst) const
	{
		dst.append(bytes);
	}
};
class Screenshotter
{
  private:
	std::weak_ptr<Converter> converter;
	WindowProcess window;
	TimePoint nextFrame;
	std::shared_ptr<VideoSource> video;
	uint32_t fps;
	bool visible;
	bool first;

	static bool takeScreenshot(shared_ptr<Screenshotter>);

  public:
	Screenshotter(const WindowProcess &w, shared_ptr<Converter> ptr, shared_ptr<VideoSource> v, const uint32_t fps);
	~Screenshotter();

	static void startTakingScreenshots(shared_ptr<Screenshotter>);
};
} // namespace TemStream