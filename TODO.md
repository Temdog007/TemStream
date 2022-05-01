### Feb 22, 2022
- [x] Verify read/write access for streams
- [x] Chat stream
    - [x] Move chat index
    - [x] Color time, username, and message differently
    - [x] Shaded background
- [x] Show last N number of chat messages

### Feb 23, 2022
- [x] Fix auto scroll
- [x] Handle dropped text
- [x] Image stream
- [x] Handle jpeg and webp

### Feb 24, 2022
- [x] File drop onto stream texture

### Feb 25, 2022
- [x] Download screenshot of stream
- [x] Retest chat
- [x] Make thread safe allocator
- [x] Packet loss (TCP merges packets, use ENET)

### Feb 26, 2022
- [x] Add threads to client
- [x] Use custom allocator for ENet
- [x] Test changing font
- [x] Allow playback device to be changed
- [x] Audio stream (allow playback and recording device to be selected)
- [x] Add no display mode
- [x] Show message box when fatal errors occur (ex. disconnect from server)

### Feb 27, 2022
- [x] Compress audio
- [x] Check when to render scene
- [x] Make left over audio bytes for capture audio

### Feb 28, 2022
- [x] Ensure each audio stream has its own playback device id (remove global playback)

### Mar 1, 2022
- [x] Don't send audio packets to main thread/handle immediately
- [x] Communicate with user input thread to prompt user for input
- [x] Fix user input stops working when playing audio

### Mar 02, 2022 - Mar 06, 2022
- [x] Rewrite message protocol to send data in chunks
- [x] Ensure each stream is its own process
- [x] Make chat work
- [x] Make audio work
- [x] Make image work
- [x] Cleanup servers in redis
- [x] Make server store information on file (give information on client connection)
- [x] Print TTF and IMG versions

### Mar 07, 2022
- [x] Fix memory leaks (leaks when client joins server, leaks when client uploads image file)
- [x] Make large images not crash server (write image data to file)
- [x] Ensure clients cannot slow down server if they don't have write access (use intercept callback in ENetHost)

### Mar 08, 2022
- [x] Stream wav file
- [x] Stream ogg with lib vorbis

### Mar 09, 2022
- [x] Stream mp3 
- [x] Audio sent over stream doesn't play pass one second (encoding issue? fill with silence if packet too small; use reliable send for audio files)
- [x] Ensure that taking too long to answer questions doesn't disconnect from server (audio server issue only; send playback/record questions to user input list)
- [x] Fix quick remove

### Mar 11, 2022
- [x] Seperate user input and main threads
- [x] Display audio mixer
- [x] Render label with display is selected

