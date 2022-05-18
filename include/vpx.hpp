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

#include <vpx/vp8cx.h>
#include <vpx/vp8dx.h>
#include <vpx/vpx_decoder.h>
#include <vpx/vpx_encoder.h>

namespace TemStream
{
class VPX : public Video::EncoderDecoder
{
  private:
	vpx_codec_ctx_t ctx;
	vpx_image_t image;
	int frameCount;
	int keyFrameInterval;

	VPX();

  public:
	~VPX();
	VPX(const VPX &) = delete;
	VPX(VPX &&);

	VPX &operator=(const VPX &) = delete;
	VPX &operator=(VPX &&);

	void encodeAndSend(ByteList &, const Message::Source &) override;
	bool decode(ByteList &) override;

	void swap(VPX &);

	friend unique_ptr<EncoderDecoder> Video::createEncoder(Video::FrameData, bool);
	friend unique_ptr<EncoderDecoder> Video::createDecoder();
};
} // namespace TemStream