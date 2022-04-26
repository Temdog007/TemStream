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
class OpenH264 : public Video::EncoderDecoder
{
  private:
	using Decoder = std::unique_ptr<ISVCDecoder, DecoderDeleter>;
	using Encoder = std::unique_ptr<ISVCEncoder, EncoderDeleter>;

	std::variant<Decoder, Encoder> data;

	OpenH264(Encoder &&, int, int);
	OpenH264(Decoder &&);

	friend class Allocator<OpenH264>;

  public:
	~OpenH264();

	void encodeAndSend(ByteList &, const Message::Source &) override;
	bool decode(ByteList &) override;

	friend unique_ptr<EncoderDecoder> Video::createEncoder(Video::FrameData, bool);
	friend unique_ptr<EncoderDecoder> Video::createDecoder();
};
} // namespace TemStream