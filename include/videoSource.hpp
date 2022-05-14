#pragma once

#include <main.hpp>

namespace TemStream
{
using Dimensions = std::optional<std::pair<uint16_t, uint16_t>>;
using VideoCaptureArg = std::variant<int32_t, String>;

class VideoSource
{
	friend class Allocator<VideoSource>;

	template <typename T, typename... Args> friend T *allocate(Args &&...args);

	friend struct Deleter<VideoSource>;

  private:
	Message::Source source;
	const WindowProcess windowProcress;
	String name;
	bool running;

	VideoSource(const Message::Source &, String &&);
	VideoSource(const Message::Source &, const WindowProcess &);
	VideoSource(const Message::Source &, const Address &);

  public:
	~VideoSource();

	const static size_t MaxVideoPackets;
	const WindowProcess &getInfo() const
	{
		return windowProcress;
	}

	const String &getName() const
	{
		return name;
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
		std::optional<int> delay;
		int fps;
		int bitrateInMbps;
		int keyFrameInterval;
		int32_t scale;

		FrameData();
		~FrameData();

		void draw();
	};

	static WindowProcesses getRecordableWindows();
	static shared_ptr<VideoSource> recordWindow(const WindowProcess &, const Message::Source &, VideoSource::FrameData);
	static shared_ptr<VideoSource> recordWebcam(const VideoCaptureArg &, const Message::Source &,
												VideoSource::FrameData);
	static shared_ptr<VideoSource> listenToUdpPort(const Address &, const Message::Source &);
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

	static unique_ptr<EncoderDecoder> createEncoder(VideoSource::FrameData, const bool forCamera = false);
	static unique_ptr<EncoderDecoder> createDecoder();

	static int getFourcc();

	class FrameEncoder
	{
	  private:
		ConcurrentQueue<Frame> frames;
		FrameData frameData;
		TimePoint lastReset;
		unique_ptr<EncoderDecoder> encoder;
		shared_ptr<VideoSource> video;
		bool first;

		bool encodeFrames();

	  public:
		FrameEncoder(shared_ptr<VideoSource> v, FrameData, const bool forCamera);
		virtual ~FrameEncoder();

		void addFrame(Frame &&);

		ByteList resize(const ByteList &);
		ByteList encode(const ByteList &) const;

		static void startEncodingFrames(shared_ptr<FrameEncoder>);
	};

	struct Writer
	{
		cv::String filename;
		shared_ptr<cv::VideoWriter> writer;
		int32_t vidsWritten;
		int32_t framesWritten;

		Writer();
		~Writer();
	};

	static bool resetVideo(Writer &, shared_ptr<VideoSource>, FrameData);

