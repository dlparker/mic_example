# mic_example
TTGO T-Watch version 3 microphone example

A couple of simple programs for version 3 of the T-Watch, exploring how to use the microphone. 
One program just reads mic data for 5 seconds and reports the amount of data read, doing this 5 times.
The other program does the same sequence, but sends the data to a python TCP server that saves the data in a .WAV file, which you can then play. 
The python program can be found in the simple_sound_server directory. 

If you are running linux, you can play the resulting .wave files using "aplay".

Note that in my testing, the process of sending data over WiFi seems to sometimes cause data to be missed. You can see this when examining either the twatch program serial output or the sound server serial output. Both of these calculate the duration of the recorded sound and print that, along with the elapsed time of the recording process. These two values should be very close, but sometimes they are not. 

I speculate that the WiFi operations sometimes run at a higher priorty than the sound recording operations, and are busy enough that the sound recording task doesn't get to run soon enough to read the sound data before the next DMA operation overwrites the DMA buffers. I hope I can interest someone in investigating this, someone who has better tools than I do to study what is going on. It is possible that some fiddling at the FreeRTOS level will improve the performance.

# Update

I adapted the remote (TCP) version of the program to make a new one that uses the AsyncTCP library. After a couple hundred test cycles, I have never seen the skipped read problem. This seems to verify that the regular TCP send method can get into a blocking state where it hogs the processor so long you miss microphone reads long enough for DMA overwrite.

# Update 2

I added a new version that uses regular TCP operations instead of AsyncTCP, but that makes a number of changes that I have found helpful for getting reliable remote recording. The details are in the README for the new version, called remote_task
