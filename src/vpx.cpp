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

#include <vpx.hpp>

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

namespace TemStream
{
VPX::VPX() : ctx({}), image({}), frameCount(0), keyFrameInterval(120)
{
}
VPX::VPX(VPX &&v) : ctx({}), image({}), frameCount(), keyFrameInterval()
{
	swap(v);
}
VPX::~VPX()
{
	vpx_codec_destroy(&ctx);
	ctx = {};
	vpx_img_free(&image);
	image = {};
}
VPX &VPX::operator=(VPX &&v)
{
	swap(v);
	return *this;
}
void VPX::swap(VPX &v)
{
	std::swap(ctx, v.ctx);
	std::swap(image, v.image);
	std::swap(frameCount, v.frameCount);
	std::swap(keyFrameInterval, v.keyFrameInterval);
	std::swap(width, v.width);
	std::swap(height, v.height);
}
void VPX::encodeAndSend(ByteList &bytes, const Message::Source &source)
{
	auto *ptr = bytes.data();
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
		(*logger)(Logger::Level::Error) << "Encoding failed: " << vpx_codec_err_to_string(res) << std::endl;
		return;
	}
	vpx_codec_iter_t iter = NULL;
	const vpx_codec_cx_pkt_t *pkt = NULL;
	MessagePackets *packets = allocateAndConstruct<MessagePackets>();
	while ((pkt = vpx_codec_get_cx_data(&ctx, &iter)) != nullptr)
	{
		if (pkt->kind != VPX_CODEC_CX_FRAME_PKT)
		{
			continue;
		}
		Message::Packet packet;
		packet.source = source;
		Message::Frame v;
		const char *data = reinterpret_cast<const char *>(pkt->data.frame.buf);
		v.bytes = ByteList(data, pkt->data.frame.sz);
		v.width = vpx_img_plane_width(&image, 0);
		v.height = vpx_img_plane_height(&image, 0);
		packet.payload.emplace<Message::Video>(std::move(v));
		packets->emplace_back(std::move(packet));
	}
	SDL_Event e;
	e.type = SDL_USEREVENT;
	e.user.code = TemStreamEvent::SendMessagePackets;
	e.user.data1 = packets;
	e.user.data2 = nullptr;
	if (!tryPushEvent(e))
	{
		destroyAndDeallocate(packets);
	}
}
bool VPX::decode(ByteList &bytes)
{
	vpx_codec_err_t res =
		vpx_codec_decode(&ctx, reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size(), NULL, VPX_DL_REALTIME);
	if (res != VPX_CODEC_OK)
	{
		(*logger)(Logger::Level::Error) << "Failed to decode video: " << vpx_codec_err_to_string(res) << std::endl;
		return false;
	}
	vpx_codec_iter_t iter = NULL;
	vpx_image_t *img = NULL;
	bytes.reallocate(MB(8));

	while ((img = vpx_codec_get_frame(&ctx, &iter)) != NULL)
	{
		bytes.clear();
		for (int plane = 0; plane < 3; ++plane)
		{
			const char *buf = reinterpret_cast<const char *>(img->planes[plane]);
			const int stride = img->stride[plane];
			const int w = vpx_img_plane_width(img, plane) * ((img->fmt & VPX_IMG_FMT_HIGHBITDEPTH) ? 2 : 1);
			const int h = vpx_img_plane_height(img, plane);
			for (int y = 0; y < h; ++y)
			{
				bytes.append(buf, w);
				buf += stride;
			}
		}
	}
	return true;
}
unique_ptr<Video::EncoderDecoder> Video::createEncoder(Video::FrameData fd, const bool)
{
	VPX vpx;
	vpx_codec_enc_cfg_t cfg;
	if (vpx_img_alloc(&vpx.image, VPX_IMG_FMT_I420, fd.width, fd.height, 1) == nullptr)
	{
		(*logger)(Logger::Level::Error) << "Failed to allocate image" << std::endl;
		return nullptr;
	}

	(*logger)(Logger::Level::Trace) << "Encoder: " << vpx_codec_iface_name(codec_encoder_interface()) << std::endl;

	const vpx_codec_err_t res = vpx_codec_enc_config_default(codec_encoder_interface(), &cfg, 0);
	if (res)
	{
		(*logger)(Logger::Level::Error) << "Failed to get default video encoder configuration: "
										<< vpx_codec_err_to_string(res) << std::endl;
		return nullptr;
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
		return tem_unique<VPX>(std::move(vpx));
	}
	(*logger)(Logger::Level::Error) << "Failed to initialize encoder" << std::endl;
	return nullptr;
}
unique_ptr<Video::EncoderDecoder> Video::createDecoder()
{
	VPX vpx;
	struct vpx_codec_dec_cfg cfg;
	cfg.threads = std::thread::hardware_concurrency();
	if (vpx_codec_dec_init(&vpx.ctx, codec_decoder_interface(), &cfg, 0) == 0)
	{
		return tem_unique<VPX>(std::move(vpx));
	}
	(*logger)(Logger::Level::Error) << "Failed to initialize decoder" << std::endl;
	return nullptr;
}
ByteList resizePlane(const char *bytes, const uint32_t oldWidth, const uint32_t oldHeight, const uint32_t newWidth,
					 const uint32_t newHeight)
{
	ByteList b(newWidth * newHeight);
	for (uint32_t y = 0; y < newHeight; ++y)
	{
		const float ypercent = (float)y / (float)newHeight;
		const uint32_t yIndex = static_cast<uint32_t>(oldHeight * ypercent);
		for (uint32_t x = 0; x < newWidth; ++x)
		{
			const float xpercent = (float)x / (float)newWidth;
			const uint32_t xIndex = static_cast<uint32_t>(oldWidth * xpercent);

			b[x + y * newWidth] = bytes[xIndex + yIndex * oldWidth];
		}
	}
	return b;
}
} // namespace TemStream