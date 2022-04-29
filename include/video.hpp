#pragma once

#include <main.hpp>

namespace TemStream
{
using Dimensions = std::optional<std::pair<uint16_t, uint16_t>>;
#if TEMSTREAM_USE_OPENCV
using VideoCaptureArg = std::variant<int32_t, String>;
struct MakeVideoCapture
{
	cv::VideoCapture operator()(const String &s)
	{
		return cv::VideoCapture(cv::String(s));
	}
	cv::VideoCapture operator()(const int32_t index)
	{
		return cv::VideoCapture(index);
	}
};
extern std::ostream &operator<<(std::ostream &, const VideoCaptureArg &);
#endif
class Video
{
	friend class Allocator<Video>;

	template <typename T, typename... Args> friend T *allocate(Args &&...args);

	friend class Deleter<Video>;

  private:
	Message::Source source;
	const WindowProcess windowProcress;

	Video(const Message::Source &);
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
		ByteList bytes;
		uint32_t width;
		uint32_t height;
		uint32_t format;

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

		FrameData() : width(800), height(600), fps(24), bitrateInMbps(10), keyFrameInterval(120)
		{
		}
		~FrameData()
		{
		}

		void draw();
	};

	static WindowProcesses getRecordableWindows();
	static shared_ptr<Video> recordWindow(const WindowProcess &, const Message::Source &, int32_t, Video::FrameData);
	static shared_ptr<Video> recordWebcam(const VideoCaptureArg &, const Message::Source &, int32_t, Video::FrameData);

	class EncoderDecoder
	{
	  protected:
		int width;
		int height;

		EncoderDecoder() : width(0), height(0)
		{
		}

	  public:
		virtual ~EncoderDecoder()
		{
		}

		virtual void encodeAndSend(ByteList &, const Message::Source &) = 0;
		virtual bool decode(ByteList &) = 0;

		Dimensions getSize() const
		{
			return std::make_pair(width, height);
		}

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
	};

	static unique_ptr<EncoderDecoder> createEncoder(Video::FrameData, const bool forCamera = false);
	static unique_ptr<EncoderDecoder> createDecoder();

	class FrameEncoder
	{
	  private:
		ConcurrentQueue<Frame> frames;
		Message::Source source;
		const int32_t ratio;

		static void encodeFrames(shared_ptr<FrameEncoder>, FrameData, const bool forCamera);

	  public:
		FrameEncoder(const Message::Source &, int32_t);
		virtual ~FrameEncoder();

		void addFrame(Frame &&);

		ByteList resize(const ByteList &);
		ByteList encode(const ByteList &) const;

		static void startEncodingFrames(shared_ptr<FrameEncoder>, FrameData, const bool forCamera);
	};

	template <typename T> class RGBA2YUV
	{
	  private:
		ConcurrentQueue<T> frames;
		const Message::Source source;
		std::weak_ptr<FrameEncoder> encoder;

		static void convertFrames(shared_ptr<RGBA2YUV>);

		virtual std::optional<Frame> convertToFrame(T &&) = 0;

	  public:
		RGBA2YUV(std::shared_ptr<FrameEncoder> encoder, const Message::Source &source)
			: frames(), source(source), encoder(encoder)
		{
		}
		virtual ~RGBA2YUV()
		{
		}

		void addFrame(T &&t)
		{
			frames.push(std::move(t));
		}

		static void startConverteringFrames(shared_ptr<RGBA2YUV> ptr)
		{
			WorkPool::workPool.addWork([ptr]() { convertFrames(ptr); });
		}
	};
};
template <typename T> void Video::RGBA2YUV<T>::convertFrames(shared_ptr<RGBA2YUV> ptr)
{
	(*logger)(Logger::Trace) << "Starting converter thread" << std::endl;
	try
	{
		using namespace std::chrono_literals;
		while (!appDone)
		{
			auto result = ptr->frames.clearIfGreaterThan(20);
			if (result)
			{
				logDroppedPackets(*result, ptr->source);
			}

			auto data = ptr->frames.pop(3s);
			if (!data)
			{
				break;
			}

			auto frame = ptr->convertToFrame(std::move(*data));
			if (!frame)
			{
				continue;
			}
			if (auto e = ptr->encoder.lock())
			{
				e->addFrame(std::move(*frame));
			}
			else
			{
				break;
			}
		}
	}
	catch (const std::exception &e)
	{
		(*logger)(Logger::Error) << "Convert thread error: " << e.what() << std::endl;
	}
	(*logger)(Logger::Trace) << "Ending converter thread: " << ptr->source << std::endl;
}
} // namespace TemStream