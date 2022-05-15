#include <main.hpp>

namespace TemStream
{
AudioSource::AudioSource(const Message::Source &source, const Type type, const float volume)
	: source(source), decoder(nullptr), id(0), volume(volume), type(type)
{
}
AudioSource::~AudioSource()
{
	close();
}
void AudioSource::close()
{
	SDL_CloseAudioDevice(id);
	id = 0;
	if (isRecording() && encoder != nullptr)
	{
		free(encoder);
		encoder = nullptr;
	}
	else if (decoder != nullptr)
	{
		free(decoder);
		decoder = nullptr;
	}
}
void AudioSource::enqueueAudio(const ByteList &bytes)
{
	const int result = opus_decode_float(decoder, reinterpret_cast<const unsigned char *>(bytes.data()), bytes.size(),
										 fbuffer.data(), audioLengthToFrames(spec.freq, OPUS_FRAMESIZE_120_MS), 0);
	if (result < 0)
	{
		(*logger)(Logger::Level::Error) << "Failed to decode audio: " << opus_strerror(result) << std::endl;
	}
	else
	{
		for (auto i = 0; i < result; ++i)
		{
			fbuffer[i] = std::clamp(fbuffer[i] * volume, -1.f, 1.f);
		}
		const size_t bytesRead = result * spec.channels * sizeof(float);
		Lock lock(id);
		if (!storedAudio.append(buffer.data(), bytesRead))
		{
			(*logger)(Logger::Level::Warning)
				<< "AudioSource delay is occurring for playback. AudioSource packets will be dropped." << std::endl;
		}
	}
}
void AudioSource::clearAudio()
{
	Lock lock(id);
	storedAudio.clear();
}
void AudioSource::getCurrentAudio(ByteList &list) const
{
	Lock lock(id);
	list = currentAudio;
}
SDL_AudioSpec AudioSource::getAudioSpec()
{
	SDL_AudioSpec desired;
	desired.channels = 2;
	desired.format = AUDIO_F32;
	desired.freq = 48000;
	desired.samples = 2048;
	return desired;
}
unique_ptr<AudioSource> AudioSource::startRecording(const Message::Source &source, const char *name,
													const float silenceThresold)
{
	AudioSource *a = allocateAndConstruct<AudioSource>(source, Type::Record, silenceThresold);
	a->name = (name == nullptr ? "(Default audio device)" : name);
	return startRecording(a, OPUS_APPLICATION_VOIP);
}
unique_ptr<AudioSource> AudioSource::startRecording(AudioSource *audioPtr, const int application)
{
	auto a = unique_ptr<AudioSource>(audioPtr);

	SDL_AudioSpec desired = getAudioSpec();
	desired.callback = (SDL_AudioCallback)AudioSource::recordCallback;
	desired.userdata = a.get();

	a->id = SDL_OpenAudioDevice(a->name.c_str(), SDL_TRUE, &desired, &a->spec, 0);
	if (a->id == 0)
	{
		logSDLError("Failed to open audio device");
		return nullptr;
	}

	const int size = opus_encoder_get_size(a->spec.channels);
	a->encoder = reinterpret_cast<OpusEncoder *>(malloc(size));
	const int error = opus_encoder_init(a->encoder, a->spec.freq, a->spec.channels, application);
	if (error < 0)
	{
		(*logger)(Logger::Level::Error) << "Failed to create audio encoder: " << opus_strerror(error) << std::endl;
		return nullptr;
	}

	SDL_PauseAudioDevice(a->id, SDL_FALSE);
	*logger << "Recording audio from device: " << a->name << std::endl;
	return a;
}
unique_ptr<AudioSource> AudioSource::startPlayback(const Message::Source &source, const char *name, const float volume)
{
	auto a = tem_unique<AudioSource>(source, Type::Playback, volume);

	SDL_AudioSpec desired = getAudioSpec();
	desired.callback = (SDL_AudioCallback)AudioSource::playbackCallback;
	desired.userdata = a.get();

	a->id = SDL_OpenAudioDevice(name, SDL_FALSE, &desired, &a->spec, 0);
	if (a->id == 0)
	{
		logSDLError("Failed to open audio device");
		return nullptr;
	}

	const int size = opus_decoder_get_size(a->spec.channels);
	a->decoder = reinterpret_cast<OpusDecoder *>(malloc(size));
	const int error = opus_decoder_init(a->decoder, a->spec.freq, a->spec.channels);
	if (error < 0)
	{
		(*logger)(Logger::Level::Error) << "Failed to create audio decoder: " << opus_strerror(error) << std::endl;
		return nullptr;
	}

	SDL_PauseAudioDevice(a->id, SDL_FALSE);
	a->name = (name == nullptr ? "(Default audio device)" : name);
	return a;
}
void AudioSource::recordCallback(AudioSource *a, uint8_t *data, const int count)
{
	a->recordAudio(data, count);
}
void AudioSource::playbackCallback(AudioSource *a, uint8_t *data, const int count)
{
	a->playbackAudio(data, count);
}
void AudioSource::playbackAudio(uint8_t *data, const int count)
{
	if (count < 1)
	{
		return;
	}
	memset(data, spec.silence, count);

	storedAudio.peek(data, static_cast<size_t>(count));

	currentAudio.clear();
	storedAudio.pop(currentAudio, static_cast<size_t>(count));
}

bool AudioSource::isLoudEnough(const float *data, const int count) const
{
	float sum = 0.f;
	for (int i = 0; i < count; ++i)
	{
		sum += std::abs(data[i]);
	}
	return sum / count > silenceThreshold;
}
void AudioSource::recordAudio(uint8_t *data, const int count)
{
	if (!isLoudEnough(reinterpret_cast<const float *>(data), count / sizeof(float)))
	{
		memset(data, spec.silence, count);
	}
	currentAudio.clear();
	if (storedAudio.append(data, static_cast<size_t>(count)))
	{
		currentAudio.append(data, static_cast<uint32_t>(count));
	}
	else
	{
		(*logger)(Logger::Level::Warning)
			<< "AudioSource delay is occurring for recording. AudioSource packets will be dropped." << std::endl;
		storedAudio.clear();
	}
}
bool AudioSource::isRecording() const
{
	switch (type)
	{
	case Type::Record:
	case Type::RecordWindow:
		return true;
	default:
		return false;
	}
}
bool AudioSource::isActive() const
{
	return decoder != nullptr || encoder != nullptr;
}
void AudioSource::encodeAndSendAudio(ClientConnection &peer)
{
	if (!isRecording())
	{
		return;
	}

	{
		Lock lock(id);
		outgoing = std::move(storedAudio);
	}

	const int minDuration = audioLengthToFrames(spec.freq, OPUS_FRAMESIZE_2_5_MS);
	MessagePackets packets;

	while (true)
	{
		int frameSize = static_cast<int>(outgoing.size() / (spec.channels * sizeof(float)));
		if (frameSize < minDuration)
		{
			break;
		}

		frameSize = closestValidFrameCount(spec.freq, frameSize);

		const int result =
			opus_encode_float(encoder, reinterpret_cast<float *>(outgoing.data()), frameSize,
							  reinterpret_cast<unsigned char *>(buffer.data()), static_cast<opus_int32>(buffer.size()));

		const size_t bytesUsed = (frameSize * spec.channels * sizeof(float));
		outgoing.remove(bytesUsed);
		if (result < 0)
		{
			(*logger)(Logger::Level::Error) << "Failed to encode audio packet: " << opus_strerror(result)
											<< "; Frame size: " << frameSize << std::endl;
			break;
		}

		Message::Packet packet;
		packet.source = source;
		packet.payload.emplace<Message::Audio>(Message::Audio{ByteList(buffer.data(), result)});
		packets.emplace_back(std::move(packet));
	}

	for (const auto &packet : packets)
	{
		peer.sendPacket(packet);
	}
	peer.addPackets(std::move(packets));
	if (!outgoing.empty())
	{
		Lock lock(id);
		if (!storedAudio.prepend(outgoing))
		{
			(*logger)(Logger::Level::Warning)
				<< "AudioSource delay is occurring for recording. AudioSource packets will be dropped." << std::endl;
			storedAudio.clear();
		}
	}
}
constexpr int AudioSource::closestValidFrameCount(const int frequency, const int frames)
{
	const int values[] = {OPUS_FRAMESIZE_120_MS, OPUS_FRAMESIZE_100_MS, OPUS_FRAMESIZE_80_MS,
						  OPUS_FRAMESIZE_60_MS,	 OPUS_FRAMESIZE_40_MS,	OPUS_FRAMESIZE_20_MS,
						  OPUS_FRAMESIZE_10_MS,	 OPUS_FRAMESIZE_5_MS,	OPUS_FRAMESIZE_2_5_MS};
	const int arrayLength = sizeof(values) / sizeof(int);
	if (frames >= audioLengthToFrames(frequency, values[0]))
	{
		return audioLengthToFrames(frequency, values[0]);
	}
	if (frames <= audioLengthToFrames(frequency, values[arrayLength - 1]))
	{
		return audioLengthToFrames(frequency, values[arrayLength - 1]);
	}
	for (int i = 1; i < arrayLength; ++i)
	{
		const int prev = audioLengthToFrames(frequency, values[i - 1]);
		const int current = audioLengthToFrames(frequency, values[i]);
		if (prev >= frames && frames >= current)
		{
			return audioLengthToFrames(frequency, values[i]);
		}
	}
	return audioLengthToFrames(frequency, values[arrayLength - 1]);
}
constexpr int AudioSource::audioLengthToFrames(const int frequency, const int duration)
{
	switch (duration)
	{
	case OPUS_FRAMESIZE_2_5_MS:
		return frequency / 400;
	case OPUS_FRAMESIZE_5_MS:
		return frequency / 200;
	case OPUS_FRAMESIZE_10_MS:
		return frequency / 100;
	case OPUS_FRAMESIZE_20_MS:
		return frequency / 50;
	case OPUS_FRAMESIZE_40_MS:
		return frequency / 25;
	case OPUS_FRAMESIZE_60_MS:
		return audioLengthToFrames(frequency, OPUS_FRAMESIZE_20_MS) * 3;
	case OPUS_FRAMESIZE_80_MS:
		return audioLengthToFrames(frequency, OPUS_FRAMESIZE_40_MS) * 2;
	case OPUS_FRAMESIZE_100_MS:
		return frequency / 10;
	case OPUS_FRAMESIZE_120_MS:
		return audioLengthToFrames(frequency, OPUS_FRAMESIZE_60_MS) * 2;
	default:
		return 0;
	}
}
AudioSource::Lock::Lock(const SDL_AudioDeviceID id) : id(id)
{
	SDL_LockAudioDevice(id);
}
AudioSource::Lock::~Lock()
{
	SDL_UnlockAudioDevice(id);
}
} // namespace TemStream