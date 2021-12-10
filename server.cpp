#include <bits/stdc++.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

using namespace std;

#define PORT 8080
#define BACKLOG 10
#define BUFFER_SIZE 1024
#define CONTENT_SIZE 100000

pthread_mutex_t mutex_lock;

unordered_map<string,int> mp[3];

struct broadcast_data {
  int status;
  int connection_socket;
  int recv_socket;
  string response;
  string recp;
  string sender;
};

void error(string err, int ex = 1) {
  cout<<err<<"\n";
  if(ex) exit(-1);
}

void server_log(string res, string user) {
  if(user == ""){
    user = "UNREGISTERED_CONNECTION";
  }
  cout<<"Server TO " + user + " :\n";
  cout<<res;
  cout<<"\n------------------------------\n";
}

void client_log(string res, string user) {
  if(user == ""){
    user = "UNREGISTERED_CONNECTION";
  }
  cout<<"From client: " + user + " :\n";
  cout<<res;
  cout<<"\n------------------------------\n";
}

bool valid_username(string user) {
  if(user.size() == 0){
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

void refresh_buffer(char buffer[], int &currentIndex,int idx) {
  memmove(buffer, buffer + idx + 1, currentIndex - idx - 1);
  currentIndex = currentIndex - idx - 1;
  buffer[currentIndex] = '\0';
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

void exit_thread(int sock, int ex = 1, string username = "") {
  close(sock);
  if(ex) {
    if(username != "") {
      pthread_mutex_lock(&mutex_lock);
      mp[2][username] = 0;
      close(mp[0][username]);
      mp[0].erase(username);
      mp[1].erase(username);
      pthread_mutex_unlock(&mutex_lock);
    }
    pthread_exit(NULL);
  } else if(username != "") {
    mp[2][username] = 0;
    close(mp[0][username]);
    mp[0].erase(username);
    mp[1].erase(username);
  }
}

int wrap_send(int connection_socket, string response, string username) {
  server_log(response, username);
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
      return -2;
    }
    st = currentIndex;
    currentIndex += read_res;
  }
  
  return -1;
}

void inform_sender(int connection_socket, string sender, string recp, int status) {
  if(status == 0) {
    string resp = "SEND " + recp + "\n\n";
    if(wrap_send(connection_socket, resp, sender)) {
      exit_thread(connection_socket, 1, sender);
    }
  } else if(status == 102) {
    string resp = "ERROR 102 Unable to send\n\n";
    if(wrap_send(connection_socket, resp, sender)) {
      exit_thread(connection_socket, 1, sender);
    }
  }
  else if(status == 103) {
    string resp = "ERROR 103 Header incomplete\n\n";
    if(wrap_send(connection_socket, resp, sender)) {
      exit_thread(connection_socket, 1, sender);
    }
    exit_thread(connection_socket, 1, sender);
  }
}

int forward(int connection_socket, string recp, string response, string sender, int recv_socket) {
  if(wrap_send(recv_socket, response, recp)) {
    exit_thread(recv_socket, 0, recp);
    return 102;
  }
  char recv_buffer[BUFFER_SIZE+1];
  int recvIndex = 0;
  int idx = wrap_read(recv_socket, recv_buffer, recvIndex);
  if(idx == -2) {
    exit_thread(recv_socket, 0, recp);
    return 103;
  }
  if(idx == -1) {
    // invalid response from receptor client
    client_log("Some unwanted data", recp);
    return 103;
  }

  recv_buffer[idx] = '\0';
  string msg(recv_buffer);
  client_log(msg, recp);
  if(msg == "RECEIVED " + sender + "\n") {
    return 0;
  } else if(msg == "ERROR 103 Header incomplete\n") {
    return 103;
  } else {
    return 102;
  }
}

void * broadcast(void *th_arg) {
  struct broadcast_data *data = (struct broadcast_data*)th_arg;
  data->status = forward(data->connection_socket, data->recp, data->response, data->sender, data->recv_socket);
  pthread_exit(NULL);
}

void * handle_connection(void *th_arg) {
  int connection_socket = *((int *)th_arg);
  bool registered = false;
  int currentIndex = 0;
  char buffer[BUFFER_SIZE+1];
  int type = -1;
  string username;

  // registration
  while(true){
    int idx = wrap_read(connection_socket, buffer, currentIndex);

    if(idx == -2){
      exit_thread(connection_socket);
    }
    if(idx == -1){
      client_log("Some unwanted data",username);
      string response = "ERROR 101 No user registered\n\n";
      if(wrap_send(connection_socket, response, username)) exit_thread(connection_socket);
      buffer[0] = '\0';
      currentIndex = 0;
      continue;
    }

    buffer[idx] = '\0';
    string msg(buffer);
    client_log(msg, username);

    if(!check(msg, "REGISTER", 0)) {
      string response = "ERROR 101 No user registered\n\n";
      if(wrap_send(connection_socket, response, username)) exit_thread(connection_socket);
      refresh_buffer(buffer, currentIndex, idx);
      continue;
    }
    if(!check(msg, "TOSEND", 9) && !check(msg, "TORECV", 9)) {
      string response = "ERROR 101 No user registered\n\n";
      if(wrap_send(connection_socket, response, username)) exit_thread(connection_socket);
      refresh_buffer(buffer, currentIndex, idx);
      continue;
    }

    username = msg.substr(16, idx-17);
    if(!valid_username(username)) {
      string response = "ERROR 100 Malformed username\n\n";
      if(wrap_send(connection_socket, response, username)) exit_thread(connection_socket);
      refresh_buffer(buffer, currentIndex, idx);
      continue;
    }

    type = msg.substr(9,6) == "TOSEND" ? 1 : 0;
    pthread_mutex_lock(&mutex_lock);
    bool already_registered = mp[type].find(username)!=mp[type].end();
    pthread_mutex_unlock(&mutex_lock);
    if(already_registered) {
      string response = "ERROR 100 Malformed username\n\n";
      if(wrap_send(connection_socket, response, username)) exit_thread(connection_socket);
      refresh_buffer(buffer, currentIndex, idx);
      continue;
    }

    string tmp = type==0 ?"TORECV ":"TOSEND ";
    string response = "REGISTERED " + tmp + username + "\n\n"; 
    if(wrap_send(connection_socket, response, username)) exit_thread(connection_socket);
    pthread_mutex_lock(&mutex_lock);
    mp[type][username] = connection_socket;
    mp[2][username]++;
    pthread_mutex_unlock(&mutex_lock);

    refresh_buffer(buffer, currentIndex, idx);
    break;
  }

  // close if receiving thread
  if(!type) {
    pthread_exit(NULL);
  }

  // chat
  while(true) {

    // read header
    int idx = wrap_read(connection_socket, buffer, currentIndex);

    if(idx == -2) {
      exit_thread(connection_socket, 1, username);
    }
    
    if(!registered) {
      pthread_mutex_lock(&mutex_lock);
      bool check = mp[2][username] == 2;
      pthread_mutex_unlock(&mutex_lock);
      if(check){
        registered = true;
      }
      else{
        string response = "ERROR 101 No user registered\n\n";
        if(wrap_send(connection_socket, response, username)) exit_thread(connection_socket, 1, username);
        continue;
      }
    }
    
    if(idx == -1){
      client_log("Some unwanted data", username);
      string response = "ERROR 103 Header incomplete\n\n";
      if(wrap_send(connection_socket, response, username)) exit_thread(connection_socket, 1, username);
      buffer[0] = '\0';
      break;
    }

    buffer[idx] = '\0';
    string msg(buffer);
    client_log(msg, username);
    int split = msg.find('\n');
    if(split == string::npos) {
      string response = "ERROR 103 Header incomplete\n\n";
      if(wrap_send(connection_socket, response, username)) exit_thread(connection_socket, 1, username);
      refresh_buffer(buffer, currentIndex, idx);
      break;
    }

    string sendto = msg.substr(0,split);
    if(!check(sendto, "SEND", 0)) {
      string response = "ERROR 103 Header incomplete\n\n";
      if(wrap_send(connection_socket, response, username)) exit_thread(connection_socket, 1, username);
      refresh_buffer(buffer, currentIndex, idx);
      break;
    }

    sendto = sendto.substr(5);
    if(!valid_username(sendto) && sendto != "ALL") {
      string response = "ERROR 103 Header incomplete\n\n";
      if(wrap_send(connection_socket, response, username)) exit_thread(connection_socket, 1, username);
      refresh_buffer(buffer, currentIndex, idx);
      break;
    }

    string con_len = msg.substr(split+1);
    if(!check(con_len, "Content-length:", 0)) {
      string response = "ERROR 103 Header incomplete\n\n";
      if(wrap_send(connection_socket, response, username)) exit_thread(connection_socket, 1, username);
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
      if(wrap_send(connection_socket, response, username)) exit_thread(connection_socket, 1, username);
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
    /*
    if(chatIndex < len) {
      // ERROR 103 because message length is less than what is mentioned in header.
      string response = "ERROR 103 Header incomplete\n\n";
      if(wrap_send(connection_socket, response, username)) exit_thread(connection_socket, 1, username);
      break;
    }
    if(currentIndex != 0) {
      // ERROR 103 because message length is more than what is mentioned in header.
      string response = "ERROR 103 Header incomplete\n\n";
      if(wrap_send(connection_socket, response, username)) exit_thread(connection_socket, 1, username);
      break;
    }
    */

    while(chatIndex != len) {
      int length = recv(connection_socket, chat + chatIndex, len-chatIndex, 0);
      if(length <= 0){
        error("Reading failed", 0);
        exit_thread(connection_socket, 1, username);
      }
      chatIndex += length;
    }
    chat[chatIndex] = '\0';
    string sendmsg(chat);
    client_log(sendmsg, username);

    pthread_mutex_lock(&mutex_lock);
    bool avail_user = mp[2][sendto] == 2;
    pthread_mutex_unlock(&mutex_lock);
    if(!avail_user || sendto==username) {
      string response = "ERROR 102 Unable to send\n\n";
      if(wrap_send(connection_socket, response, username)) exit_thread(connection_socket, 1, username);
      continue;
    }

    // forward message
    string response = "FORWARD " + username + "\nContent-length: " + to_string(sendmsg.size()) + "\n\n" + sendmsg;

    vector<pair<string,int>> list;
    pthread_mutex_lock(&mutex_lock);
    if(sendto == "ALL") {
      for(auto x:mp[2]) {
        if(x.first!="ALL" && x.first!=username && x.second == 2){
          list.push_back({x.first,mp[0][x.first]});
        }
      }
      int num = list.size();
      if(num==0) {
        inform_sender(connection_socket, username, sendto, 0);
      }
      int flag = 0;
      pthread_t tid[num];
      struct broadcast_data th_data[num];
      for(int i=0;i<num;i++) {
        th_data[i].connection_socket = connection_socket;
        th_data[i].response = response;
        th_data[i].recp = list[i].first;
        th_data[i].recv_socket = list[i].second;
        th_data[i].sender = username;
        pthread_create(&tid[i], NULL, broadcast, (void*)&th_data[i]);
      }
      for(int i=0;i<num;i++) {
        pthread_join(tid[i], NULL);
        flag=max(flag,th_data[i].status);
      }
      inform_sender(connection_socket, username, sendto, flag);
    }
    else {
      int status = forward(connection_socket, sendto, response, username, mp[0][sendto]);
      inform_sender(connection_socket, username, sendto, status);
    }
    pthread_mutex_unlock(&mutex_lock);

  }

  exit_thread(connection_socket, 1, username);
  pthread_exit(NULL);
}

int main() {
  if (pthread_mutex_init(&mutex_lock, NULL) != 0) {
    error("Mutex not created");
  }

  mp[2]["ALL"] = 2;
  int server_fd;
  struct sockaddr_in server;

  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if(server_fd == 0){
    error("Socket creation failed");
  }

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(PORT);

  int reuse=1;
  int set_res = setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &reuse, sizeof(int));
  if(set_res < 0){
    error("reuse failed");
  }

  int bind_res = bind(server_fd, (struct sockaddr *)&server, sizeof(server));
  if(bind_res < 0){
    error("Bind failed");
  }

  int listen_res = listen(server_fd, BACKLOG);
  if(listen_res < 0){
    error("Listen failed");
  }

  cout<<"Server started\n\n---------------------\n";

  while(true) {
    int server_size = sizeof(sockaddr_in);
    int connection_socket = accept(server_fd, (struct sockaddr *)&server, (socklen_t *)&server_size);
    if(connection_socket < 0){
      error("Connection Failed");
    }

    pthread_t th;
    int *th_arg = &connection_socket; 
    pthread_create(&th, NULL, handle_connection, th_arg);
    pthread_detach(th);
  }

  pthread_exit(NULL);
  pthread_mutex_destroy(&mutex_lock);
  return 0;
}