#pragma once

#include <main.hpp>

namespace TemStream
{
using Dimensions = std::optional<std::pair<uint16_t, uint16_t>>;
#if TEMSTREAM_USE_OPENCV
using VideoCaptureArg = std::variant<int32_t, String>;
#endif
class Video
{
	friend class Allocator<Video>;

	template <typename T, typename... Args> friend T *allocate(Args &&...args);

	friend class Deleter<Video>;

  private:
	Message::Source source;
	const WindowProcess windowProcress;
	bool running;

	Video(const Message::Source &);
	Video(const Message::Source &, const WindowProcess &);
	~Video();

  public:
	const static size_t MaxVideoPackets;
	const WindowProcess &getInfo() const
	{
		return windowProcress;
	}

	const Message::Source &getSource() const
	{
		return source;
	}

	void setRunning(const bool b)
	{
		running = b;
	}
	bool isRunning() const
	{
		return running;
	}

	static void logDroppedPackets(size_t, const Message::Source &, const char *);

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
		bool jpegCapture;

		FrameData() : width(800), height(600), fps(24), bitrateInMbps(10), keyFrameInterval(120), jpegCapture(false)
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
		FrameData frameData;
		TimePoint lastReset;
		unique_ptr<EncoderDecoder> encoder;
		shared_ptr<Video> video;
		const int32_t ratio;
		bool first;

		bool encodeFrames();

	  public:
		FrameEncoder(shared_ptr<Video> v, int32_t, FrameData, const bool forCamera);
		virtual ~FrameEncoder();

		void addFrame(Frame &&);

		ByteList resize(const ByteList &);
		ByteList encode(const ByteList &) const;

		static void startEncodingFrames(shared_ptr<FrameEncoder>);
	};

	template <typename T> class RGBA2YUV
	{
	  protected:
		ConcurrentQueue<T> frames;
		std::shared_ptr<Video> video;
		std::weak_ptr<FrameEncoder> encoder;
		bool first;

		bool convertFrames();
		virtual bool convertToJpeg() = 0;
		virtual std::optional<Frame> convertToFrame(T &&) = 0;

	  public:
		RGBA2YUV(std::shared_ptr<FrameEncoder> encoder, shared_ptr<Video> video)
			: frames(), video(video), encoder(encoder), first(true)
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
			if (ptr->encoder.expired())
			{
				WorkPool::workPool.addWork([ptr]() {
					if (!ptr->convertToJpeg())
					{
						*logger << "Ending converting: " << ptr->video->getSource() << std::endl;
						return false;
					}
					return true;
				});
			}
			else
			{
				WorkPool::workPool.addWork([ptr]() {
					if (!ptr->convertFrames())
					{
						*logger << "Ending RGBA to YUV converter: " << ptr->video->getSource() << std::endl;
						return false;
					}
					return true;
				});
			}
		}
	};
};
template <typename T> bool Video::RGBA2YUV<T>::convertFrames()
{
	if (first)
	{
		*logger << "Starting RGBA to YUV converter: " << video->getSource() << std::endl;
		first = false;
	}
	if (!video->isRunning())
	{
		return false;
	}
	try
	{
		using namespace std::chrono_literals;
		while (!appDone)
		{
			auto result = frames.clearIfGreaterThan(MaxVideoPackets);
			if (result)
			{
				logDroppedPackets(*result, video->getSource(), "BGRA to YUV converter");
			}

			auto data = frames.pop(0s);
			if (!data)
			{
				return true;
			}

			auto frame = convertToFrame(std::move(*data));
			if (!frame)
			{
				return true;
			}
			if (auto e = encoder.lock())
			{
				e->addFrame(std::move(*frame));
			}
			else
			{
				return false;
			}
		}
	}
	catch (const std::exception &e)
	{
		(*logger)(Logger::Error) << "Convert thread error: " << e.what() << std::endl;
		return false;
	}
	return true;
}
#if TEMSTREAM_USE_OPENCV
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
struct WebCamCapture
{
	cv::VideoCapture cap;
	cv::Mat image;
	Video::FrameData frameData;
	VideoCaptureArg arg;
	Message::Source source;
	std::chrono::_V2::system_clock::time_point nextFrame;
	std::shared_ptr<Video> video;
	std::weak_ptr<Video::FrameEncoder> encoder;
	bool first;

	bool execute();
};
#endif
} // namespace TemStream