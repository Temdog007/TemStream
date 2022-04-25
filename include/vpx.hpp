#include <main.hpp>

namespace TemStream
{
class VPX : public Video::EncoderDecoder
{
  private:
	vpx_codec_ctx_t ctx;
	vpx_image_t image;
	int frameCount;
	int keyFrameInterval;
	int width;
	int height;

  public:
	VPX();
	VPX(const VPX &) = delete;
	VPX(VPX &&);
	~VPX();

	VPX &operator=(const VPX &) = delete;
	VPX &operator=(VPX &&);

	void encodeAndSend(const ByteList &, const Message::Source &) override;
	std::optional<ByteList> decode(const ByteList &) override;

	Dimensions getSize() const override;

	int getWidth() const override
	{
		return width;
	}
	void setWidth(int w) override
	{
		width = w;
	}
	int getHeight() const override
	{
		return height;
	}
	void setHeight(int h) override
	{
		height = h;
	}

	void swap(VPX &);

	friend unique_ptr<EncoderDecoder> Video::createEncoder(Video::FrameData);
	friend unique_ptr<EncoderDecoder> Video::createDecoder();
};
} // namespace TemStream