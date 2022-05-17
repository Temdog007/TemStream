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