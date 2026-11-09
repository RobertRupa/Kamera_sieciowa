import socket

import esp_rfc2217_server


class LoopbackSocket(socket.socket):
    def bind(self, address):
        if self.family == socket.AF_INET and address[0] == "":
            address = ("127.0.0.1", address[1])
        return super().bind(address)


def main():
    esp_rfc2217_server.socket.socket = LoopbackSocket
    esp_rfc2217_server.main()


if __name__ == "__main__":
    main()