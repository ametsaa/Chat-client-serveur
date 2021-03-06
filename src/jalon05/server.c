#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/select.h>
#include <time.h>
#include <arpa/inet.h>

#include "fonction.h"
#include "user.h"
#include "salon.h"
#include "file_p2p.h"
#include "callback.h"




struct sockaddr_in init_serv_addr(int port);

int main(int argc, char** argv)
{

    char buffer[150] = {0};
    int sock_acc = 0;
    fd_set readfs;
    fd_set masterfs;
    int port;
    struct sockaddr_in addr;
    socklen_t addrlen;
    int i;
    int j;
    int max_fd;

    int _select;
    int max_fd_tmp;
    int sock_serv;
    int nb_connexion =0;
    char *mess_max_client ="max_cl";
    char *mess_conn_accepted ="co_acc";


    //Jalon3
    char* pseudo = malloc(20);
    char* all_user = malloc(200);
    char* date = malloc(20);
    char* addrINET = malloc(20);
    char* string_whois = malloc(100);
    int whois_indicator =0;
    //Jalon4
    int nick_user_indicator =0;
    char* msg_all = malloc(100);
    //char* msg_usage = malloc(200);
    //msg_usage ="Usage :\n /msg <user>\n /msgall\n /create <salon>\n /join <salon>\n /who\n /whois <user>\n /send <user> <filename> \n /nick <new_username>\n";
    //Jalon5
    char* receiving_user = malloc(100);
    char* filename = malloc(100);
    char * dest_addr=malloc(100);
    char * dest_port=malloc(20);

    char* p2p_msg = malloc(100);
    char* port_client=malloc(20);

    struct user* tab_user = malloc(MAX_CLIENT* sizeof(*tab_user)) ;

    if (tab_user == NULL){
        printf("erreur allocation");
    }
    memset(tab_user,'\0', sizeof(*tab_user));



    struct salon* tab_salon = malloc(MAX_CLIENT* sizeof(*tab_salon)) ;

    if (tab_salon == NULL){
        printf("erreur allocation");
    }
    memset(tab_salon,'\0', sizeof(*tab_salon));



    if (argc != 2)
    {
        fprintf(stderr, "usage: RE216_SERVER port\n");
        return 1;
    }


    //create the socket, check for validity!
    port = atoi(argv[1]);
    sock_serv = socket(AF_INET,SOCK_STREAM, IPPROTO_TCP);
    if (sock_serv < 0)
        error("erreur socket");

    max_fd = sock_serv+1;

    //init the serv_add structure
    addr = init_serv_addr(port);
    addrlen =  sizeof(addr);

    //perform the binding
    //we bind on the tcp port specified
    if (bind(sock_serv, (struct sockaddr*) & addr, sizeof(addr)) < 0)
        error("erreur bind");

    //specify the socket to be a server socket and listen for at most 20 concurrent client
    if (listen(sock_serv, MAX_CLIENT) < 0)
        error("erreur listen");

    printf("Serveur en écoute \n \n");

    //clean the set before adding file descriptors
    FD_ZERO(&readfs);
    FD_ZERO(&masterfs);

    //add the fd for server connection
    FD_SET(sock_serv, &readfs);
    FD_SET(sock_serv, &masterfs);


    for (;;)
    {
        readfs = masterfs;

        _select = select(max_fd, &readfs, NULL, NULL,NULL);
        if (_select > 0){
            max_fd_tmp = max_fd;
            for(i=0; i<max_fd_tmp; i++){
                if(FD_ISSET(i,&readfs)){
                    // socket server
                    if(i == sock_serv){

                        sock_acc = accept(sock_serv, (struct sockaddr*) & addr, &addrlen);
                        nb_connexion++;

                        if (sock_acc < 0)
                            error("Erreur acceptation");

                        //handle the case when there is too much client connected
                        if (nb_connexion > MAX_CLIENT){
                            if(write(sock_acc,mess_max_client,sizeof(mess_max_client)) < 0){
                                error("erreur write");
                            }
                            close(sock_acc);
                            nb_connexion--;
                        }
                        // send mess to client to let him know the connection has been accepted
                        else{
                            if(write(sock_acc,mess_conn_accepted,sizeof(mess_conn_accepted)) < 0){
                                error("erreur write");
                            }
                            // read the nickname (message receive is  /nick <pseudo>)
                            FD_SET(sock_acc, &masterfs);

                            if (sock_acc >= max_fd) {    // keep track of the max
                                max_fd =sock_acc+1;
                            }
                        }
                    } // end socket server


                    // socket client
                    else{

                        //Respond to the client messages until the client send "/quit"
                        memset(buffer, '\0', sizeof(buffer));
                        //read what the client has to say
                        if (read(i,buffer,sizeof(buffer)) < 0){
                            error("erreur read");
                        }
                        if (strcmp(buffer, "/quit\n") != 0 ){

                            // /nick --- When the user want to log in or change his nickname
                            if (strcmp(str_sub(buffer,0,5), "/nick ") == 0){
                                pseudo=get_2_arg(buffer);

                                // change the nickname
                                if (user_existing(tab_user+i-4)){
                                    change_nickname((tab_user+i-4), pseudo);
                                    if(write(i,"pseudo modifié",20) < 0){
                                        error("erreur write");
                                    }
                                }
                                else{

                                    //Load the current date
                                    getdate(date);
                                    //Get the IP adress
                                    addrINET = inet_ntoa(addr.sin_addr);
                                    // create a user
                                    set_user((tab_user+i-4), pseudo, date, addrINET, addr.sin_port, i-4);
                                    get_port(tab_user+i-4,port_client);

                                    sprintf(buffer, "%s", port_client);
                                    strcat(buffer," ");
                                    strcat(buffer,pseudo);
                                    if(write(i,buffer,strlen(buffer)) < 0){
                                        error("erreur write");
                                    }


                                    printf("connexion avec %s\n", get_nickname(tab_user + i -4));
                                }
                            }

                            handle_msg_from_client( buffer,tab_user, tab_salon, i, &nb_connexion);

                        }

                        
                        // /quit
                        else{
                            quit_salon(tab_salon, tab_user+i-4, get_user_salon(tab_user+i-4));
                            printf("fin de la connexion avec %s \n",get_nickname(tab_user +i -4));
                            delete_user(tab_user+i-4);
                            close(i);
                            nb_connexion--;
                            FD_CLR(i,&masterfs);
                            memset(buffer, '\0', sizeof(buffer));
                        }
                    }
                }


            }
        }

        else{
            printf("erreur select = %d \n",_select);
        }
    }
    //clean up server socketsock_acc
    close(sock_serv);
    return 0;
}
