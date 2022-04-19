#pragma once

#include <main.hpp>

namespace TemStream
{
class Audio
{
  private:
	const MessageSource source;
	Bytes storedAudio;
	Bytes currentAudio;
	SDL_AudioSpec spec;
	union {
		OpusDecoder *decoder;
		OpusEncoder *encoder;
	};
	SDL_AudioDeviceID id;
	SDL_KeyCode code;
	float volume;
	bool pushToTalk;
	const bool recording;

	Audio(const MessageSource &, bool);

	void recordAudio(const uint8_t *, int);
	void playbackAudio(uint8_t *, int);

	static SDL_AudioSpec getAudioSpec();

	static void recordCallback(Audio *, const uint8_t *, int);
	static void playbackCallback(Audio *, uint8_t *, int);

	static int audioLengthToFrames(const int frequency, const int duration);

	static int closestValidFrameCount(const int frequency, const int frames);

  public:
	Audio() = delete;
	Audio(const Audio &) = delete;
	Audio(Audio &&) = delete;
	~Audio();

	void enqueueAudio(Bytes &);

	bool isRecording() const
	{
		return recording;
	}

	const MessageSource &getSource() const
	{
		return source;
	}

	void useCurrentAudio(const std::function<void(const Bytes &)> &) const;

	static std::shared_ptr<Audio> startRecording(const MessageSource &, const char *);
	static std::shared_ptr<Audio> startPlayback(const MessageSource &, const char *);
};
} // namespace TemStream