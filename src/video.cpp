#include <main.hpp>

namespace TemStream
{
Video::Video(const Message::Source &source, const WindowProcess &wp) : source(source), windowProcress(wp)
{
}
Video::~Video()
{
}
Video::FrameEncoder::FrameEncoder(const Message::Source &source, const int32_t ratio) : frames(), source(), ratio(ratio)
{
	this->source.author = source.author;
	char buffer[KB(1)];
	snprintf(buffer, sizeof(buffer), "%s (%d%%)", source.destination.c_str(), ratio);
	this->source.destination = buffer;
}
Video::FrameEncoder::~FrameEncoder()
{
}
void Video::FrameEncoder::addFrame(shared_ptr<Frame> frame)
{
	frames.push(frame);
}
void Video::FrameEncoder::encodeFrames(shared_ptr<Video::FrameEncoder> &&ptr, FrameData frameData)
{
	(*logger)(Logger::Trace) << "Starting encoding thread: " << ptr->source << std::endl;
	if (!TemStreamGui::sendCreateMessage<Message::Video>(ptr->source))
	{
		return;
	}

	auto vpx = VPX::createEncoder(frameData);
	if (!vpx)
	{
		return;
	}

	using namespace std::chrono_literals;
	const auto maxWaitTime = 3s;
	while (!appDone)
	{
		auto result = ptr->frames.pop(maxWaitTime);
		if (!result)
		{
			break;
		}

		auto data = *result;
		if (!data)
		{
			continue;
		}

		// TODO: Resize
		vpx->encodeAndSend(data->bytes, ptr->source);
	}
	(*logger)(Logger::Trace) << "Ending encoding thread: " << ptr->source << std::endl;
}
void Video::FrameEncoder::startEncodingFrames(shared_ptr<FrameEncoder> &&ptr, FrameData frameData)
{
	Task::addTask(std::async(TaskPolicy, encodeFrames, std::move(ptr), frameData));
}
Video::VPX::VPX() : ctx(), image(), frameCount(0), keyFrameInterval(120)
{
}
Video::VPX::VPX(VPX &&v)
	: ctx(std::move(v.ctx)), image(std::move(v.image)), frameCount(v.frameCount), keyFrameInterval(v.keyFrameInterval)
{
	memset(&v.ctx, 0, sizeof(v.ctx));
	memset(&v.image, 0, sizeof(v.image));
}
Video::VPX::~VPX()
{
	vpx_codec_destroy(&ctx);
	vpx_img_free(&image);
}
vpx_codec_iface_t *codec_encoder_interface()
{
	return vpx_codec_vp8_cx();
}

vpx_codec_iface_t *codec_decoder_interface()
{
	return vpx_codec_vp8_dx();
}
int vpx_img_plane_width(const vpx_image_t *img, const int plane)
{
	if (plane > 0 && img->x_chroma_shift > 0)
	{
		return (img->d_w + 1) >> img->x_chroma_shift;
	}
	else
	{
		return img->d_w;
	}
}

int vpx_img_plane_height(const vpx_image_t *img, const int plane)
{
	if (plane > 0 && img->y_chroma_shift > 0)
	{
		return (img->d_h + 1) >> img->y_chroma_shift;
	}
	else
	{
		return img->d_h;
	}
}
void Video::VPX::encodeAndSend(const Bytes &bytes, const Message::Source &source)
{
	const char *ptr = bytes.data();
	for (int plane = 0; plane < 3; ++plane)
	{
		unsigned char *buf = image.planes[plane];
		const int stride = image.stride[plane];
		const int w = vpx_img_plane_width(&image, plane) * ((image.fmt & VPX_IMG_FMT_HIGHBITDEPTH) ? 2 : 1);
		const int h = vpx_img_plane_height(&image, plane);

		for (int y = 0; y < h; ++y)
		{
			memcpy(buf, ptr, w);
			ptr += w;
			buf += stride;
		}
	}

	int flags = 0;
	if (keyFrameInterval > 0 && ++frameCount % keyFrameInterval == 0)
	{
		flags |= VPX_EFLAG_FORCE_KF;
	}

	vpx_codec_err_t res;
	res = vpx_codec_encode(&ctx, &image, frameCount++, 1, flags, VPX_DL_REALTIME);
	if (res != VPX_CODEC_OK)
	{
		(*logger)(Logger::Error) << "Encoding failed: " << vpx_codec_err_to_string(res) << std::endl;
		return;
	}
	vpx_codec_iter_t iter = NULL;
	const vpx_codec_cx_pkt_t *pkt = NULL;
	MessagePackets *packets = allocate<MessagePackets>();
	while ((pkt = vpx_codec_get_cx_data(&ctx, &iter)) != NULL)
	{
		Message::Packet packet;
		packet.source = source;
		Message::Video v;
		const char *data = reinterpret_cast<const char *>(pkt->data.frame.buf);
		v.bytes = Bytes(data, data + pkt->data.frame.sz);
		v.width = vpx_img_plane_width(&image, 0);
		v.height = vpx_img_plane_height(&image, 0);
		packet.payload.emplace<Message::Video>(std::move(v));
		packets->push_back(std::move(packet));
	}
	SDL_Event e;
	e.type = SDL_USEREVENT;
	e.user.code = TemStreamEvent::SendMessagePackets;
	e.user.data1 = packets;
	e.user.data2 = &e;
	if (!tryPushEvent(e))
	{
		deallocate(packets);
	}
}
std::optional<Bytes> Video::VPX::decode(const Bytes &bytes)
{
	vpx_codec_err_t res =
		vpx_codec_decode(&ctx, reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size(), NULL, VPX_DL_REALTIME);
	if (res != VPX_CODEC_OK)
	{
		(*logger)(Logger::Error) << "Failed to decode video: " << vpx_codec_err_to_string(res) << std::endl;
		return std::nullopt;
	}
	vpx_codec_iter_t iter = NULL;
	vpx_image_t *img = NULL;
	Bytes rval;
	rval.reserve(MB(8));

	while ((img = vpx_codec_get_frame(&ctx, &iter)) != NULL)
	{
		rval.clear();
		for (int plane = 0; plane < 3; ++plane)
		{
			const char *buf = reinterpret_cast<const char *>(img->planes[plane]);
			const int stride = img->stride[plane];
			const int w = vpx_img_plane_width(img, plane) * ((img->fmt & VPX_IMG_FMT_HIGHBITDEPTH) ? 2 : 1);
			const int h = vpx_img_plane_height(img, plane);
			for (int y = 0; y < h; ++y)
			{
				rval.insert(rval.end(), buf, buf + w);
				buf += stride;
			}
		}
	}
	return rval;
}
std::optional<Video::VPX> Video::VPX::createEncoder(FrameData fd)
{
	Video::VPX vpx;
	vpx_codec_enc_cfg_t cfg;
	if (vpx_img_alloc(&vpx.image, VPX_IMG_FMT_I420, fd.width, fd.height, 1) == nullptr)
	{
		(*logger)(Logger::Error) << "Failed to allocate image" << std::endl;
		return std::nullopt;
	}

	(*logger)(Logger::Trace) << "Encoder: " << vpx_codec_iface_name(codec_encoder_interface()) << std::endl;

	const vpx_codec_err_t res = vpx_codec_enc_config_default(codec_encoder_interface(), &cfg, 0);
	if (res)
	{
		(*logger)(Logger::Error) << "Failed to get default video encoder configuration: "
								 << vpx_codec_err_to_string(res) << std::endl;
		return std::nullopt;
	}

	cfg.g_w = fd.width;
	cfg.g_h = fd.height;
	cfg.g_timebase.num = 1;
	cfg.g_timebase.den = fd.fps;
	cfg.rc_target_bitrate = fd.bitrateInMbps * 1024;
	cfg.g_threads = std::thread::hardware_concurrency();
	cfg.g_error_resilient = VPX_ERROR_RESILIENT_DEFAULT | VPX_ERROR_RESILIENT_PARTITIONS;

	if (vpx_codec_enc_init(&vpx.ctx, codec_encoder_interface(), &cfg, 0) == 0)
	{
		return vpx;
	}
	(*logger)(Logger::Error) << "Failed to initialize encoder" << std::endl;
	return std::nullopt;
}
std::optional<Video::VPX> Video::VPX::createDecoder()
{
	Video::VPX vpx;
	struct vpx_codec_dec_cfg cfg;
	cfg.threads = std::thread::hardware_concurrency();
	if (vpx_codec_dec_init(&vpx.ctx, codec_decoder_interface(), &cfg, 0) == 0)
	{
		return vpx;
	}
	(*logger)(Logger::Error) << "Failed to initialize decoder" << std::endl;
	return std::nullopt;
}
} // namespace TemStream