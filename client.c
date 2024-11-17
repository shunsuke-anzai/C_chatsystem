#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    int state = 1;
    int sock;
    struct sockaddr_in svr;
    struct hostent *hp;
    int nbytes;
    fd_set rfds; /* select() で用いるファイル記述子集合 */
    struct timeval tv; /* select() が返ってくるまでの待ち時間を指定する変数 */
    char rbuf[1024], sbuf[1024], name[99]={0};

    if(state == 1){
        if(argc != 3){
            printf("Usage: ./chatclient hostname username\n");
            exit(1);
        }

        /* ソケットの生成 */
        if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
            perror("socket");
            exit(1);
        }

        /* server 受付用ソケットの情報設定 */
        if ((hp = gethostbyname(argv[1])) == NULL) {
            fprintf(stderr, "unknown host %s\n", argv[1]);
            exit(1);
        }
        bzero(&svr, sizeof(svr));
        svr.sin_family = AF_INET;
        bcopy(hp->h_addr, &svr.sin_addr, hp->h_length);
        svr.sin_port = htons(10140); /* ポート番号 10140 */

        /* サーバーに接続 */
        if (connect(sock, (struct sockaddr *)&svr, sizeof(svr)) < 0) {
            perror("connect");
            exit(1);
        }

        if(write(1,"connected to server\n",20)<0){
                perror("write");
                exit(1);
        }

        state = 2;

    }

    if(state == 2){
        bzero(rbuf, sizeof(rbuf));
        if ((nbytes = read(sock, rbuf, 17)) < 0) {
            perror("read");
            exit(1);
        }

        if(strcmp(rbuf, "REQUEST ACCEPTED\n") == 0){
            if(write(1,"join request accepted\n",22)<0){
                perror("write");
                exit(1);
            }
            state = 3;
        }else{
            if(write(1,"join request rejected\n",22)<0){
                perror("write");
                exit(1);
            }
            state = 6;
        }
    }

    if(state == 3){
        strcpy(name,argv[2]);
        name[strlen(name)] = '\n';

        if ((nbytes = write(sock, name, sizeof(name))) < 0) {
            perror("write");
            exit(1);
        }

        bzero(rbuf, sizeof(rbuf));
        if ((nbytes = read(sock, rbuf, 20)) < 0) {
            perror("read");
            exit(1);
        }

        if(strcmp(rbuf, "USERNAME REGISTERED\n") == 0){
            if(write(1,"user name registered\n",21)<0){
                perror("write");
                exit(1);
            }
            state = 4;
        }else{
            if(write(1,"USERNAME REJECTED\n",18)<0){
                perror("write");
                exit(1);
            }
            state = 6;
        }
    }

    if(state == 4){
        do {
            /* 入力を監視するファイル記述子の集合を変数 rfds にセットする */
            FD_ZERO(&rfds); /* rfds を空集合に初期化 */
            FD_SET(0, &rfds); /* 標準入力 */
            FD_SET(sock, &rfds); /* クライアントを受け付けたソケット */

            /* 監視する待ち時間を 1 秒に設定 */
            tv.tv_sec = 1;
            tv.tv_usec = 0;

            /* 標準入力とソケットからの受信を同時に監視する */
            if (select(sock + 1, &rfds, NULL, NULL, &tv) > 0) {
                if (FD_ISSET(0, &rfds)) { /* 標準入力から入力があったなら */
                    /* 標準入力から読み込みクライアントに送信 */
                    if (fgets(sbuf, sizeof(sbuf), stdin) != NULL) {
                        if ((nbytes = write(sock, sbuf, strlen(sbuf))) < 0) {
                            perror("write");
                            exit(1);
                        }
                    }else{
                        break;
                    }
                }

                if (FD_ISSET(sock, &rfds)) { /* ソケットから受信したなら */
                    /* ソケットから読み込み端末に出力 */
                    bzero(rbuf, sizeof(rbuf));
                    if ((nbytes = read(sock, rbuf, sizeof(rbuf))) < 0) {
                        perror("read");
                        exit(1);
                    } else {
                        write(1, rbuf, nbytes);
                    }
                }
            }
        } while (1);
        state = 5;
    }

    if(state == 5){
        close(sock);
        printf("closed\n");
        exit(0);
    }

    if(state == 6){
        close(sock);
        printf("closed\n");
        exit(1);
    }
}