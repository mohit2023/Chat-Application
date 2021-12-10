#include <bits/stdc++.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

#define PORT 8080
#define BUFFER_SIZE 1024
#define CONTENT_SIZE 100000

struct thread_data {
  string user;
  int sock;
};

void error(string err,int ex = 1) {
  cout<<"\t ERR: \t";
  cout<<err<<"\n";
  if(ex) exit(-1);
}

void invalidInput(string hint = "") {
  cout<<"---->   Invalid input format. Try again!!!\n";
  if(hint != "") {
    cout<<"---->   "<<hint<<"\n";
  }
}

bool valid_username(string user) {
  if(user.size()==0) {
    return false;
  }

  for(auto ch:user) {
    if(!isalnum(ch)) {
      return false;
    }
  }
  if(user == "ALL") {
    return false;
  }

  return true;
}

bool check(string msg, string val, int idx){
  if(msg.size() < val.size()+idx) {
    return false;
  }
  if(val == msg.substr(idx,val.size())) {
    return true;
  }
  return false;
}

void exit_thread(int sock) {
  close(sock);
  pthread_exit(NULL);
}

void refresh_buffer(char buffer[], int &currentIndex,int idx) {
  memmove(buffer, buffer + idx + 1, currentIndex - idx - 1);
  currentIndex = currentIndex - idx - 1;
  buffer[currentIndex] = '\0';
}

int wrap_send(int connection_socket, string response) {
  const char* data = response.c_str();
  int tosend = strlen(data);
  int sent = 0;
  while(tosend>0) {
    int length = send(connection_socket, data + sent, tosend, 0);
    if(length <= 0){
      error("Sending failed", 0);
      return 1;
    }
    sent += length;
    tosend -= length;
  }
  return 0;
}

int read_buffer(char buffer[], int sz, int st){
  for(int i=st; i<sz-1; i++) {
    if(buffer[i]=='\n' && buffer[i+1]=='\n'){
      return i+1;
    }
  }
  return -1;
}

int wrap_read(int connection_socket, char buffer[], int &currentIndex) {
  int st = 0;
  while(true){
    int idx = read_buffer(buffer, currentIndex, st);
    if(idx != -1){
      return idx;
    }
    if(currentIndex == BUFFER_SIZE){
      break;
    }
    int read_res = recv(connection_socket, buffer + currentIndex, BUFFER_SIZE - currentIndex, 0);
    if(read_res <= 0){
      error("Reading failed",0);
      return -2;
    }
    st = currentIndex;
    currentIndex += read_res;
  }
  
  return -1;
}

int create_connection(struct sockaddr_in server) {
  int sock;

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if(sock == 0) {
    error("Socket creation failed");
  }

  int connect_res = connect(sock, (struct sockaddr *)&server, sizeof(server));
  if(connect_res < 0) {
    error("Connection failed");
  }

  return sock;
}

int recv_connection(string user, struct sockaddr_in server) {
  int sock = create_connection(server);
  
  string reg = "REGISTER TORECV " + user + "\n\n";
  if(wrap_send(sock, reg)) {
    close(sock);
    error("Server Disconnected");
  }
  char buffer[BUFFER_SIZE+1];
  int currentIndex = 0;
  int idx = wrap_read(sock, buffer, currentIndex);
  if(idx == -2) {
    close(sock);
    error("Server Disconnected...  RECV Registration failed");
  }
  if(idx == -1) {
    close(sock);
    error("Server behaviour undetermined...  RECV Registration failed");
  }
  buffer[idx] = '\0';
  string resp(buffer);
  if(resp == "REGISTERED TORECV " + user + "\n") {
    cout<<resp;
    return sock;
  } else if(resp == "ERROR 100 Malformed username\n") {
    cout<<"**  Server: "<<resp;
    close(sock);
    error("Invalid Username...  RECV Registration failed");
  } else {
    close(sock);
    error("Server behaviour undetermined...  RECV Registration failed");
  }

  return sock;
}

int send_connection(string user, struct sockaddr_in server) {
  int sock = create_connection(server);
  string reg = "REGISTER TOSEND " + user + "\n\n";
  if(wrap_send(sock, reg)) {
    close(sock);
    error("Server Disconnected");
  }

  char buffer[BUFFER_SIZE+1];
  int currentIndex = 0;
  int idx = wrap_read(sock, buffer, currentIndex);
  if(idx == -2) {
    close(sock);
    error("Server Disconnected...  SEND Registration failed");
  }
  if(idx == -1) {
    close(sock);
    error("Server behaviour undetermined...  SEND Registration failed");
  }
  buffer[idx] = '\0';
  string resp(buffer);

  if(resp == "REGISTERED TOSEND " + user + "\n") {
    cout<<resp;
    return sock;
  } else if(resp == "ERROR 100 Malformed username \n") {
    cout<<"**  Server: "<<resp;
    close(sock);
    error("Invalid Username...  SEND Registration failed");
  } else {
    close(sock);
    error("Server behaviour undetermined...  SEND Registration failed");
  }

  return sock;
}

