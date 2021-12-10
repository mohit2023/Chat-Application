# Chat-Application

## To Run Server:
### Compile:
``` shell
g++ -pthread -o server server.cpp
```
### Run:
``` shell
./sever
```

## To Run Client:
### Complie:
``` shell
g++ -pthread -o client client.cpp
```
### Run:
``` shell
./client username ipaddress
```

Here username is the username for the client to register with server and ipaddress is the IP address (not domain name) on which the server is running (PORT number is hardcoded as 8080). My server code is hardcoded to run on IP “127.0.0.1” and PORT “8080”.
