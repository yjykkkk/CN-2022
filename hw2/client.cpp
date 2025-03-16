#include<sys/socket.h> 
#include<arpa/inet.h>
#include<sys/ioctl.h>
#include<net/if.h>
#include<unistd.h> 
#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include <iostream> 
#include <string>
#include <fstream>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>
#include "opencv2/opencv.hpp"

using namespace std; 
using namespace cv;
#define BUFF_SIZE 1024
#define CMD_BUFF_SIZE 4096  //terminal buffer size
#define ERR_EXIT(a){ perror(a); exit(1); }


int main(int argc , char *argv[]){
    int sockfd, read_byte;
    struct sockaddr_in addr;
    char buffer[BUFF_SIZE] = {};
    char name[BUFF_SIZE];
    strcpy(name, argv[1]);
    string username(argv[1]);
    string ip_port(argv[2]);
    int idx =ip_port.find(":", 0);
    string ip_;
    ip_ = ip_.assign(ip_port, 0, idx);
    string port_;
    port_ = port_.assign(ip_port, idx+1, ip_port.length()-1-idx); //ip_port.length()-1-(idx+1)+1
    int port = std::atoi(port_.c_str());
    struct stat folder_stat;
    if(stat("./client_dir/", &folder_stat) != 0 || !(folder_stat.st_mode & S_IFDIR))
        mkdir("./client_dir/", 0777);
    chdir("./client_dir/");


    // Get socket file descriptor
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        ERR_EXIT("socket failed\n");
    }

    // Set server address
    bzero(&addr,sizeof(addr));
    addr.sin_family = AF_INET; 
    addr.sin_addr.s_addr = inet_addr(ip_.c_str()); //ip: 本機: 127.0.0.1
    addr.sin_port = htons(port);

    // Connect to the server
    if(connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0){
        ERR_EXIT("connect failed\n");
    }
    int write_stat = send(sockfd, name, strlen(name), 0);//MSG_DONTWAIT 
    if (write_stat < 0)
        cout<<"write name failed.\n";

    char input_cmd[CMD_BUFF_SIZE] = {};
    char message[BUFF_SIZE] = {};
    char permission_msg[BUFF_SIZE]= {};
    
    bzero(message, BUFF_SIZE);
    recv(sockfd, &message, sizeof(message), 0); 

    fprintf(stdout, "User %s successfully logged in.\n", name);

    while(1){
        printf("$ ");
        bzero(input_cmd, sizeof(char) * CMD_BUFF_SIZE);
        bzero(permission_msg, sizeof(char) * BUFF_SIZE);
        cin.getline(input_cmd, CMD_BUFF_SIZE, '\n');
        string input_cmd_string = string(input_cmd);
        string command = input_cmd_string.substr(0, input_cmd_string.find(" "));
        string send_cmd_msg;
        string argument;
        input_cmd_string = input_cmd_string.substr(input_cmd_string.find(" ")+1, input_cmd_string.length());
        if (command == "blocklist"){
            if (send(sockfd, input_cmd, strlen(input_cmd), 0) <0){ 
                cout<<"write command file\n";
            }
            bzero(permission_msg, BUFF_SIZE);
            recv(sockfd, &permission_msg, sizeof(permission_msg), 0); 
            if (strcmp(permission_msg, "Permission denied.\n") == 0){
                printf("Permission denied.\n");
                continue;
            }
            send(sockfd, "ok", strlen("ok"), 0);
            while(1){
                bzero(message, BUFF_SIZE);
                recv(sockfd, &message, sizeof(message), 0); 
                if (strcmp(message, "END!!!!!BL-1-1")==0){
                    break;
                }
                //if empty blocklist, do nothing, else print (blocklist content) OR (Permission denied.)             
                if (strcmp(message, "empty blocklist!!!\n") != 0){ 
                    printf("%s", message);
                }
                send(sockfd, "ok", strlen("ok"), 0);
                bzero(message, BUFF_SIZE);
            }
        }
        else if (command == "ban" || command == "unban"){
            if (send(sockfd, input_cmd, strlen(input_cmd), 0) <0){ 
                cout<<"write command file\n";
            }
            bzero(permission_msg, BUFF_SIZE);
            recv(sockfd, &permission_msg, sizeof(permission_msg), 0); 
            if (strcmp(permission_msg, "Permission denied.\n") == 0){
                printf("Permission denied.\n");
                continue;
            }
            send(sockfd, "ok", strlen("ok"), 0);
            while(1){
                bzero(message, BUFF_SIZE);
                recv(sockfd, &message, sizeof(message), 0); 
                if (strcmp(message, "END!!!!!-1-1")==0){
                    break;
                }
                printf("%s", message);
                send(sockfd, "ok", strlen("ok"), 0);
                bzero(message, BUFF_SIZE);
            }
        }
        else if (command == "ls"){
            if (send(sockfd, input_cmd, strlen(input_cmd), 0) <0){ 
                cout<<"write command file\n";
            }
            bzero(permission_msg, sizeof(char) * BUFF_SIZE);
            recv(sockfd, &permission_msg, sizeof(permission_msg), 0);
            if (strcmp(permission_msg, "Permission denied.\n") == 0){
                fprintf(stdout, "Permission denied.\n");
                continue;
            }
            send(sockfd, "ok", strlen("ok"), 0);
            while (1){
                bzero(message, sizeof(char) * BUFF_SIZE);
                recv(sockfd, &message, sizeof(message), 0);
                if (strcmp(message, "\n") == 0){
                    break;
                }
                printf("%s", message);
                send(sockfd, "ok", strlen("ok"), 0);
            }
        }
        else if (command == "put"){
            if (send(sockfd, input_cmd, strlen(input_cmd), 0) <0){ 
                cout<<"write command file\n";
            }
            bzero(permission_msg, sizeof(char) * BUFF_SIZE);
            recv(sockfd, &permission_msg, sizeof(permission_msg), 0);
            if (strcmp(permission_msg, "Permission denied.\n") == 0){ //user is banned
                fprintf(stdout, "Permission denied.\n");
                continue;
            }
            if (input_cmd_string.find(" ") != -1){
                input_cmd_string = input_cmd_string.substr(0, input_cmd_string.find(" "));
            }
            string filename = input_cmd_string;

            FILE *fp = fopen(filename.c_str(), "rb");
            if(fp == NULL) {
                fprintf(stdout, "%s doesn't exist.\n", filename.c_str());
                send(sockfd, "file does not exist!!!", strlen("file does not exist!!!"), 0);
                continue;
            }            
            else{
                int sent_stat = send(sockfd, "ok", strlen("ok"), 0);
            }
            char recvdMessage[BUFF_SIZE] = {};
            bzero(recvdMessage, BUFF_SIZE);
            recv(sockfd, &recvdMessage, sizeof(recvdMessage), 0);  //server has read file_exist message
            printf("putting %s...\n", filename.c_str());
            int sent;
            bzero(message, sizeof(char) * BUFF_SIZE);
            int read_num;
            send(sockfd, input_cmd, strlen(input_cmd), 0);
            bzero(recvdMessage, sizeof(char) * BUFF_SIZE);
            recv(sockfd, &recvdMessage, sizeof(recvdMessage), 0);//receive ok
            while( (read_num = fread(message, sizeof(char), sizeof(message), fp)) > 0 ) {
                sent = send(sockfd, message, read_num, 0); 
                bzero(message, sizeof(char) * BUFF_SIZE);
                bzero(recvdMessage, sizeof(char) * BUFF_SIZE);
                recv(sockfd, &recvdMessage, sizeof(recvdMessage), 0);//receive ok
                send(sockfd, input_cmd, strlen(input_cmd), 0);
                bzero(recvdMessage, sizeof(char) * BUFF_SIZE);
                recv(sockfd, &recvdMessage, sizeof(recvdMessage), 0);//receive ok
            }
            send(sockfd, "ENDOFFILE!!!!!-1-1", strlen("ENDOFFILE!!!!!-1-1"), 0);
            fclose(fp);
        }
        else if (command == "get"){
            if (send(sockfd, input_cmd, strlen(input_cmd), 0) <0){ 
                cout<<"write command file\n";
            }
            bzero(permission_msg, sizeof(char) * BUFF_SIZE);
            recv(sockfd, &permission_msg, sizeof(permission_msg), 0);//receive permission_msg (if I am banned)
            if (strcmp(permission_msg, "Permission denied.\n") == 0){ //user is banned
                fprintf(stdout, "Permission denied.\n");
                continue;
            }
            send(sockfd, "finish reading permission msg\n", strlen("finish reading permission msg\n"), 0);
            if (input_cmd_string.find(" ") != -1){
                input_cmd_string = input_cmd_string.substr(0, input_cmd_string.find(" "));
            }            
            string filename = input_cmd_string;
            char recvdMessage[BUFF_SIZE] = {};
            bzero(recvdMessage, BUFF_SIZE);
            recv(sockfd, &recvdMessage, sizeof(recvdMessage), 0);  //read file_exist message
            if (strcmp(recvdMessage, "ok") != 0){ //file doesn't exist
                printf("%s doesn't exist.\n", filename.c_str());
                continue;
            }
            else{
                send(sockfd, "ok", strlen("ok"), 0);
            }
            printf("getting %s...\n", filename.c_str());
            FILE *fp = fopen(filename.c_str(), "wb");
            int read_num;
            char Message[BUFF_SIZE] = {};
            bzero(Message, BUFF_SIZE);
            while(1){
                bzero(Message, BUFF_SIZE);
                read_num = recv(sockfd, &Message, sizeof(Message), 0);
                if (strcmp(Message, "ENDOFFILE!!!!!-1-1")==0){
                    fclose(fp);
                    break;
                }
                int write_bytes = fwrite(Message, sizeof(char), read_num, fp); 
                send(sockfd, "ok", strlen("ok"), 0);
                bzero(Message, BUFF_SIZE);
                recv(sockfd, &Message, sizeof(Message), 0); //read "can send command again"
                send(sockfd, input_cmd, strlen(input_cmd), 0);
            }
        }
        else if (command == "play"){
            if (send(sockfd, input_cmd, strlen(input_cmd), 0) <0){ 
                cout<<"write command file\n";
            }
            bzero(permission_msg, sizeof(char) * BUFF_SIZE);
            recv(sockfd, &permission_msg, sizeof(permission_msg), 0);//receive permission_msg (if I am banned)
            if (strcmp(permission_msg, "Permission denied.\n") == 0){ //user is banned
                fprintf(stdout, "Permission denied.\n");
                continue;
            }
            send(sockfd, "finish reading permission msg\n", strlen("finish reading permission msg\n"), 0);
            if (input_cmd_string.find(" ") != -1){
                input_cmd_string = input_cmd_string.substr(0, input_cmd_string.find(" "));
            }  
            string filename = input_cmd_string;
            char recvdMessage[BUFF_SIZE] = {};
            bzero(recvdMessage, BUFF_SIZE);
            recv(sockfd, &recvdMessage, sizeof(recvdMessage), 0);  //read file_exist message
            if (strcmp(recvdMessage, "ok") != 0){
                printf("%s", recvdMessage);
                continue;
            }
            else{
                send(sockfd, "ok", strlen("ok"), 0);
            }
            printf("playing the video...\n");

            bzero(recvdMessage, BUFF_SIZE);
            recv(sockfd, &recvdMessage, sizeof(recvdMessage), 0);  //read resolution message
            string resolution_msg = string(recvdMessage);
            string width_str, height_str;
            width_str = resolution_msg.substr(0, resolution_msg.find(" "));
            resolution_msg = resolution_msg.substr(resolution_msg.find(" ")+1, resolution_msg.length());
            height_str = resolution_msg;
            int width = atoi(width_str.c_str());
            int height = atoi(height_str.c_str());
            char buffer[BUFF_SIZE] = {};
            bzero(buffer, BUFF_SIZE);
            Mat frame = Mat::zeros(height, width, CV_8UC3);
            int image_size = frame.total() * frame.elemSize();
            if (!frame.isContinuous())
            {
                frame = frame.clone();
            } 
            uchar *iptr = frame.data;
            int receive_size;
            int end_stat = 0;
            while (1){
                send(sockfd, input_cmd, strlen(input_cmd), 0); // send "play video.mpg" again
                bzero(buffer, BUFF_SIZE);
                recv(sockfd, &buffer, sizeof(buffer), 0); 
                char c = (char)waitKey(33.3333);
                if (strcmp(buffer, "ok") == 0){
                    send(sockfd, "ok", strlen("ok"), 0);
                }
                else { //the video has ended
                    c = (char)waitKey(33.3333);
                    while (1){
                        c = (char)waitKey(33.3333);
                        if (c == 27){
                            destroyAllWindows();
                            end_stat = 1;
                            break;
                        }
                    }                    
                }
                if (end_stat == 1){
                    end_stat = 0;
                    break;
                }
                receive_size = recv(sockfd, iptr, image_size, MSG_WAITALL);
                while (receive_size < image_size){
                    receive_size += recv(sockfd, iptr+receive_size, image_size-receive_size, MSG_WAITALL);
                }
                imshow("Video", frame); 
                if (c == 27)
                {
                    send(sockfd, "0", strlen("0"), 0);
                    destroyAllWindows();
                    break;
                }
                else
                {
                    send(sockfd, "1", strlen("1"), 0);
                }
                bzero(recvdMessage, BUFF_SIZE);
                recv(sockfd, &recvdMessage, sizeof(recvdMessage), 0);  //read "ok"
            }
        }
        else{
            send(sockfd, "not found", strlen("not found"), 0);
            bzero(message, sizeof(char) * BUFF_SIZE);
            recv(sockfd, &message, sizeof(message), 0);
            printf("%s", message);
        }
    }
    return 0;
}



