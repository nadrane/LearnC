import socket
import threading
import signal

MAX_RECEIVE_LENGTH = 2048

class StoppableThread(threading.Thread):
    """Thread class with a stop() method. The thread itself has to check
    regularly for the stopped() condition."""

    def __init__(self, *args, **kwargs):
        super(StoppableThread, self).__init__(*args, **kwargs)
        self._stop = threading.Event()

    def stop(self):
        self._stop.set()

    def stopped(self):
        return self._stop.is_set()

class SocketHoldingThread(StoppableThread):
    def __init__(self, socket, *args, **kwargs):
        super(SocketHoldingThread, self).__init__(*args, **kwargs)
        self.socket = socket

    def terminate(self):
        # Close the open socket
        try:
            self.socket.shutdown(socket.SHUT_RDWR)
            self.socket.close()
        # There are going to be cases where we accidentally try to close the socket twice
        # e.g., the process is killed, and the parent and child both try to close it
        except OSError:
            pass
        self.stop()

def run_server():
    thread_list= []  # A list of all open client conncetions. Stores type Connection
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    # We need to create a closure here so that clean_up (a signal handler) has access to the stored threads.
    # This is necessary because we cannot explicitly pass parametes to handlers.
    # The alternative is that thread_list is global
    def clean_up(signal, frame):
        # Iterate only over threads that are still running.
        # Threads can shut themselves down without notifying the parent again (when close to sent).
        # We don't need to trying closing the sockets associated with those threads a second time.
        for thread in filter(lambda x: x.stopped, thread_list):
            thread.terminate()
        server_socket.close()
        return
        
    # Set the signal handler for 
    signal.signal(signal.SIGINT, clean_up)

    # Reuse sockets that are in the TIME_WAIT state. This setting lets us rapidly shutdown the program and restart it.
    # http://serverfault.com/questions/329845/how-to-forcibly-close-a-socket-in-time-wait
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    # bind the socket to a public host, and a well-known port
    server_socket.bind(('127.0.0.1', 8000)) 
    # become a server socket
    server_socket.listen(5)

    while True:
        print ('Waiting for additional client')
        
        # The program will be exited by using a ctrl-c command.
        # When this happpens server_socket.accept fails ungracefully and raises an EINTR
        # Catch this and gracefully kill off the child threads and close the open sockets
        try:
            client_socket, address = server_socket.accept()
        # This happened from click ctrl-c. The signal handler will actually close 
        # all of the connections before this exception is handled, so no need to close them again
        except InterruptedError:
            return

        print ('New connection established')
        client_socket.send('Thank you for connecting to the server\n'.encode('ascii'))
        # Create a new thread to handle the domain name resolution process.
        new_thread = SocketHoldingThread(socket=client_socket, target=address_resolver)
        thread_list.append(new_thread)
        new_thread.start()

def address_resolver():
    current_thread = threading.current_thread()
    client_socket = current_thread.socket
    while not current_thread.stopped():
        current_thread.socket.sendall('Please enter a domain name\n'.encode('ascii'))

        # If this fails, just sever the connection with the client
        try:
            response = client_socket.recv(MAX_RECEIVE_LENGTH).decode('ascii')
        except OSError:
            current_thread.terminate()
            break

        # Telnet likes to append a pesky line feed. Let's get rid of it
        response = response.rstrip()

        if response == 'close':
            current_thread.terminate()
            break

        print ('Looking up hostname {}'.format(response))

        ip_address = socket.gethostbyname(response)

        # If this fails, just sever the connection with the client
        try:
            client_socket.sendall('The ip address is {ip_address}\n'.format(ip_address=ip_address).encode('ascii'))
        except OSError:
            current_thread.terminate()
            break
    return

if __name__ == "__main__":
    run_server()



