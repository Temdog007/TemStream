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
std::optional<WindowProcesses> AudioSource::getWindowsWithAudio()
{
	// On Windows, t isn't possible to record audio from a single window process without recording the entire audio.
	// device. So, don't return anything.
	return std::nullopt;
}
unique_ptr<AudioSource> AudioSource::startRecordingWindow(const Message::Source &source, const WindowProcess &wp,
														  const float silenceThreshold)
{
	// On Windows, t isn't possible to record audio from a single window process without recording the entire audio.
	// device. So, don't return anything.
	return nullptr;
}
} // namespace TemStream