### Mar 12, 2022
- [x] Allow canceling current menu selection
- [x] Write key in redis to signal new stream so lobby can update clients quickly
- [x] Ensure textures can be hidden for a stream
- [x] Update audio texture (don't delete)
- [x] Allow volume of playback to be changed
- [x] Server isn't sending data to all peers

### Mar 13, 2022
- [x] Fix audio not playing on separate clients
- [x] Allow audio source/destinations to be changed

### Mar 14, 2022
- [x] Put stream files in a directory
- [x] Allow press to talk to be set for recording microphone
- [x] Discard audio frames if they are mostly silence
- [x] Allow press to talk and silence to be changed during execution
- [x] Make authentication work as a plugin

### Mar 16, 2022
- [x] Record window audio
- [x] Test from another computer

### Mar 16, 2022
- [x] Make circular queue for audio
- [x] Weird random files were showing up in directory (from VS code; something about keyring)

### Mar 19, 2022
- [x] Record window display

### Mar 20, 2022
- [x] Improve video recording performance (deemed to complex)

### Mar 21, 2022
- [x] Remove video streaming code (opencv)
- [x] Use multi-threading to encode video frames faster (is already done)

### Mar 22, 2022
- [x] Make compute shader for color conversion for texture
- [x] Implement h.264 video encoding

### Mar 23, 2022
- [x] Use compute shader with VP8
- [x] Figure out color issue with h.264 encoding (might be easier to seperate Y and UV calculation)

### Mar 24, 2022
- [x] Close command doesn't work when video streaming (replace compute shader with OpenCL)
- [x] Allow resizing of streamed window
- [x] Fix image with OpenCL conversion

### Mar 25, 2022
- [x] Use jpeg turbo for jpeg files (no changes needed?)
- [x] Limit incoming and outgoing packets
- [x] Test on laptop (memory issues were occuring when video frames couldn't be processed fast enough)

### Mar 26, 2022
- [x] Record webcam

### Mar 27, 2022 - Mar 28, 2022
- [x] Allow video streams have scale attribute
- [x] Use linear interpolation for scaling
- [x] Use shared memory for screen recording
- [x] Test lower qualities for laptop
- [x] Make mouse not visible if not moved for period of time

### Mar 30, 2022 - Apr 02, 2022
- [x] Implement stream recording and playback (need to timestamp messages)
- [x] Implement replay stream
- [x] Make replay stream send time range 
- [x] Make replay handler to request data from replay stream peridically

### Apr 03, 2022
- [x] Add OpenH264

### Apr 06, 2022 - Apr 09, 2022
- [x] Write own Ui code
- [x] Display audio states
- [x] Fix background not showing when ui is displayed
- [x] Allow displays to be toggled in GUI

### Apr 10, 2022
- [x] Add --no-tui option (default false)
- [x] Allow video to be sent in GUI
- [x] Record audio from windows
- [x] Allow audio to be sent in GUI
- [x] Don't create streams from client

### Apr 11, 2022
- [x] Send chat rejected message on bad chat messages
- [x] Assign role to client (Producer, Consumer, Admin); Allow certain messages based on role
- [x] Separate client and server code
- [x] Remove read/write access list

### Apr 12, 2022
- [x] Add z-order to stream displays
- [x] Inline font file as a default
- [x] Fix chat sender and re-test
- [x] Redo chat logs to not need to loaded in memory

### Apr 13, 2022
- [x] Make layout message
- [x] Fix user input freezing app when run in background

### Apr 15, 2022
- [x] Rewrite
    - [x] C++ (remove all references to TemLang code)
    - [x] Remove ENet; Just use TCP
    - [x] Remove UI; Just use ImGui
    - [x] Add gitmodules for 3rd dependencies

### Apr 16, 2022
- [x] Socket interface
- [x] Add default font (Ubunutu font)
- [x] Allow font to be changed
- [x] Send text to server
- [x] Process text from server
- [x] Render text stream

### Apr 17, 2022
- [x] Custom allocator
- [x] Stream name not showing in window
- [x] Images not loading
- [x] Handle message sending in a separate thread
- [x] Make non-imgui displays movable
- [x] Make non-imgui displays resizable
- [x] Make displays hideable (menu bar)
- [x] Create logger interface (log to console for server; window for gui; remove message boxes; show log window on error)

### Apr 18, 2022
- [x] Show colored text to signal if connected to stream
- [x] Use image button (SDL_Texture should work)
- [x] On dropped file, automatically create the appropriate query data
- [x] Allow title bar and resize to be toggled for stream displays
- [x] Display all font characters
- [x] Handle audio streaming
- [x] Ensure new stream displays have a minimum window size
- [x] Fix audio playback

### Apr 19, 2022
- [x] Remove message flush; use sdl event to handle messages from server; send messages immediately
- [x] Improve audio delay (didn't encode all audio on callback)
- [x] Allow playback device to be changed
- [x] Show error when disconnected from server unexpectedly
- [x] Add context menu to stream displays
- [x] Show specfic error when connecting to server fails before getting information
- [x] Handle streaming audio from windows
- [x] Add file browser
- [x] Add keyboard shortcuts
- [x] Implement screenshot (and text copy)

### Apr 20, 2022
- [x] Ensure smart pointers use custom allocator if enabled
- [x] Change image message to tell client how be image will be
- [x] Make work queue
- [x] Make audio device start as work
- [x] Log error if tried to send message when not connected to peer
- [x] Make client implicitly create a stream
- [x] Make server store list of streams
- [x] Make client subscribe to stream before sending data to it
- [x] Make displays that don't match to a stream disappear

### Apr 21, 2022
- [x] Store image and text data to hard disk (send to newly subscribed clients)
- [x] Make configuration structs/files
    Limit how many streams a client can create (default 10)
    Limit max message size
    Optional stream recording
- [x] Update build for server to not include imgui and sdl
- [x] Make server use configuration
- [x] Replace peer information with credentials

### Apr 22, 2022
- [x] Cleanup showDisplays window
- [x] Add silence thresold, default volume and default st
- [x] Allow for custom colors
- [x] Make custom allocator optional
- [x] Fix JSON serializing
- [x] Label ImGui Windows
- [x] Allow multiple transcodes for each video stream
- [x] Render audio channels separately

### Apr 23, 2022 - Apr 24, 2022
- [x] Window streaming
- [x] Make thread to handle decoding (all frames must be decoded in order)
- [x] Handle texture resizing
- [x] Optional OpenCV
- [x] Make byte list class

### Apr 25, 2022
- [x] Make logs scroll to bottom
- [x] Implement OpenH264 for video
- [x] Delete old C code
- [x] Only have one scaling for video stream
- [x] Don't send encoded video stream to streaming. Just use raw image.
- [x] Webcam streaming
- [x] Video file streaming

### Apr 26, 2022 - Apr 29, 2022
- [x] Add Thread pool
- [x] Allow memory size to be reset during runtime
- [x] Make work pool usuable for single threaded use
- [x] Connection is randomly lost when sending too much data (Message size is sent wrong or read wrong. Handle issue without ending connection)
- [x] Fix issues found when testing on laptop
    - [x] Playing audio and video at the same time on laptop causes disconnection (see above)
    - [x] Restart encoder periodically

### Apr 30, 2022
- [x] Read from socket a number of times before leaving read function
- [x] Add optional MJPEG video streaming
- [x] Make OpenCV and SDL_Image use the same version of libjpeg9
- [x] Add header to all packets sent

### May 1, 2022
- [ ] Make each stream its own process (with its own configuration)
- [ ] Make video streams
- [ ] Create re-stream app to subscribe to video stream, scale (based on config), and stream
- [ ] Test with multiple servers
- [ ] Have server send list of servers back to client. Allow client to select a new server to connect to 

### May 2, 2022
- [ ] Make server write all messages to hard disk when recording
- [ ] Add replay streams that only accept replay messages
- [ ] Add message to get stream time range
- [ ] Add message to get recorded messages at time stamp

### May 3, 2022
- [ ] Validate stream names/client names
- [ ] Create layout message that applies to user's streams
- [ ] Allow layout message to be saved/loaded
- [ ] Add configuration for imgui style and fonts

### May 4, 2022
- [ ] Add comments to code
- [ ] Implement SSL socket
- [ ] Implement web socket
- [ ] Test browser
- [ ] Test android

### Bugs
- [x] Connecting to multiple streams quickly causes some displays to not show
- [x] Deadlock when switching between ui and no ui quickly while watching video stream with audio
- [x] Custom allocator deadlocks (size_t is num of objects to allocate; not size)
- [x] Sinks not being unloaded (need to call destructor in custom deleter)
- [x] Memory leak when streams (removed shared pointers so most likely that was the issue)
- [?] Audio clipping when network issue (don't know how to fix; Release mode doesn't fix)
- [?] Message size is sometimes wrong when server sends data to client (don't know what causes this issue)

### Maybe
- [ ] Make plug in library for OBS to send video and audio to server
- [ ] Fix loading tga files
- [ ] Make refresh button for clients, streams, servers, etc (this shouldn't be necessary. Server always updates clients when needed)