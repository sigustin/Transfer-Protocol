# Transfer-Protocol
Implementation of a transfer protocol without data loss for the course LINGI1341 &ndash; Réseaux informatiques, in 2016.
Specifically, the protocol is intended to be used to transfer data between 2 computers.

It is built using **UDP** segments and uses **IPv6**.
The data loss prevention is achieved thanks to **CRC checksums**, a **go-back-n** strategy and a **selective repeat** (on the receiver side).
A **graceful disconnect** was also implemented to prevent losing the last segments.

*Languages used:*
- *C*
- *Makefile (to automate compilations and test executions)*
- *bash (for the tests)*

## Collaborators
Even though this is a two people project, I made the code and report.
My collaborator helped me at the time with the feature design, architecture and projects for other courses.

## What I learned
- Interact with the network using *C*
- Find relevant information in *C*'s manpages
- Implement and optimize communication protocols
- Automate command execution using makefiles
- Follow a (quite vague) protocol description close enough to make a protocol able of inter-operate with other protocols that follow the same guidelines

## Files worth checking
- The instructions (in French): [consignes.pdf](https://github.com/sigustin/Transfer-Protocol/blob/master/consignes.pdf)
- The report explaining what we made (in French): [rapport.pdf](https://github.com/sigustin/Transfer-Protocol/blob/master/rapport.pdf)
- The tests: [tests/tests.sh](https://github.com/sigustin/Transfer-Protocol/blob/master/tests/tests.sh)
- Main functions of the receiver: [src/receiver.c](https://github.com/sigustin/Transfer-Protocol/blob/master/src/receiver.c) and [src/receiverReadWriteLoop.c](https://github.com/sigustin/Transfer-Protocol/blob/master/src/receiverReadWriteLoop.c)
- Main functions of the sender: [src/sender.c](https://github.com/sigustin/Transfer-Protocol/blob/master/src/sender.c) and [src/senderReadWriteLoop.c](https://github.com/sigustin/Transfer-Protocol/blob/master/src/senderReadWriteLoop.c)
- Packet definition: [src/packets.c](https://github.com/sigustin/Transfer-Protocol/blob/master/src/packets.c)
- Network connections: [src/createConnection.c](https://github.com/sigustin/Transfer-Protocol/blob/master/src/createConnection.c)

## Compilation and execution
Compile the receiver on the first computer:
```sh
make receiver
```

Compile the sender on the second computer:
```sh
make sender
```

Run the receiver:
```sh
./receiver [<option>] <sender-IP> <port>
```
with `-f <file-path>` the path to the file where messages have to be taken from.

Run the sender:
```sh
./sender [<option>] <receiver-IP> <port>
```
with `-f <file-path>` the path to the file where messages have to be written to.

Run the tests:
```sh
make tests
```
