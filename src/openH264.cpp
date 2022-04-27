#include <main.hpp>

#include <openH264.hpp>

namespace TemStream
{
OpenH264::OpenH264(Encoder &&e, int width, int height) : data(std::move(e))
{
	setWidth(width - (width % 2));
	setHeight(height - (height % 2));
}
OpenH264::OpenH264(Decoder &&d) : data(std::move(d))
{
}
OpenH264::~OpenH264()
{
}
unique_ptr<Video::EncoderDecoder> Video::createEncoder(Video::FrameData fd, const bool forCamera)
{
	std::unique_ptr<ISVCEncoder, EncoderDeleter> encoder = nullptr;
	{
		ISVCEncoder *encoderPtr = nullptr;
		const int rv = WelsCreateSVCEncoder(&encoderPtr);
		if (rv != cmResultSuccess || encoderPtr == nullptr)
		{
			(*logger)(Logger::Error) << "Failed to create encoder" << std::endl;
			return nullptr;
		}
		encoder = std::unique_ptr<ISVCEncoder, EncoderDeleter>(encoderPtr, EncoderDeleter());
	}

	SEncParamExt param{};
	if (encoder->GetDefaultParams(&param) != cmResultSuccess)
	{
		(*logger)(Logger::Error) << "Failed to create encoder" << std::endl;
		return nullptr;
	}

	param.iUsageType = forCamera ? CAMERA_VIDEO_REAL_TIME : SCREEN_CONTENT_REAL_TIME;
	param.fMaxFrameRate = fd.fps;
	param.iPicWidth = fd.width;
	param.iPicHeight = fd.height;
	param.iTargetBitrate = fd.bitrateInMbps * 1024u * 1024u;
	param.iMaxBitrate = param.iTargetBitrate;
	param.iTemporalLayerNum = true;
	param.iSpatialLayerNum = true;
	param.bEnableDenoise = 0;
	param.bEnableBackgroundDetection = false;
	param.bEnableAdaptiveQuant = false;
	param.bEnableFrameSkip = true;
	param.bEnableLongTermReference = 0;
	param.iLtrMarkPeriod = 30;
	param.iMultipleThreadIdc = std::thread::hardware_concurrency();

	param.sSpatialLayers[0].iVideoWidth = param.iPicWidth;
	param.sSpatialLayers[0].iVideoHeight = param.iPicHeight;
	param.sSpatialLayers[0].fFrameRate = param.fMaxFrameRate;
	param.sSpatialLayers[0].iSpatialBitrate = param.iTargetBitrate;
	param.sSpatialLayers[0].iMaxSpatialBitrate = param.iMaxBitrate;
	param.sSpatialLayers[0].uiProfileIdc = PRO_BASELINE;

	if (encoder->InitializeExt(&param) != cmResultSuccess)
	{
		(*logger)(Logger::Error) << "Failed to create encoder" << std::endl;
		return nullptr;
	}

#if _DEBUG
	int log_level = WELS_LOG_INFO;
#else
	int log_level = WELS_LOG_WARNING;
#endif

	int videoFormat = videoFormatI420;
	const bool result = encoder->SetOption(ENCODER_OPTION_TRACE_LEVEL, &log_level) == cmResultSuccess &&
						encoder->SetOption(ENCODER_OPTION_DATAFORMAT, &videoFormat) == cmResultSuccess;
	if (!result)
	{
		return nullptr;
	}

	return tem_unique<OpenH264>(std::move(encoder), fd.width, fd.height);
}
void OpenH264::encodeAndSend(ByteList &bytes, const Message::Source &source)
{
	if (auto encoderPtr = std::get_if<Encoder>(&data))
	{
		auto &encoder = *encoderPtr;
		SSourcePicture pic{};
		pic.iPicWidth = getWidth();
		pic.iPicHeight = getHeight();
		pic.iColorFormat = videoFormatI420;
		pic.iStride[0] = pic.iPicWidth;
		pic.iStride[1] = pic.iStride[2] = pic.iPicWidth >> 1;

		pic.pData[0] = reinterpret_cast<unsigned char *>(bytes.data());
		pic.pData[1] = reinterpret_cast<unsigned char *>(bytes.data() + (pic.iPicWidth * pic.iPicHeight));
		pic.pData[2] = reinterpret_cast<unsigned char *>(pic.pData[1] + ((pic.iPicWidth * pic.iPicHeight) / 4));

		SFrameBSInfo info{};
		const int rv = encoder->EncodeFrame(&pic, &info);
		if (rv != cmResultSuccess)
		{
			return;
		}

		if (info.eFrameType == videoFrameTypeSkip)
		{
			return;
		}

		size_t layerSize[MAX_LAYER_NUM_OF_FRAME] = {0};
		for (int layerNum = 0; layerNum < info.iLayerNum; ++layerNum)
		{
			for (int i = 0; i < info.sLayerInfo[layerNum].iNalCount; ++i)
			{
				layerSize[layerNum] += info.sLayerInfo[layerNum].pNalLengthInByte[i];
			}
		}

		Message::Video v;
		v.width = getWidth();
		v.height = getHeight();
		for (int layerNum = 0; layerNum < info.iLayerNum; ++layerNum)
		{
			v.bytes.append(info.sLayerInfo[layerNum].pBsBuf, layerSize[layerNum]);
		}

		Message::Packet *packet = allocateAndConstruct<Message::Packet>();
		packet->source = source;
		packet->payload.emplace<Message::Video>(std::move(v));

		SDL_Event e;
		e.type = SDL_USEREVENT;
		e.user.code = TemStreamEvent::SendSingleMessagePacket;
		e.user.data1 = packet;
		e.user.data2 = nullptr;
		if (!tryPushEvent(e))
		{
			destroyAndDeallocate(packet);
		}
	}
}
unique_ptr<Video::EncoderDecoder> Video::createDecoder()
{
	std::unique_ptr<ISVCDecoder, DecoderDeleter> decoder = nullptr;
	{
		ISVCDecoder *decoderPtr = nullptr;
		if (WelsCreateDecoder(&decoderPtr))
		{
			(*logger)(Logger::Error) << "Failed to create decoder" << std::endl;
			return nullptr;
		}
		decoder = std::unique_ptr<ISVCDecoder, DecoderDeleter>(decoderPtr, DecoderDeleter());
	}

	SDecodingParam param{};
	if (decoder->Initialize(&param) != cmResultSuccess)
	{
		(*logger)(Logger::Error) << "Failed to initialize decoder" << std::endl;
		return nullptr;
	}

#if _DEBUG
	int log_level = WELS_LOG_INFO;
#else
	int log_level = WELS_LOG_WARNING;
#endif

	const bool result = decoder->SetOption(DECODER_OPTION_TRACE_LEVEL, &log_level) == cmResultSuccess;
	if (!result)
	{
		return nullptr;
	}

	return tem_unique<OpenH264>(std::move(decoder));
}
bool OpenH264::decode(ByteList &bytes)
{
	if (auto decoderPtr = std::get_if<Decoder>(&data))
	{
		auto &decoder = *decoderPtr;
		unsigned char *ptrs[3] = {0};
		SBufferInfo info{};
		const DECODING_STATE state = decoder->DecodeFrameNoDelay(bytes.data(), bytes.size(), ptrs, &info);

		if (state != dsErrorFree)
		{
			(*logger)(Logger::Warning) << "Failed to decode video frame" << std::endl;
			return false;
		}
		if (info.iBufferStatus != 1)
		{
			return false;
		}

		bytes.reallocate(info.UsrData.sSystemBuffer.iWidth * info.UsrData.sSystemBuffer.iHeight * 2);

		setWidth(info.UsrData.sSystemBuffer.iWidth);
		setHeight(info.UsrData.sSystemBuffer.iHeight);

		int strides[3] = {
			info.UsrData.sSystemBuffer.iStride[0],
			info.UsrData.sSystemBuffer.iStride[1],
			info.UsrData.sSystemBuffer.iStride[1],
		};
		int ws[3] = {
			info.UsrData.sSystemBuffer.iWidth,
			info.UsrData.sSystemBuffer.iWidth / 2,
			info.UsrData.sSystemBuffer.iWidth / 2,
		};
		int hs[3] = {
			info.UsrData.sSystemBuffer.iHeight,
			info.UsrData.sSystemBuffer.iHeight / 2,
			info.UsrData.sSystemBuffer.iHeight / 2,
		};
		bytes.clear();
		for (int plane = 0; plane < 3; ++plane)
		{
			const unsigned char *buf = ptrs[plane];
			const int stride = strides[plane];
			const int w = ws[plane];
			const int h = hs[plane];
			for (int y = 0; y < h; ++y)
			{
				bytes.append(buf, w);
				buf += stride;
			}
		}
		return true;
	}

	return false;
}
void DecoderDeleter::operator()(ISVCDecoder *d) const
{
	if (d != nullptr)
	{
		d->Uninitialize();
		WelsDestroyDecoder(d);
	}
}
void EncoderDeleter::operator()(ISVCEncoder *e) const
{
	if (e != nullptr)
	{
		e->Uninitialize();
		WelsDestroySVCEncoder(e);
	}
}
} // namespace TemStream