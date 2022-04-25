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

	Dimensions getSize() const override;

	void swap(VPX &);

	friend unique_ptr<EncoderDecoder> Video::createEncoder(Video::FrameData, bool);
	friend unique_ptr<EncoderDecoder> Video::createDecoder();
};
} // namespace TemStream