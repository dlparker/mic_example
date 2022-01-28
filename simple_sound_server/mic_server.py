import datetime
import os
import sys
import time
import socket
import select
import wave

TCP_IP = "0.0.0.0"
TCP_PORT = 3334

tcp_sock = None

def record(client):
    fname = '_'.join((str(datetime.datetime.now()).split())) +".wav"
    print(f'{fname}')
    samples_this_second = 0
    sample_seconds = 0
    with wave.open(fname, 'wb') as wf:
        client.sendall(b'go') # tell client to start
        data = client.recv(4) # client sends sample rate as four bytes msb first
        sample_rate = int.from_bytes(data, byteorder='big')
        wf.setnchannels(1)
        wf.setsampwidth(4)
        print(f'{fname} samplerate = {sample_rate}', flush=True)
        wf.setframerate(sample_rate)
        start_time = time.time()
        rec_count = 0
        bytes_count = 0
        # sockets from which we expect to read
        inputs = [client]
        outputs = []
        while True:
            # wait for at least one of the sockets to be ready for processing
            readable, writable, exceptional = select.select(inputs, outputs, inputs, 0.05)
            if client in exceptional:
                print('exception on client socket')
                client.close()
                break
            if client not in readable:
                continue
            data = client.recv(1024)
            if len(data) == 0:
                print('socket closed, closing file')
                client.close()
                break
            wf.writeframes(data)
            rec_count += 1
            bytes_count += len(data)
            elapsed = time.time() - start_time
            samples_this_second  += len(data)/4 # 32 bits per sample
            if samples_this_second >= sample_rate:
                print('a seconds worth of samples received\n', flush=True)
                sample_seconds += 1
                samples_this_second -= sample_rate
    elapsed = time.time() - start_time
    print(f"recording done: {rec_count} records, {bytes_count} bytes, loop time {elapsed}")
    calc_value = float(bytes_count/4) / float(sample_rate)
    print(f"recoding seconds calculated from samples {calc_value}")
          

if __name__=="__main__":
    tcp_sock = socket.socket()
    tcp_sock.bind((TCP_IP, TCP_PORT))
    tcp_sock.listen(0)                 
    print(f'tcp socket setup done, sock is {tcp_sock}')
    while True:
        client, addr = tcp_sock.accept()
        print('got client', flush=True)
        record(client)
            

