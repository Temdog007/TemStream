#include <main.hpp>

namespace TemStream
{
Video::Video(const Message::Source &source, const WindowProcess &wp) : source(source), windowProcress(wp)
{
}
Video::~Video()
{
}
} // namespace TemStream