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
- [ ] Audio stream

### Later
- [ ] Allow for blacklist/whitelist for stream access
- [ ] Allow owner to add/remove writers/readers
- [ ] Play gifs (use FFMPEG)
- [ ] Video stream
- [ ] Make layout message
- [ ] Make save configuration file based on current connected streams
- [ ] Make configuration files that also connects to streams on startup and sets display rects
- [ ] Create an HTML page that allows browser GUI to send commands on behalf of the native version
- [ ] Add websocket connection
- [ ] SSL encryption (dtls)
- [ ] Add logging library (or make one)
