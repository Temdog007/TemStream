# TemStream

TemStream is a streaming application designed for streaming to different sources with different data types. 

## Example

Insert screenshot here

## Components

### Client
Sends and receives data to and from servers

### Server
Distributes information to clients. Each server only handles 1 type of [data stream](#data-streams).

## Data Streams

### Text
Simple text stream. This is intended for stream descriptions. Only one text message is displayed at a time.

### Chat
For communicating with other clients. All chat messages are stored and displayed.

### Image
For displaying images.

#### Supported Image Formats

- PNG
- JPEG
- BMP
- XPM

### Audio
Clients encodes audio using the [Opus Codec](https://opus-codec.org/) and sends the packets to the server. Server distribute packets to other clients. Those clients will then decode and play the audio.

**NOTE**: *If using the GUI to send audio, audio can be recorded from any capture device detected on the client machine. Audio can also be recorded from a running process but only for **Linux** machines that are using [PulseAudio](https://www.freedesktop.org/wiki/Software/PulseAudio/).*

### Video
Clients can either send a video file or an video packet encoded with [VP8 Codec](https://en.wikipedia.org/wiki/VP8) or the [OpenH264 Codec](http://www.openh264.org/). 
**NOTE**: *Audio is never sent with video. So, audio from video files will never be played.*

### Links
Link servers will provide a list of servers that the client can connect to.

## Usage

### Client

The TemStream client comes with a GUI to connect, stream, and receive data to and from servers.

### Servers

A TemStream server has a series of arguments to define its usage.

| Argument | Short | Long | Description |
| :--------: | :-----: | :----: | ----------- |
| Hostname | `-H`    | `--hostname` | The hostname to use when opening the server's socket |
| Port     | `-P` | `--port` | The port number to use when opening the server's socket |
| Certificate | `-CT` | `--certificate` | The certificate file to be used when creating an SSL socket. The key file must also be defined if this argument is defined |
| Key | `-K` | `--key` | The key file to be used when creating an SSL socket. The cerfiticate file must also be defined if this argument is defined |
| Name | `-N` | `--name` | The name of the server |
| Set as Text server | `-T` | `--text` | Define this server as a text server |
| Set as Chat server | `-C` | `--chat` | Define this server as a chat server |
| Set as Image server | `-I` | `--image` | Define this server as an image server |
| Set as Video server | `-V` | `--video` | Define this server as a video server |
| Set as Audio server | `-A` | `--audio` | Define this server as an audio server |
| Set as Links server | `-L` | `--links` | Define this server as a link server |
| Memory | `-M` | `--memory` | The maximum amount of memory the server can use. This is only applicable if the server was compiled custom memory allocation enabled |
| Max Clients | `-MC` | `--max-clients` | The maximum number of clients that the server will accept
| Max Message Size | `-MS` | `--max-message-size` | The maximum size a message from a client can be. If client sends a message greater than this, that client will be disconnected.|
| Message Rate | `-MR` | `--message-rate` | The rate of messages that clients should be sending at. If client sends messages beyond the message rate, that client will be disconnected.|
| Record? | `-R` | `--record` | If this is set, all data messages (i.e. audio messages for audio streams) will be saved to a file. This file will then be used to support replay for clients. |
| Ban List | `-B` | `--banned` | A file that contains a list of users (separated by a newline character) that are banned from connecting to this server. This will overwrite the allowed list if defined |
| Allow List | `-AL` | `--allowed` | A file that contains a list of users (separated by a newline character) that are allowed to connect to this server. This will overwrite the ban list if defined |

## Compiling

TemStream uses CMake to handle builds. Typical build instructions use the following:

```bash
mkdir build
cd build
cmake ../
make
```

While this repository uses submodules to clone 3rd party dependencies, it may be easier to have the dependencies installed with a package manager (i.e. vpckg, aptitude, etc)

### 3rd Party Dependencies

- [SDL2](https://github.com/libsdl-org/SDL)
- SDL2 image
- Freetype
- [ImGui](https://github.com/ocornut/imgui)
- OpenCV
- OpenSSL
- Opus
- OpenH264
- VPX
- [Cereal](https://uscilab.github.io/cereal/)

### License

TemStream is licensed under [GNU GPLv2](LICENSE.txt).