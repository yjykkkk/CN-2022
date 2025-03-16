#include<stdio.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<sys/ioctl.h>
#include<net/if.h>
#include<unistd.h> 
#include<string.h>
#include<stdlib.h>
#include <iostream> 
#include <string>
#include <fstream>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unordered_set>
#include <string.h>
#include <dirent.h>
#include <csignal>
#include "opencv2/opencv.hpp"

using namespace std; 
using namespace cv;
#define BUFF_SIZE 1024
#define ERR_EXIT(a){ perror(a); exit(1); }
#define MAX_FD 1024   // FD_SETSIZE
#define CMD_BUFF_SIZE 4096  //terminal buffer size

typedef struct {
    int port;  // port to listen
    int listen_fd;  // fd to wait for a new connection
} server;
server svr;

typedef struct {
    FILE *fp;
    char name[BUFF_SIZE];
    int socketfd;
    int status;
    Mat imgServer;
} Clients;

void initOneClient(Clients *clients)
{
    bzero(clients->name, sizeof(char) * BUFF_SIZE);
    clients->fp = NULL;
    clients->status = 0;
    return;
}

void initClients(Clients *clients)
{
    for(int i = 0; i < MAX_FD; ++i)
        initOneClient(&clients[i]);
    return;
}


static void init_server(int port) {
    struct sockaddr_in servaddr;
    svr.port = port;
    if((svr.listen_fd = socket(AF_INET , SOCK_STREAM , 0)) < 0){
        ERR_EXIT("socket failed\n")
    }
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    if (bind(svr.listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        ERR_EXIT("bind failed\n");
    }
    if (listen(svr.listen_fd, 3) < 0) {
        ERR_EXIT("listen failed\n");
    }
    return;
}

unordered_set<string> blocklist;
VideoCapture clients_cap[MAX_FD];

int main(int argc, char *argv[]){
    signal(SIGPIPE, SIG_IGN);   //ignore SIGPIPE
    int  client_sockfd, write_byte;
    struct sockaddr_in server_addr, client_addr;
    int client_addr_len = sizeof(client_addr);
    char message[BUFF_SIZE];

    string port_(argv[1]);
    int port = std::atoi(port_.c_str());
    // make folder "server" and enter
    struct stat folder_stat;
    if(stat("./server_dir/", &folder_stat) != 0 || !(folder_stat.st_mode & S_IFDIR))
        mkdir("./server_dir/", 0777);
    chdir("./server_dir/");

    init_server(port); 
    
    //----------preparing structure for all clients---------
    Clients *clients = (Clients *)malloc(sizeof(Clients) * MAX_FD);
    initClients(clients);

    //------------preparing select() table---------------
    fd_set read_set, read_set_copy; //因為select會改掉set，所以用之前要先複製一份，不要直接把原本的拿去用
    FD_ZERO(&read_set);
    //FD_ZERO(&write_set);

    while (1){
        FD_SET(svr.listen_fd, &read_set);
        memcpy(&read_set_copy, &read_set, sizeof(read_set));
        if (select(MAX_FD, &read_set_copy, NULL, NULL, NULL) == -1){
            continue;
        }
        // check new connection
        if(FD_ISSET(svr.listen_fd, &read_set_copy)) { 
            if((client_sockfd = accept(svr.listen_fd, (struct sockaddr *)&client_addr, (socklen_t*)&client_addr_len)) < 0){
                ERR_EXIT("accept failed\n");
            }
            FD_SET(client_sockfd, &read_set);
            char name[BUFF_SIZE] = "";
            int read_stat = recv(client_sockfd, &name, sizeof(name), 0);
            if (read_stat == 0){
                continue;
            }
            send(client_sockfd, "ok", strlen("ok"), 0);
            bzero(clients[client_sockfd].name, sizeof(char) * BUFF_SIZE);
            strcpy(clients[client_sockfd].name, name);
            clients[client_sockfd].socketfd = client_sockfd;
            clients[client_sockfd].status = 0;
            clients[client_sockfd].fp = NULL;
            printf("Accept a new connection on socket [%d]. Login as %s.\n", client_sockfd, name);
            char path[64] = "./";
            strcat(path, name);
            struct stat folder_stat;
            if(stat(path, &folder_stat) != 0 || !(folder_stat.st_mode & S_IFDIR))
                mkdir(path, 0777);
            continue;   
        }
        else{ //沒有新連線

            // check whether we can read data from other socket
            for (int c_fd=4; c_fd<MAX_FD; c_fd++){    //svr.listen_fd = 3
                if (FD_ISSET(c_fd, &read_set_copy)){
                    char input_cmd[CMD_BUFF_SIZE] = {};
                    int recv_stat;
                    recv_stat = recv(c_fd, &input_cmd, sizeof(input_cmd), 0);
                    if (recv_stat == 0){   //client 關閉連線了
                        close(c_fd);
                        FD_CLR(c_fd, &read_set);
                        printf("Close client socket [%d].\n", c_fd);
                        continue;
                    }
                    string cmd_string = string(input_cmd);
                    int sendstatus;
                    char buffer[BUFF_SIZE] = {};
                    bzero(buffer, BUFF_SIZE);
                    if (strncmp(input_cmd, "blocklist", 9)==0){ 
                        if (strcmp(clients[c_fd].name, "admin") != 0){
                            send(c_fd, "Permission denied.\n", strlen("Permission denied.\n"), 0);
                            continue;
                        }
                        else    send(c_fd, "ok", strlen("ok"), 0);
                        bzero(buffer, BUFF_SIZE);
                        int read_bl;
                        read_bl = recv(c_fd, &buffer, sizeof(buffer), 0);//wait for client to receive permission_msg
                        if (read_bl == 0){
                            continue;
                        }
                        string blocklist_msg;
                        if (blocklist.empty()){  //If the blocklist is empty, you don’t need to print anything.
                            send(c_fd, "empty blocklist!!!\n", strlen("empty blocklist!!!\n"), 0);
                            bzero(buffer, BUFF_SIZE);
                            read_bl = recv(c_fd, &buffer, sizeof(buffer), 0); //client send "ok"
                            if (read_bl == 0)
                                continue;
                        }
                        else{
                            int jump_bl = 0;
                            for (unordered_set<string>::iterator it = blocklist.begin(); it != blocklist.end(); it++){
                                blocklist_msg = "";
                                blocklist_msg += *it;
                                blocklist_msg +='\n';
                                send(c_fd, blocklist_msg.c_str(), strlen(blocklist_msg.c_str()), 0);
                                bzero(buffer, BUFF_SIZE);
                                read_bl = recv(c_fd, &buffer, sizeof(buffer), 0); //client send "ok"
                                if (read_bl == 0){
                                    jump_bl = 1;
                                    break;
                                }
                            }
                            if (jump_bl == 1){
                                continue;
                            }
                        }
                        send(c_fd, "END!!!!!BL-1-1", strlen("END!!!!!BL-1-1"), 0);
                    }
                    else if (strncmp(input_cmd, "ls", 2)==0){
                        if (blocklist.count(string(clients[c_fd].name)) > 0){
                            sendstatus = send(c_fd, "Permission denied.\n", strlen("Permission denied.\n"), 0);
                            continue;
                        }
                        else{
                            sendstatus = send(c_fd, "not banned.", strlen("not banned."), 0); //user is not banned
                        }
                        char Message[BUFF_SIZE] = {};
                        bzero(Message, BUFF_SIZE);
                        int read_ls;
                        read_ls = recv(c_fd, &Message, sizeof(Message), 0);//wait for client to receive permission_msg
                        if (read_ls == 0)
                            continue;
                        string path = "./"; 
                        path.append(clients[c_fd].name);
                        path += "/";
                        chdir(path.c_str());
                        string ls_msg = "";
                        DIR *d;
                        struct dirent *dir;
                        d = opendir(".");
                        int file_num = 0;
                        if (d) {
                            while ((dir = readdir(d)) != NULL) { //bypass "." & ".."
                                if (strcmp(dir->d_name, ".")==0 || strcmp(dir->d_name, "..")==0){
                                    continue;
                                }
                                sprintf(message, "%s\n", dir->d_name);
                                send(c_fd, message, strlen(message), 0);
                                bzero(Message, BUFF_SIZE);
                                recv(c_fd, &Message, sizeof(Message), 0); //read "ok"
                            }
                            closedir(d);
                        }
                        int sent;
                        if (file_num == 0)
                            send(c_fd, "\n", strlen("\n"), 0);
                        chdir("../");
                    }
                    else{              
                        string command = cmd_string.substr(0, cmd_string.find(" "));
                        cmd_string = cmd_string.substr(cmd_string.find(" ")+1, cmd_string.length());
                        if (command == "ban"){  
                            if (strcmp(clients[c_fd].name, "admin")!=0){
                                send(c_fd, "Permission denied.\n", strlen("Permission denied.\n"), 0);
                                continue;
                            }
                            else    send(c_fd, "ok", strlen("ok"), 0);
                            bzero(buffer, BUFF_SIZE);
                            int read_ban;
                            read_ban = recv(c_fd, &buffer, sizeof(buffer), 0);//wait for client to receive permission_msg
                            if (read_ban == 0)
                                continue;
                            string argument;
                            string ban_msg;
                            int jump_ban = 0;
                            while(1){
                                if (cmd_string.find(" ")==-1){ //取得最後一個字 (找不到空白)
                                    if (cmd_string.length() == 0){
                                        break;
                                    }
                                    argument = cmd_string;
                                    if (argument == "admin"){
                                        send(c_fd, "You cannot ban yourself!\n", strlen("You cannot ban yourself!\n"), 0);
                                        bzero(buffer, BUFF_SIZE);
                                        read_ban = recv(c_fd, &buffer, sizeof(buffer), 0);//wait for client 
                                        if (read_ban == 0){
                                            jump_ban = 1;
                                        }
                                        break;
                                    }
                                    else{
                                        if (blocklist.count(argument)!=0){
                                            sprintf(message, "User %s is already on the blocklist!\n", argument.c_str());
                                            send(c_fd,message, strlen(message), 0);
                                            bzero(buffer, BUFF_SIZE);
                                            read_ban = recv(c_fd, &buffer, sizeof(buffer), 0);//wait for client 
                                            if (read_ban == 0){
                                                jump_ban = 1;
                                            }
                                        }
                                        else{
                                            blocklist.insert(argument);
                                            sprintf(message, "Ban %s successfully!\n", argument.c_str());
                                            send(c_fd,message, strlen(message), 0);
                                            bzero(buffer, BUFF_SIZE);
                                            read_ban = recv(c_fd, &buffer, sizeof(buffer), 0);//wait for client 
                                            if (read_ban == 0){
                                                jump_ban = 1;
                                            }
                                        }
                                        break;
                                    }
                                }
                                size_t find_space = cmd_string.find(" ");
                                if (find_space == 0){
                                    break;
                                }
                                argument = cmd_string.substr(0, find_space);
                                if (strncmp(argument.c_str(), " ", 1)==0){
                                    break;
                                }
                                cmd_string = cmd_string.substr(cmd_string.find(" ")+1, cmd_string.length());
                                if (argument == "admin"){
                                    send(c_fd, "You cannot ban yourself!\n", strlen("You cannot ban yourself!\n"), 0);
                                    bzero(buffer, BUFF_SIZE);
                                    read_ban = recv(c_fd, &buffer, sizeof(buffer), 0);//wait for client to receive permission_msg
                                    continue;
                                }
                                else{
                                        if (blocklist.count(argument)!=0){
                                            sprintf(message, "User %s is already on the blocklist!\n", argument.c_str());
                                            send(c_fd,message, strlen(message), 0);
                                            bzero(buffer, BUFF_SIZE);
                                            read_ban = recv(c_fd, &buffer, sizeof(buffer), 0);//wait for client to receive permission_msg
                                            if (read_ban == 0){
                                                jump_ban = 1;
                                                break;
                                            }
                                        }
                                        else{
                                            blocklist.insert(argument);
                                            sprintf(message, "Ban %s successfully!\n", argument.c_str());
                                            send(c_fd,message, strlen(message), 0);
                                            bzero(buffer, BUFF_SIZE);
                                            recv(c_fd, &buffer, sizeof(buffer), 0);//wait for client to receive permission_msg
                                            if (read_ban == 0){
                                                jump_ban = 1;
                                                break;
                                            }
                                        }
                                }
                            }
                            if (jump_ban == 1){
                                jump_ban = 0;
                                continue;
                            }
                            send(c_fd, "END!!!!!-1-1", strlen("END!!!!!-1-1"), 0);
                        }
                        else if (command == "unban"){
                            if (strcmp(clients[c_fd].name, "admin")!=0){
                                send(c_fd, "Permission denied.\n", strlen("Permission denied.\n"), 0);
                                continue;
                            }
                            else    send(c_fd, "ok", strlen("ok"), 0);
                            bzero(buffer, BUFF_SIZE);
                            int read_unban;
                            read_unban = recv(c_fd, &buffer, sizeof(buffer), 0);//wait for client to receive permission_msg
                            if (read_unban == 0){
                                continue;
                            }
                            string argument;
                            string unban_msg = "";
                            int jump_unban = 0;
                            while(1){
                                if (cmd_string.find(" ")==-1){ 
                                    if (cmd_string.length() == 0){
                                        break;
                                    }
                                    argument = cmd_string;
                                    if (blocklist.count(argument)==0){
                                        sprintf(message, "User %s is not on the blocklist!\n", argument.c_str());
                                        send(c_fd, message, strlen(message), 0);
                                        bzero(buffer, BUFF_SIZE);
                                        read_unban = recv(c_fd, &buffer, sizeof(buffer), 0);
                                        if (read_unban == 0){
                                            jump_unban = 1;
                                        }
                                    }
                                    else{
                                        blocklist.erase(argument);
                                        sprintf(message, "Successfully removed %s from the blocklist!\n", argument.c_str());
                                        send(c_fd, message, strlen(message), 0);
                                        bzero(buffer, BUFF_SIZE);
                                        read_unban = recv(c_fd, &buffer, sizeof(buffer), 0);
                                        if (read_unban == 0){
                                            jump_unban = 1;
                                        }
                                    }
                                    break;   
                                }
                                size_t find_space = cmd_string.find(" ");
                                if (find_space == 0){
                                    break;
                                }
                                argument = cmd_string.substr(0, find_space);
                                cmd_string = cmd_string.substr(cmd_string.find(" ")+1, cmd_string.length());
                                if (blocklist.count(argument)==0){
                                    sprintf(message, "User %s is not on the blocklist!\n", argument.c_str());
                                    send(c_fd, message, strlen(message), 0);
                                    bzero(buffer, BUFF_SIZE);
                                    read_unban = recv(c_fd, &buffer, sizeof(buffer), 0);
                                    if (read_unban == 0){
                                        jump_unban = 1;
                                        break;
                                    }
                                }
                                else{
                                    blocklist.erase(argument);
                                    sprintf(message, "Successfully removed %s from the blocklist!\n", argument.c_str());
                                    send(c_fd, message, strlen(message), 0);
                                    bzero(buffer, BUFF_SIZE);
                                    read_unban = recv(c_fd, &buffer, sizeof(buffer), 0);
                                    if (read_unban == 0){
                                        jump_unban = 1;
                                        break;
                                    }
                                }
                            }
                            if (jump_unban == 1){
                                jump_unban = 0;
                                continue;
                            }
                            send(c_fd, "END!!!!!-1-1", strlen("END!!!!!-1-1"), 0);
                        }
                        else if (command == "put"){
                            char Message[BUFF_SIZE] = {};
                            int read_stat;
                            int read_put;
                            if (clients[c_fd].status == 0){
                                if (blocklist.count(string(clients[c_fd].name)) > 0){
                                    sendstatus = send(c_fd, "Permission denied.\n", strlen("Permission denied.\n"), 0);
                                    continue;
                                }
                                else{
                                    sendstatus = send(c_fd, "not banned.", strlen("not banned."), 0); //user is not banned
                                }      
                                bzero(Message, BUFF_SIZE);
                                read_put = recv(c_fd, &Message, sizeof(Message), 0); //read file_exist message
                                if (read_put == 0){
                                    continue;
                                }
                                if (strcmp("ok", Message)!=0){  //file doesn't exist
                                    continue;
                                }            
                                else{
                                    send(c_fd, "ok", strlen("ok"), 0);
                                }
                                
                                if (cmd_string.find(" ") != -1){
                                    cmd_string = cmd_string.substr(0, cmd_string.find(" "));
                                }            
                                
                                string file_path = "./";
                                file_path += string(clients[c_fd].name);
                                file_path += "/";
                                file_path += cmd_string;     

                                FILE *fp = fopen(file_path.c_str(), "wb");
                                clients[c_fd].fp = fp;
                                clients[c_fd].status = 1;
                            }
                            else { //client[c_fd].status = 1;
                                send(c_fd, "ok", strlen("ok"), 0);
                                bzero(Message, BUFF_SIZE);
                                read_stat = recv(c_fd, &Message, sizeof(Message), 0);
                                if (read_stat == 0){
                                    clients[c_fd].status = 0;
                                    continue;
                                }
                                if (strcmp(Message, "ENDOFFILE!!!!!-1-1")==0){
                                    fclose(clients[c_fd].fp);
                                    clients[c_fd].status = 0;
                                    break;
                                }
                                write_byte = fwrite(Message, sizeof(char), read_stat, clients[c_fd].fp); 
                                send(c_fd, "ok", strlen("ok"), 0);//
                            }
                        }
                        else if (command == "get"){
                            char recvdMessage[BUFF_SIZE] = {};
                            char message[BUFF_SIZE] = {};
                            int read_get;
                            if (clients[c_fd].status == 0){ //first time receive the command
                                if (blocklist.count(string(clients[c_fd].name)) > 0){
                                    sendstatus = send(c_fd, "Permission denied.\n", strlen("Permission denied.\n"), 0);
                                    continue;
                                }
                                else{
                                    sendstatus = send(c_fd, "not banned.", strlen("not banned."), 0); //user is not banned
                                }     
                                bzero(recvdMessage, BUFF_SIZE);
                                read_get = recv(c_fd, &recvdMessage, sizeof(recvdMessage), 0); //client has received permission msg
                                if (read_get == 0){
                                    continue;
                                }
                                if (cmd_string.find(" ") != -1){
                                    cmd_string = cmd_string.substr(0, cmd_string.find(" "));
                                }
                                string filename = "./";
                                filename += string(clients[c_fd].name);
                                filename += "/";
                                filename += cmd_string;
                                FILE *fp = fopen(filename.c_str(), "rb");
                                if(fp == NULL) {
                                    send(c_fd, "file doesn't exist!!!", strlen("file doesn't exist!!!"), 0);
                                    continue;
                                }            
                                else{
                                    clients[c_fd].fp = fp;
                                    int sent_stat = send(c_fd, "ok", strlen("ok"), 0);
                                    bzero(recvdMessage, BUFF_SIZE);
                                    read_get = recv(c_fd, &recvdMessage, sizeof(recvdMessage), 0);  //client has read file_exist message
                                    if (read_get == 0){
                                        continue;
                                    }
                                    int read_num;
                                    if ((read_num = fread(message, sizeof(char), sizeof(message), clients[c_fd].fp)) > 0){ //BUFF_SIZE-1
                                        send(c_fd, message, read_num, 0);  //strlen(message)
                                        bzero(recvdMessage, sizeof(char) * BUFF_SIZE);
                                        read_get = recv(c_fd, &recvdMessage, sizeof(recvdMessage), 0);//receive ok
                                        if (read_get == 0)
                                            continue;
                                        send(c_fd, "can send command again", strlen("can send command again"), 0);  
                                    }
                                    clients[c_fd].status = 1;
                                }
                            } 
                            else { //clients[c_fd].status == 1
                                bzero(message, sizeof(char) * BUFF_SIZE);
                                int read_num;
                                if ((read_num = fread(message, sizeof(char), sizeof(message), clients[c_fd].fp)) > 0){
                                    send(c_fd, message, read_num, 0); 
                                    bzero(recvdMessage, sizeof(char) * BUFF_SIZE);
                                    read_get = recv(c_fd, &recvdMessage, sizeof(recvdMessage), 0);//receive ok
                                    if (read_get == 0){
                                        clients[c_fd].status = 0;
                                        continue;
                                    }
                                    send(c_fd, "can send command again", strlen("can send command again"), 0);  
                                }
                                else{
                                    send(c_fd, "ENDOFFILE!!!!!-1-1", strlen("ENDOFFILE!!!!!-1-1"), 0);
                                    fclose(clients[c_fd].fp);
                                    clients[c_fd].fp = NULL;
                                    clients[c_fd].status = 0;
                                }
                            }
                        }
                        else if (command == "play"){
                            
                            int read_play;
                            char recvdMessage[BUFF_SIZE] = {};
                            char message[BUFF_SIZE] = {};
                            if (clients[c_fd].status == 0){ //first time receive the command
                                if (blocklist.count(string(clients[c_fd].name)) > 0){
                                    sendstatus = send(c_fd, "Permission denied.\n", strlen("Permission denied.\n"), 0);
                                    continue;
                                }
                                else{
                                    sendstatus = send(c_fd, "not banned.", strlen("not banned."), 0); //user is not banned
                                }     
                                bzero(recvdMessage, BUFF_SIZE);
                                read_play = recv(c_fd, &recvdMessage, sizeof(recvdMessage), 0); //client has received permission msg
                                if (read_play == 0){
                                    continue;
                                }
                                if (cmd_string.find(" ") != -1){
                                    cmd_string = cmd_string.substr(0, cmd_string.find(" "));
                                }
                                string filename = "./";
                                filename += string(clients[c_fd].name);
                                filename += "/";
                                filename += cmd_string;
  
                                FILE *fp = fopen(filename.c_str(), "rb");
                                char fileName[BUFF_SIZE];
                                strcpy(fileName, filename.c_str());
                                char *extension = strrchr(fileName, '.');
                                if(fp == NULL || extension == NULL || strncmp(extension, ".mpg", 4)) {
                                    if (fp == NULL){
                                        send(c_fd, "file doesn't exist.\n", strlen("file doesn't exist.\n"), 0);
                                    }
                                    else{
                                        sprintf(message, "%s is not an mpg file.\n", cmd_string.c_str());
                                        send(c_fd, message, strlen(message), 0);                      
                                        fclose(fp);              
                                    }
                                    continue;
                                }    
                                else{   //file exists and it is an mpeg file
                                    fclose(fp);
                                    send(c_fd, "ok", strlen("ok"), 0);
                                    bzero(recvdMessage, BUFF_SIZE);
                                    read_play = recv(c_fd, &recvdMessage, sizeof(recvdMessage), 0);  //client has read file_exist message
                                    if (read_play == 0){
                                        continue;
                                    }
                                    // Get the resolution of the video
                                    clients_cap[c_fd].open(filename.c_str());
                                    int width = clients_cap[c_fd].get(CAP_PROP_FRAME_WIDTH);
                                    int height = clients_cap[c_fd].get(CAP_PROP_FRAME_HEIGHT);
                                    clients[c_fd].imgServer = Mat::zeros(height, width, CV_8UC3);                          
                                    if (!clients[c_fd].imgServer.isContinuous())
                                    {
                                        clients[c_fd].imgServer = clients[c_fd].imgServer.clone();
                                    }
                                    int image_size = clients[c_fd].imgServer.total() * clients[c_fd].imgServer.elemSize();
                                    sprintf(message, "%d %d", width, height);
                                    send(c_fd, message, strlen(message), 0);  
                                    clients[c_fd].status = 1;                    
                                }
                            } 
                            else { //clients[c_fd].status = 1
                                clients_cap[c_fd] >> clients[c_fd].imgServer;
                                if (!clients[c_fd].imgServer.empty()){
                                    
                                    send(c_fd, "ok", strlen("ok"), 0);
                                    bzero(buffer, BUFF_SIZE);
                                    int recv_num;
                                    recv_num = recv(c_fd, buffer, BUFF_SIZE, 0);
                                    if (recv_num == 0){
                                        clients[c_fd].status = 0;
                                        continue;
                                    }
                                    
                                    int image_size = clients[c_fd].imgServer.total() * clients[c_fd].imgServer.elemSize();
                                    send(c_fd, clients[c_fd].imgServer.data, image_size, 0);
                                    bzero(buffer, BUFF_SIZE);
                                    if (recv(c_fd, buffer, BUFF_SIZE, 0) == 0){ //receive "0" or "1"
                                        clients[c_fd].status = 0;
                                        clients_cap[c_fd].release();
                                        continue;
                                    }
                                    if (buffer[0] == '0'){
                                        clients[c_fd].status = 0;
                                        clients_cap[c_fd].release();
                                        continue;
                                    }
                                    send(c_fd, "ok", strlen("ok"), 0);
                                }
                                else{
                                    send(c_fd, "ENDOFVIDEO!!!!!-1-1", strlen("ENDOFVIDEO!!!!!-1-1"), 0);
                                    clients_cap[c_fd].release();
                                    clients[c_fd].status = 0;
                                    break;
                                }
                            }
                            
                        }
                        else{ //command not found
                            if (blocklist.count(string(clients[c_fd].name)) > 0){
                                sendstatus = send(c_fd, "Permission denied.\n", strlen("Permission denied.\n"), 0);
                            }
                            else{
                                sendstatus = send(c_fd, "Command not found.\n", strlen("Command not found.\n"), 0); 
                            }      
                        }
                    }                    
                }
            }
        }
    }
    return 0;
}

