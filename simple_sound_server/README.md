Tested with python 3.8.10 on linux (kbuntu)

Edit mic_server.py to contain valid port number matching the one you put in the
client for the "tcp_port" variable

$ python3 mic_server.py

You can optionally set the bytes per sample value for the incomming
data using a command line argument:

$ python3 mic_server.py 2

for 2 bytes (16 bits) per sample or

$ python3 mic_server.py 4

for 4 bytes (32 bits) per sample

The default value is 4

# When running remote_task version as checked in, use 2 bytes per sample

