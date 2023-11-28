#!/usr/bin/env python3


import socket
import socketserver
import time

class MyTCPHandler(socketserver.BaseRequestHandler):
    """
    The request handler class for our server.

    It is instantiated once per connection to the server, and must
    override the handle() method to implement communication to the
    client.
    """

    total_connections = 0

    def setup(self):
        self.cid = MyTCPHandler.total_connections
        MyTCPHandler.total_connections += 1
        print(f'Opened connection ID {self.cid}')

    def handle(self):
        # self.request is the TCP socket connected to the client
        # self.data = self.request.recv(1024).strip()
        # print("{} wrote: {}".format(self.client_address[0], self.data))
        # just send back the same data, but upper-cased
        # self.request.sendall(self.data.upper())
        while not self.server.stop_now:
            time.sleep(1)

class Server(socketserver.ThreadingTCPServer):

    def __init__(self, host_port, handler):
        self.stop_now = False
        self.request_queue_size = 100
        self.allow_reuse_address = True
        super().__init__(host_port, handler)

    def get_request(self):
        ret = self.socket.accept()
        sock, _ = ret
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
        return ret

if __name__ == "__main__":
    HOST, PORT = "localhost", 9999
    socketserver.BaseServer.allow_reuse_address = True

    # Create the server, binding to localhost on port 9999
    with Server((HOST, PORT), MyTCPHandler) as server:
        # Activate the server; this will keep running until you
        # interrupt the program with Ctrl-C
        try:
            server.serve_forever()
        except:
            server.stop_now = True
            raise