void * thf_send(void *td) {
  struct thread_data data = *((struct thread_data *)td);
  string user = data.user;
  int sock = data.sock;

  while(true) {
    string str;
    getline(cin,str);
    if(str[0] != '@') {
      invalidInput();
      continue;
    }

    int user_idxe = str.find(' ');
    if(user_idxe == string::npos) {
      invalidInput();
      continue;
    }
    string to_send = str.substr(1,user_idxe-1);
    if(to_send==user || (!valid_username(to_send) && to_send!="ALL")) {
      invalidInput("Invalid username");
      continue;
    }

    string content = str.substr(user_idxe+1);
    int content_len = content.size();
    if(content_len == 0) {
      invalidInput("Content is missing");
      continue;
    }

    string msg_send = "SEND " + to_send +
      "\nContent-length: " + to_string(content_len) + "\n\n" + content;
    if(wrap_send(sock, msg_send)) break;

    char buffer[BUFFER_SIZE+1];
    int currentIndex = 0;
    int idx = wrap_read(sock, buffer, currentIndex);
    if(idx == -2) break;
    if(idx == -1) break;
    
    buffer[idx] = '\0';
    string resp(buffer);
    if(resp == "SEND " + to_send + "\n") {
      cout<<"**  Server: "<<resp+"("+str+")\n";
    } else if(resp == "ERROR 102 Unable to send\n") {
      cout<<"**  Server: "<<resp+"("+str+")\n";
    } else if(resp == "ERROR 103 Header incomplete\n") {
      cout<<"**  Server: "<<resp+"("+str+")\n";
    } else {
      break;
    }
  }

  close(sock);
  cout<<"Server Disconnected or Behaviour Undetermined...  SEND Connection closed\n";
  pthread_exit(NULL);
}

void * thf_recv(void *td) {
  struct thread_data data = *((struct thread_data *)td);
  string user = data.user;
  int sock = data.sock;
  
  char buffer[BUFFER_SIZE+1];
  int currentIndex = 0;
  while(true) {
    // read header
    int idx = wrap_read(sock, buffer, currentIndex);
    if(idx == -2) break;
    if(idx == -1) break;
    buffer[idx] = '\0';
    string msg(buffer);
    int split = msg.find('\n');
    if(split == string::npos) {
      string response = "ERROR 103 Header incomplete\n\n";
      if(wrap_send(sock, response)) break;
      refresh_buffer(buffer, currentIndex, idx);
      break;
    }

    string sendto = msg.substr(0,split);
    if(!check(sendto, "FORWARD", 0)) {
      string response = "ERROR 103 Header incomplete\n\n";
      if(wrap_send(sock, response)) break;
      refresh_buffer(buffer, currentIndex, idx);
      break;
    }

    sendto = sendto.substr(8);
    if(!valid_username(sendto)) {
      string response = "ERROR 103 Header incomplete\n\n";
      if(wrap_send(sock, response)) break;
      refresh_buffer(buffer, currentIndex, idx);
      break;
    }

    string con_len = msg.substr(split+1);
    if(!check(con_len, "Content-length:", 0)) {
      string response = "ERROR 103 Header incomplete\n\n";
      if(wrap_send(sock, response)) break;
      refresh_buffer(buffer, currentIndex, idx);
      break;
    }

    con_len = con_len.substr(16,con_len.size()-17);
    int len = -1;
    try {
      len = stoi(con_len);
      if(len <= 0 || len > CONTENT_SIZE) throw -1;
    }
    catch(...) {
      len = 0;
    }
    if(!len){
      string response = "ERROR 103 Header incomplete\n\n";
      if(wrap_send(sock, response)) break;
      refresh_buffer(buffer, currentIndex, idx);
      break;
    }

    refresh_buffer(buffer, currentIndex, idx);
    // read len characters from buffer+socket.
    char chat[len+1];
    int chatIndex = 0;
    while(chatIndex<currentIndex && chatIndex<len) {
      chat[chatIndex] = buffer[chatIndex];
      chatIndex++;
    }
    refresh_buffer(buffer, currentIndex, chatIndex-1);
    // Uncomment below code for assumption that message is not defragmented and
    // to get errors when there is mismatch in actual messsage length and one mentioned in header.
    /*if(chatIndex < len) {
      // ERROR 103 because message length is less than what is mentioned in header.
      string response = "ERROR 103 Header incomplete\n\n";
      if(wrap_send(sock, response)) exit_thread(sock);
      break;
    }
    if(currentIndex != 0) {
      // ERROR 103 because message length is more than what is mentioned in header.
      string response = "ERROR 103 Header incomplete\n\n";
      if(wrap_send(sock, response)) exit_thread(sock);
      break;
    }*/
    
    
    while(chatIndex != len) {
      int length = recv(sock, chat + chatIndex, len-chatIndex, 0);
      if(length <= 0){
        error("Reading failed", 0);
        break;
      }
      chatIndex += length;
    }
    chat[chatIndex] = '\0';
    string sendmsg(chat);
    cout<<"\t\t\t @" + sendto + " " + sendmsg + "\n";

    string resp = "RECEIVED " + sendto + "\n\n";
    if(wrap_send(sock, resp)) break;

  }

  close(sock);
  cout<<"Server Disconnected or Behaviour Undetermined...  RECV Connection closed\n";
  pthread_exit(NULL);
}

int main(int argc, char **argv) {

  string user = "default";
  string IP = "127.0.0.1";
  if(argc != 3) {
    error("Wrong command line arguements");
  }

  user = argv[1];
  IP = argv[2];
  const char *ip = IP.c_str();

  if(!valid_username(user)) {
    error("Invalid username!!!");
  }


  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_port = htons(PORT);

  int inet_pton_res = inet_pton(AF_INET, ip, &server.sin_addr);
  if(inet_pton_res <= 0) {
    error("Invalid address");
  }

  int recv_socket = recv_connection(user, server);
  int send_socket = send_connection(user, server);


  pthread_t th_send,th_recv;
  struct thread_data td_send,td_recv;
  td_send.user = user;
  td_recv.user = user;
  td_send.sock = send_socket;
  td_recv.sock = recv_socket;
  pthread_create(&th_send, NULL, thf_send, (void*)&td_send);
  pthread_detach(th_send);
  pthread_create(&th_recv, NULL, thf_recv, (void*)&td_recv);
  pthread_detach(th_recv);

  pthread_exit(NULL);

  return 0;
}