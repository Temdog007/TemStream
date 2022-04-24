#include <main.hpp>

namespace TemStream
{
Video::Video(const Message::Source &source, const WindowProcess &wp) : source(source), windowProcress(wp)
{
}
Video::~Video()
{
}
Video::FrameEncoder::FrameEncoder(const Message::Source &source, const float ratio)
	: frames(), vpx(), source(), ratio(ratio)
{
	this->source.author = source.author;
	char buffer[KB(1)];
	snprintf(buffer, sizeof(buffer), "%s (%d%%)", source.destination.c_str(), static_cast<int32_t>(ratio * 100.f));
	this->source.destination = buffer;
}
Video::FrameEncoder::~FrameEncoder()
{
}
void Video::FrameEncoder::addFrame(shared_ptr<Frame> frame)
{
	frames.push(frame);
}
void Video::FrameEncoder::encodeFrames(shared_ptr<Video::FrameEncoder> &&ptr)
{
	(*logger)(Logger::Trace) << "Starting encoding thread: " << ptr->source << std::endl;
	if (!TemStreamGui::sendCreateMessage<Message::Video>(ptr->source))
	{
		return;
	}
	using namespace std::chrono_literals;
	const auto maxWaitTime = 3s;
	while (!appDone)
	{
		auto data = ptr->frames.pop(maxWaitTime);
		if (!data)
		{
			break;
		}

		// TODO: Resize, encode, make and send packet
	}
	(*logger)(Logger::Trace) << "Ending encoding thread: " << ptr->source << std::endl;
}
void Video::FrameEncoder::startEncodingFrames(shared_ptr<FrameEncoder> &&ptr)
{
	std::thread thread(encodeFrames, std::move(ptr));
	thread.detach();
}
Video::VPX::VPX() : ctx(), image(), frameCount(0)
{
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

std::optional<Video::VPX> Video::VPX::createEncoder(uint32_t width, uint32_t height, int fps, uint32_t bitrate)
{
	Video::VPX vpx;
	vpx_codec_enc_cfg_t cfg;
	if (vpx_img_alloc(&vpx.image, VPX_IMG_FMT_I420, width, height, 1) == nullptr)
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

	cfg.g_w = width;
	cfg.g_h = height;
	cfg.g_timebase.num = 1;
	cfg.g_timebase.den = fps;
	cfg.rc_target_bitrate = bitrate * 1024;
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