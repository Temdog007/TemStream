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
- [ ] Use custom allocator for ENet
- [ ] Include press to talk
- [ ] Test changing font
- [x] Allow playback device to be changed
- [x] Audio stream (allow playback and recording device to be selected)
- [x] Add no display mode
- [x] Show message box when fatal errors occur (ex. disconnect from server)

### Later
- [ ] Implement stream recording and playback (need to timestamp messages)
- [ ] Allow for blacklist/whitelist for stream access
- [ ] Allow owner to add/remove writers/readers
- [ ] Include FFMPEG
    - [ ] Play gifs
    - [ ] Compress audio
    - [ ] Record windows
    - [ ] Record screen
    - [ ] Record webcam
- [ ] Mix audio properply
- [ ] Create web page for GUI
- [ ] Make layout message
- [ ] Make save configuration file based on current connected streams
- [ ] Make configuration files that also connects to streams on startup and sets display rects
- [ ] Add websocket connection
- [ ] SSL encryption (dtls)

### Maybe
- [ ] Add logging library (or make one)
