#pragma once

#include <main.hpp>

namespace TemStream
{
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

	static WindowProcesses getRecordableWindows();
	static shared_ptr<Video> recordWindow(const WindowProcess &, const Message::Source &, const List<int32_t> &,
										  const uint32_t fps);

	using Frame = shared_ptr<Bytes>;
	class FrameEncoder
	{
	  private:
		ConcurrentQueue<Frame> frames;
		Message::Source source;
		const float ratio;

		static void encodeFrames(shared_ptr<FrameEncoder> &&);

	  public:
		FrameEncoder(const Message::Source &, float);
		~FrameEncoder();

		void addFrame(Frame);

		static void startEncodingFrames(shared_ptr<FrameEncoder> &&);
	};

	using FrameEncoders = std::vector<std::weak_ptr<FrameEncoder>>;

	template <typename T> class RGBA2YUV
	{
	  private:
		ConcurrentQueue<T> frames;
		FrameEncoders encoders;

		static void convertFrames(shared_ptr<RGBA2YUV> &&ptr)
		{
			(*logger)(Logger::Trace) << "Starting converter thread" << std::endl;
			using namespace std::chrono_literals;
			const auto maxWaitTime = 1s;
			while (!appDone)
			{
				auto data = ptr->frames.pop(maxWaitTime);
				if (!data)
				{
					break;
				}

				// Convert and send to encoders
			}
			(*logger)(Logger::Trace) << "Ending converter thread" << std::endl;
		}

	  public:
		RGBA2YUV(FrameEncoders &&frames) : frames(), encoders(std::move(frames))
		{
		}
		~RGBA2YUV()
		{
		}

		void addFrame(T &&t)
		{
			frames.push(std::move(t));
		}

		static void startConverteringFrames(shared_ptr<RGBA2YUV> &&ptr)
		{
			std::thread thread(convertFrames, std::move(ptr));
			thread.detach();
		}
	};
};
} // namespace TemStream