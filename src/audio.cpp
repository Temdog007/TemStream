#include <main.hpp>

namespace TemStream
{
Audio::Audio(const Message::Source &source, const Type type, const float volume)
	: buffer(), source(source), name(), recordBuffer(), storedAudio(), currentAudio(), spec(), decoder(nullptr), id(0),
	  code(SDLK_UNKNOWN), volume(volume), type(type)
{
}
Audio::~Audio()
{
	close();
}
void Audio::close()
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
void Audio::enqueueAudio(const ByteList &bytes)
{
	const int result = opus_decode_float(decoder, reinterpret_cast<const unsigned char *>(bytes.data()), bytes.size(),
										 fbuffer.data(), audioLengthToFrames(spec.freq, OPUS_FRAMESIZE_120_MS), 0);
	if (result < 0)
	{
		(*logger)(Logger::Error) << "Failed to decode audio: " << opus_strerror(result) << std::endl;
	}
	else
	{
		// (*logger)(Logger::Trace) << "Decoded bytes " << bytesRead << std::endl;
		for (auto i = 0; i < result; ++i)
		{
			fbuffer[i] = std::clamp(fbuffer[i] * volume, -1.f, 1.f);
		}
		const size_t bytesRead = result * spec.channels * sizeof(float);
		Lock lock(id);
		storedAudio.append(buffer.data(), bytesRead);
	}
}
void Audio::clearAudio()
{
	Lock lock(id);
	storedAudio.clear();
}
ByteList Audio::getCurrentAudio() const
{
	Lock lock(id);
	return currentAudio;
}
SDL_AudioSpec Audio::getAudioSpec()
{
	SDL_AudioSpec desired;
	desired.channels = 2;
	desired.format = AUDIO_F32;
	desired.freq = 48000;
	desired.samples = 2048;
	return desired;
}
unique_ptr<Audio> Audio::startRecording(const Message::Source &source, const char *name, const float silenceThresold)
{
	Audio *a = allocateAndConstruct<Audio>(source, Type::Record, silenceThresold);
	a->name = (name == nullptr ? "(Default audio device)" : name);
	return startRecording(a, OPUS_APPLICATION_VOIP);
}
unique_ptr<Audio> Audio::startRecording(Audio *audioPtr, const int application)
{
	auto a = unique_ptr<Audio>(audioPtr);

	SDL_AudioSpec desired = getAudioSpec();
	desired.callback = (SDL_AudioCallback)Audio::recordCallback;
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
		(*logger)(Logger::Error) << "Failed to create audio encoder: " << opus_strerror(error) << std::endl;
		return nullptr;
	}

	SDL_PauseAudioDevice(a->id, SDL_FALSE);
	*logger << "Recording audio from device: " << a->name << std::endl;
	return a;
}
unique_ptr<Audio> Audio::startPlayback(const Message::Source &source, const char *name, const float volume)
{
	auto a = tem_unique<Audio>(source, Type::Playback, volume);

	SDL_AudioSpec desired = getAudioSpec();
	desired.callback = (SDL_AudioCallback)Audio::playbackCallback;
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
		(*logger)(Logger::Error) << "Failed to create audio decoder: " << opus_strerror(error) << std::endl;
		return nullptr;
	}

	SDL_PauseAudioDevice(a->id, SDL_FALSE);
	a->name = (name == nullptr ? "(Default audio device)" : name);
	return a;
}
void Audio::recordCallback(Audio *a, uint8_t *data, const int count)
{
	a->recordAudio(data, count);
}
void Audio::playbackCallback(Audio *a, uint8_t *data, const int count)
{
	a->playbackAudio(data, count);
}
void Audio::playbackAudio(uint8_t *data, const int count)
{
	if (count < 1)
	{
		return;
	}
	memset(data, spec.silence, count);
	if (storedAudio.size() > MB(1))
	{
		(*logger)(Logger::Warning) << "Audio delay is occurring for playback. Audio packets will be dropped."
								   << std::endl;
		storedAudio.clear();
	}
	const size_t toCopy = std::min((size_t)count, storedAudio.size());
	currentAudio.clear();
	if (toCopy > 0)
	{
		memcpy(data, storedAudio.data(), toCopy);
		currentAudio.append(storedAudio, toCopy);
		storedAudio.remove(toCopy);
	}
	// else if (!currentAudio.empty())
	// {
	// 	// Make echo effect rather than random intervals of silence if possible
	// 	SDL_MixAudioFormat(data, currentAudio.data(), this->spec.format, std::min((size_t)count, currentAudio.size()),
	// 					   SDL_MIX_MAXVOLUME / 2);
	// }
}
bool Audio::isLoudEnough(float *data, const int count) const
{
	float sum = 0.f;
	for (int i = 0; i < count; ++i)
	{
		sum += std::abs(data[i]);
	}
	return sum / count > silenceThreshold;
}
void Audio::recordAudio(uint8_t *data, const int count)
{
	if (!isLoudEnough(reinterpret_cast<float *>(data), count / sizeof(float)))
	{
		memset(data, spec.silence, count);
	}
	storedAudio.append(data, count);
	if (storedAudio.size() > MB(1))
	{
		(*logger)(Logger::Warning) << "Audio delay is occurring for recording. Audio packets will be dropped."
								   << std::endl;
		storedAudio.clear();
		return;
	}

	currentAudio.clear();
	currentAudio.append(data, count);
}
bool Audio::isRecording() const
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
bool Audio::encodeAndSendAudio(ClientConnetion &peer)
{
	if (!isRecording())
	{
		return true;
	}

	ByteList outgoing;
	{
		Lock lock(id);
		outgoing.swap(storedAudio);
	}

	const int minDuration = audioLengthToFrames(spec.freq, OPUS_FRAMESIZE_2_5_MS);
	MessagePackets packets;

	while (true)
	{
		int frameSize = outgoing.size() / (spec.channels * sizeof(float));
		if (frameSize < minDuration)
		{
			break;
		}

		frameSize = closestValidFrameCount(spec.freq, frameSize);
		const size_t bytesUsed = (frameSize * spec.channels * sizeof(float));

		recordBuffer.clear();
		recordBuffer.append(outgoing, bytesUsed);
		outgoing.remove(bytesUsed);

		const int result = opus_encode_float(encoder, reinterpret_cast<float *>(recordBuffer.data()), frameSize,
											 reinterpret_cast<unsigned char *>(buffer.data()), buffer.size());
		if (result < 0)
		{
			(*logger)(Logger::Error) << "Failed to encode audio packet: " << opus_strerror(result)
									 << "; Frame size: " << frameSize << std::endl;
			break;
		}

		Message::Packet packet;
		packet.source = source;
		packet.payload.emplace<Message::Audio>(Message::Audio{ByteList(buffer.data(), result)});
		packets.push_back(std::move(packet));
	}

	for (const auto &packet : packets)
	{
		peer->sendPacket(packet);
	}
	peer.addPackets(std::move(packets));
	if (!outgoing.empty())
	{
		Lock lock(id);
		storedAudio = outgoing + storedAudio;
	}
	return true;
}
constexpr int Audio::closestValidFrameCount(const int frequency, const int frames)
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
constexpr int Audio::audioLengthToFrames(const int frequency, const int duration)
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
Audio::Lock::Lock(const SDL_AudioDeviceID id) : id(id)
{
	SDL_LockAudioDevice(id);
}
Audio::Lock::~Lock()
{
	SDL_UnlockAudioDevice(id);
}
} // namespace TemStream