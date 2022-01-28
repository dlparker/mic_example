The purpose of this program is to measure the process of reading data from the
microphone, so that the same measurements can be compared when sending that data
over a TCP socket. In my testing, this program has never lost any mic data.

In contrast, when sending the mic data over a TCP socket, sometimes reading the
data from the mic delivers fewer samples than it should. This is demonstrated by
calculating and displaying the number of seconds of sound data actually collected.
When running this program, the calculated length of the sound data is very close to
the timed duration of the recording process. When running the TCP program, the
values are usually similar, but occasionally the calculated length is shorter
than the recorded duration.

You can look in ../remote to see how to run the TCP program.