# dhm
Distributed Homomorphic Math

* TCP-based matrix exchange protocol
* Homomorphic encryption protocol
* client-server example

### Requirements
* C++ >= 17
* Boost
* HElib

### Build examples
```bash
mkidr build && cd build
cmake ..
make
```
### Run examples
```
# in terminal tab 1
./worker 8888
# in terminal tab 2
./worker 9999
# in terminal tab 3
./client --help
./client -w localhost:8888 -w localhost:9999 --op echo
./client -w localhost:8888 -w localhost:9999 --op add
./client -w localhost:8888 -w localhost:9999 --op mul
./client -w localhost:8888 -w localhost:9999 --op hadd --size 64
./client -w localhost:8888 -w localhost:9999 --op hmul --size 64
```
