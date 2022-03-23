
This program reads data from the microphone and sends it to a python server over TCP.
The program runs 5 loops, and on each loop it makes a connection to the server, then
reads data from the mic for 10 seconds, and as it reads it sends it to the server.
The server treats the new connection as a signal to start recording the data in
a new .WAV file, and it closes the file when the connection drops.
The sample rate for the mic read is set in src/main.cpp. This value is sent to the
server at the beginning of the TCP data exchange so that the server can set the value
in the .WAV file.

To run this you will need to do the following:

1. Setup the simple sound server (see the read me there) and start it running.
2. Find the ip address of the machine running that server
3. Edit src/main.cpp to set:
   * The ssid of your WiFi access point
   * The password of your WiFi access point
   * The TCP_ADDR that you recorded in step 2
4. pio run -t upload -t monitor

# Differences from other versions (remote, remote_async)

This version uses the FreeRTOS tasking mechanism to run the recording
operation in a separate task and pin that task to processor 1. This
reduces the chance that other things going on in the chip will
interfere with the record/send loop. In particular, WiFi operations
are pinned to processor 0, so this will help reduce the chance that
the socket operation will block the i2s_read operations long enough
to cause data to be missed.

Secondly, this version uses a bigger read buffer for i2s_read calls,
which my testing has shown greatly reduces the chance of data drop out.
I think that the idea is that you want to use a data buffer large enough
to contain all the data in all the configured DMA buffers, in case they
happen to be full when you call i2s_read. 

Additionally this version can be compiled for either 16 bits or 32 bits
per sample. The 32 bit version can still have an occasional drop out of
some sound data, based on my testing, but the 16 bit version never has.
The sound quality of 16 bit samples is quite high in my testing, easily
good enough for voice recording, which is my direct interest.

To select the bits per sample, look for this:
<pre><code>
#define BYTES_PER_SAMPLE sizeof(uint16_t)
//#define BYTES_PER_SAMPLE sizeof(uint32_t)
</code></pre>
and choose which one you want. If you use 16 bits, you need to run
the simple sound server like this:

$ python3 ../simple_sound/server/mic_server.py 2

For two bytes per sample.

Finally, this version uses the button to trigger and recover from deep sleep, allowing the user to restart the test run with two quick button presses with a short pause between presses (about a second).



