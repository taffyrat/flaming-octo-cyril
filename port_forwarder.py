# -----------------------------------------------------------------------------
#    SOURCE FILE:    port_forwarder.py
#    USAGE:            python3 port_forwarder.py
#    PURPOSE:        This is a server script that will forward incoming
#                    connections to a different host based on user defined 
#                    settings in a configuration file.
#     DATE:            March 31st, 2014
#     REVISIONS:        None.
#    DESIGNER:        Jacob Miner, Albert Liao
#     PROGRAMMER:        Jacob Miner, Albert Liao
#     NOTES:
# -----------------------------------------------------------------------------

######################
# Function Definitions
######################
import socket
import threading
import sys
import errno

# -----------------------------------------------------------------------------
# This function will create two threads: one thread to send data read in from
# the outside to the inside host and another thread to send data read in from
# the inside to the outside host.
#
# @params     externalSocket - A socket that corresponds to the external host.
#            internalSocket - A socket that corresponds to the internal host.
# @return    No return.
# -----------------------------------------------------------------------------
def createForwardingThreads(externalSocket, internalSocket):
    # Set both sockets to be non-blocking.
    externalSocket.setblocking(0)
    internalSocket.setblocking(0)

    # Create the external -> internal forwarding thread.
    externalToInternalThread = threading.Thread(target=forwardPackets,
                                                args=(externalSocket, internalSocket),
                                                )
    externalToInternalThread.start()

    # Create the internal -> external forwarding thread.
    internalToExternalThread = threading.Thread(target=forwardPackets,
                                                args=(internalSocket, externalSocket),
                                                )
    internalToExternalThread.start()

# -----------------------------------------------------------------------------
# This function will constantly try and read from the readSocket. Anything that
# is read in from the readsocket will be immediately sent out to the writeSocket.
#
# @params     readSocket - The socket to try and read from.
#            writeSocket - The socket to write out the read in data to.
# @return    No return.
# -----------------------------------------------------------------------------
def forwardPackets(readSocket, writeSocket):
    BUFSIZE = 1024

    # Enter in a loop to try and read from the readSocket. Anything that is read should be immediately send to the writeSocket.
    while True:
        # In non-blocking mode an exception is raised if there is nothing to read.
        try:
            # Read in a maximum BUFSIZE amount of data.
            data = readSocket.recv(BUFSIZE)

            # If we could not read at this time, an exception would occur and we would go to the except here.

            # If no data is brought back, this means the connection was closed on the other side.
            if not data:
                readSocket.close()
                writeSocket.close()
                break
            else:
                # If we read in data, send it to the write socket.
                # TODO: sendall may not send everything. we may need to make a thread to ensure that all the data we want to send is sent.
                #      while data:
                #        sent = sock.send(data)
                #        data = data[sent:]
                writeSocket.sendall(data)

            # If we get an exception, check to see if it is because there is no data to read.
        except socket.error as e:
            if e.args[0] == errno.EWOULDBLOCK:
                continue
            elif e.args[0] == errno.EBADF:
                break                
            else:
                print("Error occured on recv: {0}".format(e))
                break
#                time.sleep(1)           # short delay, no tight loops

# -----------------------------------------------------------------------------
# This function creates a listening socket on the specified port and sets up
# any new connection to forward between the internal host given.
#
# @params     internalHostIP - The IP address of the host to forward the port to.
#            sourcePort - The port to listen on at the Port Forwarer.
#            destinationPort - The port on the internal server that we are forwarding
#                              packets to.
# @return    No return.
# -----------------------------------------------------------------------------
def portForward(internalHostIP, sourcePort, destinationPort):
    # Create a listening socket on the specified port to listen for incoming connections.
    listenSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listenSocket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listenSocket.bind(('', sourcePort))
    listenSocket.listen(1)

    # Enter a loop to listen and accept new incoming connections.
    while True:
        # Block on listening for a new connection.
        externalSocket, externalHostAddress = listenSocket.accept()

        # Create a new connection to our internal host.
        internalSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        internalSocket.connect((internalHostIP, destinationPort))

        # Create new threads to handle the forwarding from ext->int and from int->ext.
        createForwardingThreads(externalSocket, internalSocket)

        # Continue loop to listen for new connections.

    listenSocket.close()

######################
# Program Start
######################

sshThread = threading.Thread(target=portForward,args=('192.168.0.19', 22, 22),).start()
httpdThread = threading.Thread(target=portForward,args=('192.168.0.19', 80, 80),).start()

