
This program reads data from the microphone and sends it to a python server over TCP.
The program runs 5 loops, and on each loop it makes a connection to the server, then
read data from the mic for 5 seconds, and as it reads it sends it to the server.
The server treats the new connection as a signal to start recording the data in
a new .WAV file, and it closes the file when the connection drops.
The sample rate for the mic read is set in src/main.cpp. This value is sent to the
server at the beginning of the TCP data exchange so that the server can set the value
in the .WAV file.

To run this you will need to do the following:

1. Setup the simple sound server (see the read me there) and start it running.
2. Find the ip address of the machine running that server
3. Edit src/main.cpp to set:
   a. The ssid of your WiFi access point
   b. The password of your WiFi access point
   c. The TCP_ADDR that you recorded in step 2
4. pio run -t upload -t monitor

