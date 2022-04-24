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

	struct Frame
	{
		Bytes bytes;
		uint16_t width;
		uint16_t height;
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

		static std::optional<VPX> createEncoder(FrameData);
		static std::optional<VPX> createDecoder();
	};

	class FrameEncoder
	{
	  private:
		ConcurrentQueue<shared_ptr<Frame>> frames;
		Message::Source source;
		const float ratio;

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

		virtual shared_ptr<Frame> convertToFrame(T &&) const = 0;

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
	using namespace std::chrono_literals;
	const auto maxWaitTime = 3s;
	while (!appDone)
	{
		// If too many frames; start dropping them.
		ptr->frames.use([&ptr](Queue<T> &queue) {
			const size_t size = queue.size();
			if (size > 5)
			{
				(*logger)(Logger::Warning) << "Dropping " << size << " video frames from " << ptr->source << std::endl;
				Queue<T> empty;
				queue.swap(empty);
			}
		});

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
	(*logger)(Logger::Trace) << "Ending converter thread" << std::endl;
}
} // namespace TemStream