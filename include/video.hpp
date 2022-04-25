#pragma once

#include <main.hpp>

namespace TemStream
{
using Dimensions = std::optional<std::pair<uint16_t, uint16_t>>;
class Video
{
	friend class Allocator<Video>;

	template <typename T, typename... Args> friend T *allocate(Args &&...args);

	friend class Deleter<Video>;

  private:
	Message::Source source;
	WindowProcess windowProcress;

	Video(const Message::Source &, const WindowProcess &);
	~Video();

  public:
	const WindowProcess &getInfo() const
	{
		return windowProcress;
	}

	const Message::Source &getSource() const
	{
		return source;
	}

	static void logDroppedPackets(size_t, const Message::Source &);

	struct Frame
	{
		Bytes bytes;
		uint32_t width;
		uint32_t height;

		template <typename T> uint8_t getPixel(T x, T y) const
		{
			x = std::clamp(x, 0, width - 1);
			y = std::clamp(y, 0, height - 1);
			return bytes[x + y * width];
		}

		void resize(uint32_t ratio);
		void resizeTo(uint32_t, uint32_t);
	};

	struct FrameData
	{
		uint32_t width;
		uint32_t height;
		int fps;
		int bitrateInMbps;
		int keyFrameInterval;
	};

	static WindowProcesses getRecordableWindows();
	static shared_ptr<Video> recordWindow(const WindowProcess &, const Message::Source &, const List<int32_t> &,
										  Video::FrameData);

	class VPX
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

		void encodeAndSend(const Bytes &, const Message::Source &);
		std::optional<Bytes> decode(const Bytes &);

		Dimensions getSize() const;

		int getWidth() const
		{
			return width;
		}
		void setWidth(int w)
		{
			width = w;
		}
		int getHeight() const
		{
			return height;
		}
		void setHeight(int h)
		{
			height = h;
		}

		static std::optional<VPX> createEncoder(FrameData);
		static std::optional<VPX> createDecoder();
	};

	class FrameEncoder
	{
	  private:
		ConcurrentQueue<shared_ptr<Frame>> frames;
		Message::Source source;
		const int32_t ratio;

		static void encodeFrames(shared_ptr<FrameEncoder> &&, FrameData);

	  public:
		FrameEncoder(const Message::Source &, int32_t);
		virtual ~FrameEncoder();

		void addFrame(shared_ptr<Frame>);

		Bytes resize(const Bytes &);
		Bytes encode(const Bytes &) const;

		static void startEncodingFrames(shared_ptr<FrameEncoder> &&, FrameData);
	};

	using FrameEncoders = std::vector<std::weak_ptr<FrameEncoder>>;

	template <typename T> class RGBA2YUV
	{
	  private:
		ConcurrentQueue<T> frames;
		FrameEncoders encoders;
		const Message::Source source;

		static void convertFrames(shared_ptr<RGBA2YUV> &&);

		virtual shared_ptr<Frame> convertToFrame(T &&) = 0;

	  public:
		RGBA2YUV(FrameEncoders &&frames, const Message::Source &source)
			: frames(), encoders(std::move(frames)), source(source)
		{
		}
		virtual ~RGBA2YUV()
		{
		}

		void addFrame(T &&t)
		{
			frames.push(std::move(t));
		}

		static void startConverteringFrames(shared_ptr<RGBA2YUV> &&ptr)
		{
			Task::addTask(std::async(TaskPolicy, convertFrames, std::move(ptr)));
		}
	};
};
template <typename T> void Video::RGBA2YUV<T>::convertFrames(shared_ptr<RGBA2YUV> &&ptr)
{
	(*logger)(Logger::Trace) << "Starting converter thread" << std::endl;
	try
	{
		using namespace std::chrono_literals;
		const auto maxWaitTime = 3s;
		while (!appDone)
		{
			auto result = ptr->frames.clearIfGreaterThan(5);
			if (result)
			{
				logDroppedPackets(*result, ptr->source);
			}

			auto data = ptr->frames.pop(maxWaitTime);
			if (!data)
			{
				break;
			}

			auto frame = ptr->convertToFrame(std::move(*data));
			if (!frame)
			{
				continue;
			}
			for (auto &encoder : ptr->encoders)
			{
				if (auto e = encoder.lock())
				{
					e->addFrame(frame);
				}
			}
		}
	}
	catch (const std::exception &e)
	{
		(*logger)(Logger::Error) << "Convert thread error: " << e.what() << std::endl;
	}
	(*logger)(Logger::Trace) << "Ending converter thread" << std::endl;
}
} // namespace TemStream