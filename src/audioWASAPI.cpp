#include <main.hpp>

namespace TemStream
{
std::optional<WindowProcesses> AudioSource::getWindowsWithAudio()
{
	return std::nullopt;
}
unique_ptr<AudioSource> AudioSource::startRecordingWindow(const Message::Source &source, const WindowProcess &wp,
														  const float silenceThreshold)
{
	return nullptr;
}
} // namespace TemStream