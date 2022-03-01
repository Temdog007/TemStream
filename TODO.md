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
- [ ] Fix audio not working when long delay between audio packets
- [ ] use FFMPEG for audio stuff to handle streaming/converting audio files to PCM

### Later
- [ ] Display audio mixer
- [ ] Play gifs
- [ ] Record windows
- [ ] Record screen
- [ ] Record webcam

- [ ] Ensure textures can be hidden for a stream
- [ ] Implement stream recording and playback (need to timestamp messages)
- [ ] Allow for blacklist/whitelist for stream access
- [ ] Allow owner to add/remove writers/readers
- [ ] Include press to talk
- [ ] Create web page for GUI for native version
- [ ] Make layout message
- [ ] Make save configuration file based on current connected streams
- [ ] Make configuration files that also connects to streams on startup and sets display rects
- [ ] Add websocket connection
- [ ] SSL encryption (dtls)

### Maybe
- [ ] Add logging library (or make one)
