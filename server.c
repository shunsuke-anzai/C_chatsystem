#define MAXCLIENTS 5 

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void get_time_string(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", t);
}

int main(int argc, char **argv) {
    int state = 1;
    int i = 0;
    int select_flg = 0;
    int sock;
    int csock[MAXCLIENTS] = {0};
    struct sockaddr_in svr;
    struct sockaddr_in clt;
    struct hostent *cp;
    int k = 0;
    int nbytes, clen, reuse;
    fd_set rfds;
    struct timeval tv;
    char rbuf[1024], name[MAXCLIENTS][99];
    char msg[1224];

    while (1) {
        if (state == 1) {
            // ソケットの生成
            if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
                perror("socket");
                exit(1);
            }

            // ソケットアドレス再利用の指定
            reuse = 1;
            if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
                perror("setsockopt");
                exit(1);
            }

            // client 受付用ソケットの情報設定
            bzero(&svr, sizeof(svr));
            svr.sin_family = AF_INET;
            svr.sin_addr.s_addr = htonl(INADDR_ANY);
            svr.sin_port = htons(10140);

            // ソケットにソケットアドレスを割り当てる
            if (bind(sock, (struct sockaddr *)&svr, sizeof(svr)) < 0) {
                perror("bind");
                exit(1);
            }

            // 待ち受けクライアント数の設定
            if (listen(sock, 5) < 0) {
                perror("listen");
                exit(1);
            }

            // 参加クライアント数を初期化
            k = 0;

            state = 2;
        }

        if (state == 2) {
            // 入力を監視するファイル記述子の集合を変数 rfds にセットする
            FD_ZERO(&rfds);
            FD_SET(sock, &rfds);
            int max_fd = sock;
            for (int a = 0; a < k; a++) {
                FD_SET(csock[a], &rfds);
                if (csock[a] > max_fd) {
                    max_fd = csock[a];
                }
            }

            // 監視する待ち時間を 1 秒に設定
            tv.tv_sec = 1;
            tv.tv_usec = 0;
            select_flg = select(max_fd + 1, &rfds, NULL, NULL, &tv);

            if (select_flg < 0) {
                perror("select");
                exit(1);
            }

            state = 3;
        }

        if (state == 3) {
            int c=0;
            if (select_flg > 0) {
                if (FD_ISSET(sock, &rfds)) {
                    state = 4;
                    FD_CLR(sock, &rfds);
                } else {
                    for (int a = 0; a < k; a++) {
                        if (FD_ISSET(csock[a], &rfds)) {
                            i = a;
                            state = 6;
                            FD_CLR(csock[a], &rfds);
                            break;
                        }else{
                            c++;
                        }
                    }
                    if(c == k){
                        select_flg = 0;
                    }
                }
            } else {
                state = 2;
            }
        }

        if (state == 4) {
            //printf("s4");
            clen = sizeof(clt);
            int new_sock = accept(sock, (struct sockaddr *)&clt, &clen);
            if (new_sock < 0) {
                perror("accept");
                exit(2);
            }

            if (k < MAXCLIENTS) {
                csock[k] = new_sock;
                write(csock[k], "REQUEST ACCEPTED\n", 17);
                state = 5;
            } else {
                write(new_sock, "REQUEST REJECTED\n", 17);
                close(new_sock);
                state = 3;
            }
        }

        if (state == 5) {
            //printf("s5");
            int flg = 0;
            bzero(name[k],sizeof(name[k]));
            if ((nbytes = read(csock[k], name[k], 99)) < 0) {
                perror("read error");
                exit(1);
            }
            name[k][strlen(name[k])-1] = '\0'; // 改行文字を削除

            for (int a = 0; a < k; a++) {
                if (strcmp(name[a], name[k]) == 0) {
                    flg = 1;
                }
            }

            if (flg == 0) {
                write(csock[k], "USERNAME REGISTERED\n", 20);
                printf("%s is registered.\n", name[k]);

                // 参加メッセージを作成
                snprintf(msg, sizeof(msg), "USER JOINED: %s (Total users: %d)\n", name[k], k + 1);
                for (int a = 0; a <= k; a++) {
                    if (write(csock[a], msg, strlen(msg)) < 0) {
                        perror("write");
                        exit(1);
                    }
                }

                k++;
                state = 3;
            } else {
                write(csock[k], "USERNAME REJECTED\n", 19);
                printf("%s is rejected.\n", name[k]);
                close(csock[k]);
                csock[k] = 0;
                state = 3;
            }
        }

        if (state == 6) {
            //printf("S6");
            bzero(rbuf, sizeof(rbuf));
            nbytes = read(csock[i], rbuf, sizeof(rbuf));
            if (nbytes < 0) {
                perror("read");
                exit(1);
            } else if (nbytes == 0) {
                state = 7;
            } else {
                char time_str[100];
                get_time_string(time_str, sizeof(time_str));
                snprintf(msg, sizeof(msg), "%s %s>%s", time_str, name[i], rbuf);
                for (int a = 0; a < k; a++) {
                    if (write(csock[a], msg, strlen(msg)) < 0) {
                        perror("write");
                        exit(1);
                    }
                }
                state = 3;
            }
        }

        if (state == 7) {
            printf("%s left the chat.\n", name[i]);
            close(csock[i]);

            // 離脱メッセージを作成
            snprintf(msg, sizeof(msg), "USER LEFT: %s (Total users: %d)\n", name[i], k - 1);
            close(csock[i]);

            for (int a = i; a < k - 1; a++) {
                csock[a] = csock[a + 1];
                strncpy(name[a], name[a + 1], 99);
            }
            csock[k - 1] = 0;
            k--;
            for (int a = 0; a < k; a++) {
                if (write(csock[a], msg, strlen(msg)) < 0) {
                    perror("write");
                    exit(1);
                }
            }

            state = 3;
        }
    }
}