	template <typename T> class RGBA2YUV
	{
	  protected:
		ConcurrentQueue<T> frames;
		FrameData frameData;
		shared_ptr<VideoSource> video;

		using Data = std::variant<Writer, std::weak_ptr<FrameEncoder>>;
		Data data;
		bool first;

		bool convertFrames(std::weak_ptr<FrameEncoder>);
		virtual std::optional<Frame> convertToFrame(T &&) = 0;

		bool handleWriter(Writer &);

		bool doWork()
		{
			if (auto ptr = std::get_if<std::weak_ptr<FrameEncoder>>(&data))
			{
				if (!convertFrames(*ptr))
				{
					*logger << "Ending RGBA to YUV converter: " << video->getSource() << std::endl;
					return false;
				}
			}
			else if (auto ptr = std::get_if<Writer>(&data))
			{
				if (!handleWriter(*ptr))
				{
					*logger << "Ending video converter: " << video->getSource() << std::endl;
					return false;
				}
			}
			return true;
		}

	  public:
		RGBA2YUV(std::shared_ptr<FrameEncoder> encoder, shared_ptr<VideoSource> video, FrameData frameData)
			: frames(), frameData(frameData), video(video), data(encoder), first(true)
		{
		}
		RGBA2YUV(shared_ptr<VideoSource> video, FrameData frameData)
			: frames(), frameData(frameData), video(video), data(Writer()), first(true)
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
			WorkPool::addWork([ptr]() { return ptr->doWork(); });
		}
	};
};
template <typename T> bool VideoSource::RGBA2YUV<T>::convertFrames(std::weak_ptr<FrameEncoder> encoder)
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
	catch (const std::bad_alloc &)
	{
		(*logger)(Logger::Level::Error) << "Ran out of memory while converting frames" << std::endl;
		return false;
	}
	catch (const std::exception &e)
	{
		(*logger)(Logger::Level::Error) << "Convert thread error: " << e.what() << std::endl;
		return false;
	}
	return true;
}
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
	VideoSource::FrameData frameData;
	VideoCaptureArg arg;
	Message::Source source;
	TimePoint nextFrame;
	std::shared_ptr<VideoSource> video;
	std::weak_ptr<VideoSource::FrameEncoder> encoder;
	bool first;

	bool execute();
};
template <typename T> bool VideoSource::RGBA2YUV<T>::handleWriter(VideoSource::Writer &w)
{
	if (first)
	{
		if (!VideoSource::resetVideo(w, video, frameData))
		{
			return false;
		}

		*logger << "Starting video converter: " << video->getSource() << std::endl;
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
			auto result = frames.clearIfGreaterThan(VideoSource::MaxVideoPackets);
			if (result)
			{
				VideoSource::logDroppedPackets(*result, video->getSource(), "VideoSource writer");
			}

			auto data = frames.pop(0s);
			if (!data)
			{
				return true;
			}

			{
				if (frameData.width != data->width || frameData.height != data->height)
				{
					frameData.width = data->width;
					frameData.height = data->height;
					if (!VideoSource::resetVideo(w, video, frameData))
					{
						return false;
					}
					continue;
				}
				cv::Mat image(data->height, data->width, CV_8UC4, xcb_get_image_data(data->reply.get()));
				cv::Mat output;
				if (frameData.scale == 100)
				{
					output = std::move(image);
				}
				else
				{
					const double scale = frameData.scale / 100.0;
					cv::resize(image, output, cv::Size(), scale, scale, cv::InterpolationFlags::INTER_AREA);
				}
				{
					cv::Mat dst;
					cv::cvtColor(output, dst, cv::COLOR_BGRA2BGR);
					output = std::move(dst);
				}
				*w.writer << output;
				++w.framesWritten;
			}

			if (w.framesWritten < frameData.fps * (*frameData.delay))
			{
				continue;
			}

			shared_ptr<cv::VideoWriter> oldVideo = tem_shared<cv::VideoWriter>();
			oldVideo.swap(w.writer);
			WorkPool::addWork([oldFilename = w.filename, video = this->video,
							   oldVideo = std::move(oldVideo)]() mutable {
				try
				{
					oldVideo->release();
					oldVideo.reset();

					const auto fileSize = fs::file_size(oldFilename);
					ByteList bytes(fileSize);
					{
						std::ifstream file(oldFilename.c_str(), std::ios::in | std::ios::binary);
						// Stop eating new lines in binary mode!!!
						file.unsetf(std::ios::skipws);

						(*logger)(Logger::Level::Trace) << "Saving file of size " << printMemory(fileSize) << std::endl;

						// reserve capacity
						bytes.reallocate(fileSize);
						bytes.append(std::istream_iterator<uint8_t>(file), std::istream_iterator<uint8_t>());
						if (fileSize != bytes.size())
						{
							(*logger)(Logger::Level::Warning) << "Failed to write entire video file. Wrote "
															  << bytes.size() << ". Expected " << fileSize << std::endl;
						}
					}

					auto packets = allocateAndConstruct<MessagePackets>();

					Message::prepareLargeBytes(bytes,
											   [&packets, &source = video->getSource()](Message::LargeFile &&lf) {
												   Message::Packet packet;
												   packet.source = source;
												   packet.payload.emplace<Message::Video>(std::move(lf));
												   packets->emplace_back(std::move(packet));
											   });

					SDL_Event e;
					e.type = SDL_USEREVENT;
					e.user.code = TemStreamEvent::SendMessagePackets;
					e.user.data1 = packets;
					e.user.data2 = nullptr;
					if (!tryPushEvent(e))
					{
						destroyAndDeallocate(packets);
					}

					fs::remove(oldFilename);
				}
				catch (const std::bad_alloc &)
				{
					(*logger)(Logger::Level::Error) << "Ran out of memory saving video file" << std::endl;
				}
				catch (const std::exception &e)
				{
					(*logger)(Logger::Level::Error) << e.what() << std::endl;
				}
				return false;
			});
			++w.vidsWritten;
			w.framesWritten = 0;
			if (!VideoSource::resetVideo(w, video, frameData))
			{
				return false;
			}
		}
	}
	catch (const std::bad_alloc &)
	{
		(*logger)(Logger::Level::Error) << "Ran out of memory" << std::endl;
		return false;
	}
	catch (const std::exception &e)
	{
		(*logger)(Logger::Level::Error) << "Convert thread error: " << e.what() << std::endl;
		return false;
	}
	return true;
}
} // namespace TemStream