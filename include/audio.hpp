#pragma once

#include <main.hpp>

namespace TemStream
{
class ClientPeer;
class Audio
{
  private:
	union {
		std::array<float, MB(1) / sizeof(float)> fbuffer;
		std::array<char, MB(1)> buffer;
	};
	List<char> recordBuffer;
	const MessageSource source;
	String name;
	Deque<char> storedAudio;
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

	void enqueueAudio(const Bytes &);

	bool encodeAndSendAudio(ClientPeer &);

	bool isRecording() const
	{
		return recording;
	}

	float getVolume() const
	{
		return volume;
	}

	void setVolume(float volume)
	{
		this->volume = volume;
	}

	const MessageSource &getSource() const
	{
		return source;
	}

	const String &getName() const
	{
		return name;
	}

	void useCurrentAudio(const std::function<void(const Bytes &)> &) const;

	static std::shared_ptr<Audio> startRecording(const MessageSource &, const char *);
	static std::shared_ptr<Audio> startPlayback(const MessageSource &, const char *);
};
} // namespace TemStream