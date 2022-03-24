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
- [x] Use computer shader with VP8
- [ ] Figure out color issue with h.264 encoding (might be easier to seperate Y and UV calculation)
- [ ] Test on laptop

### Later
- [ ] Close command doesn't work when video streaming (a Window Manager issue, Need to change attributes on temp window or handle its events)
- [ ] Record webcam
- [ ] Implement stream recording and playback (need to timestamp messages)
- [ ] Create web page for GUI for native version
- [ ] Make layout message
- [ ] Make save configuration file based on current connected streams
- [ ] Make configuration files that also connects to streams on startup and sets display rects
- [ ] Add websocket connection (use proxy)
- [ ] SSL encryption (dtls, use proxy)

### Maybe
- [ ] Make plug in library for OBS to send video frames to server
- [ ] Add logging library (or make one)
