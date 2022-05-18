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

#pragma once

#include <main.hpp>

#include <openh264/codec/api/svc/codec_api.h>

namespace TemStream
{
struct DecoderDeleter
{
	void operator()(ISVCDecoder *d) const;
};
struct EncoderDeleter
{
	void operator()(ISVCEncoder *e) const;
};
class OpenH264 : public VideoSource::EncoderDecoder
{
  private:
	using Decoder = std::unique_ptr<ISVCDecoder, DecoderDeleter>;
	using Encoder = std::unique_ptr<ISVCEncoder, EncoderDeleter>;

	std::variant<Decoder, Encoder> data;
	int32_t decodingFails;

	OpenH264(Encoder &&, int, int);
	OpenH264(Decoder &&);

	friend class Allocator<OpenH264>;

  public:
	~OpenH264();

	void encodeAndSend(ByteList &, const Message::Source &) override;
	bool decode(ByteList &) override;

	friend unique_ptr<EncoderDecoder> VideoSource::createEncoder(VideoSource::FrameData, bool);
	friend unique_ptr<EncoderDecoder> VideoSource::createDecoder();
};
} // namespace TemStream