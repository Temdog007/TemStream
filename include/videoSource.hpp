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

namespace TemStream
{
using Dimensions = std::optional<std::pair<uint16_t, uint16_t>>;
using VideoCaptureArg = std::variant<int32_t, String>;

class Screenshot
{
  protected:
	uint16_t width;
	uint16_t height;

  public:
	Screenshot() : width(0), height(0)
	{
	}
	virtual ~Screenshot()
	{
	}

	uint16_t getWidth() const
	{
		return width;
	}
	uint16_t getHeight() const
	{
		return height;
	}

	void setWidth(uint16_t w)
	{
		width = w;
	}
	void setHeight(uint16_t h)
	{
		height = h;
	}

	constexpr uint32_t getSize() const
	{
		return width * height * 4u;
	}

	virtual uint8_t *getData() = 0;
};
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

	class RGBA2YUV
	{
	  protected:
		ConcurrentQueue<unique_ptr<Screenshot>> frames;
		FrameData frameData;
		ByteList temp;
		shared_ptr<VideoSource> video;

		using Data = std::variant<Writer, std::weak_ptr<FrameEncoder>>;
		Data data;
		bool first;

		bool convertFrames(std::weak_ptr<FrameEncoder>);
		std::optional<Frame> convertToFrame(unique_ptr<Screenshot> &&);

		bool handleWriter(Writer &);

		bool doWork();

	  public:
		RGBA2YUV(shared_ptr<FrameEncoder> encoder, shared_ptr<VideoSource> video, FrameData frameData);
		RGBA2YUV(shared_ptr<VideoSource> video, FrameData frameData);
		~RGBA2YUV();

		void addFrame(unique_ptr<Screenshot> &&t)
		{
			frames.push(std::move(t));
		}

		static void startConverteringFrames(shared_ptr<RGBA2YUV> ptr)
		{
			WorkPool::addWork([ptr]() { return ptr->doWork(); });
		}
	};
};
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
using Converter = VideoSource::RGBA2YUV;
} // namespace TemStream