#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <signal.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "wrap.h"
#include "data_type.h"
#include "utils.h"

#define DEBUG 1
#define SERV_PORT 8010
#define MAX_CLIENT_SIZE 100
#define MAX_BUF_SIZE 100
// SIGN_UP name passwd    size is 3
#define MAX_CMD_SIZE 10


int init();
int handle_request(int);
void do_message(char*, int, int);
void response(int fd, int code, const char *msg);

void print_client_list(struct client *head);
void add_client(struct client *head, int fd, struct sockaddr_in addr);
void remove_client(struct client *head, int fd);
char** parse_cmd(char *buf, int n);
void unparse_cmd(char** cmd);
void handle_signal(int signal);

void do_signup(int fd, struct user *users, char *name, char *passwd);
int check_user(struct user *users, int len, char *name);
void save_user(struct user *users, char *name, char *passwd);
void show_user(struct user *users, int len);

void do_signin(int fd, struct user *users, char *name, char *passwd);
void do_signout(int fd);
struct client * find_client(int fd);


static struct client *g_client_head;
static struct room *g_room_head;
static struct user g_users[100];
static int g_user_size = 0;


int main(int argc, char* argv[]) {
    init();

    int listenfd = Socket(AF_INET, SOCK_STREAM, 0);
    printf("listen fd=%d\n", listenfd);

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    // on_exit(handle_signal, (void*)&listenfd);
    // signal(SIGINT, handle_signal);
    
    struct sockaddr_in servaddr;
    bzero((void*)&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);
    Bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

    Listen(listenfd, 5);

    int epfd = Epoll_create(1);

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = listenfd;
    Epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);

    struct  epoll_event evlist[MAX_CLIENT_SIZE];
    
    while (1) {
        int count = Epoll_wait(epfd, evlist, MAX_CLIENT_SIZE, -1);
        for (int i=0; i<count; i++) {
            struct epoll_event *one = evlist + i;
            if (one->events & EPOLLIN) {
                int fd = one->data.fd;
                if (listenfd == fd) {
                    struct sockaddr_in clientaddr;
                    socklen_t len = sizeof(clientaddr);
                    int clientfd = Accept(fd, (struct sockaddr*)&clientaddr, &len);
                    printf("enter clientfd %d\n", clientfd);

                    struct epoll_event ev;
                    ev.events = EPOLLIN;
                    ev.data.fd = clientfd;
                    Epoll_ctl(epfd, EPOLL_CTL_ADD, clientfd, &ev);
                    add_client(g_client_head, clientfd, clientaddr);

                } else {
                    if (handle_request(fd) == 0) {
                        Epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &ev);
                        Close(fd);
                        remove_client(g_client_head, fd);
                        printf("client exit fd=%d\n", fd);
                    }
                }
            }
        }
    }

    return 0;
}

int init() {
    // create head node, fd is -1.
    struct client *c = malloc(sizeof(struct client));
    c->fd = -1;
    c->addr = NULL;
    c->next = NULL;
    g_client_head = c;

    // room_head

    // user
    g_user_size = 0;
}

void handle_signal(int signal) {
    printf("recv signal is %d....\n", signal);
    exit(0);
}

int handle_request(int fd) {
    char buf[MAX_BUF_SIZE];
    bzero(buf, MAX_BUF_SIZE);
    int n = Read(fd, buf, MAX_BUF_SIZE);
    if (n > 0) {
        do_message(buf, n, fd);
        //Write(fd, resp, n);
    }
    return n;
}

void do_message(char *buf, int n, int fd) {
    char** cmd = parse_cmd(buf, n);
    if (strcmp(cmd[0], "SIGN_UP") == 0) {
        do_signup(fd, g_users, cmd[1], cmd[2]);
    } else if (strcmp(cmd[0], "SIGN_IN") == 0) {
        do_signin(fd, g_users, cmd[1], cmd[2]);
    } else if (strcmp(cmd[0], "SIGN_OUT") == 0) {
        do_signout(fd);

    } else if (strcmp(cmd[0], "EXIT") == 0) {

    } else if (strcmp(cmd[0], "LIST_ROOM") == 0) {
    } else if (strcmp(cmd[0], "ENTER_ROOM") == 0) {
    } else if (strcmp(cmd[0], "EXIT_ROOM") == 0) {

    } else if (strcmp(cmd[0], "CREATE_ROOM") == 0) {
    } else if (strcmp(cmd[0], "DELETE_ROOM") == 0) {
    } else if (strcmp(cmd[0], "SEND_MSG") == 0) {
        // 转发到room中每个用户
    } else {
        printf("unknow command!\n");
    }

    unparse_cmd(cmd);
}

void unparse_cmd(char** cmd) {
    for (int i=0; i<MAX_CMD_SIZE; i++) {
        free(cmd[i]);
    }
    free(cmd);
}

char** parse_cmd(char *src, int len) {
    int size = MAX_CMD_SIZE * sizeof(char*);
    char** res = malloc(size);
    bzero(res, size);
    int i = 0;
    char *begin = NULL;
    for (char *p=src; p<(src+len); p++) {
        if (isspace(*p)) {
            if (begin != NULL) {
                int n = p - begin;
                res[i] = malloc(n+1);
                bzero(res[i], n+1);
                strncpy(res[i], begin, n);
                begin = NULL;
                i++;
            }
        } else {
            if (begin == NULL) {
                begin = p;
            }
        }
    }
    return res;
}

void add_client(struct client *head, int fd, struct sockaddr_in addr) {
    struct client *c = malloc(sizeof(struct client));
    c->fd = fd;
    c->next = NULL;
    int size = sizeof(struct sockaddr_in);
    c->addr = malloc(size);
    memcpy(c->addr, &addr, size);

    struct client *p = head;
    while (p->next) {
        p = p->next;
    } 
    p->next = c;

    if (DEBUG) print_client_list(head);
}

void remove_client(struct client *head, int fd) {
    struct client *pre = NULL;
    struct client *p = head;
    while (p) {
        if (p->fd == fd) {
            if (pre) pre->next = p->next;
            p->next = NULL;
            free(p->addr);
            free(p);
            break;
        }
        pre = p;
        p = p->next;
    }

    if (DEBUG) print_client_list(head);
}

void print_client_list(struct client *head) {
    struct client *p = head->next;
    while (p) {
        char str[INET_ADDRSTRLEN];
        const char *host = inet_ntop(AF_INET, &(p->addr->sin_addr), str, sizeof(str));
        int port = ntohs(p->addr->sin_port);
        printf("(%d %s:%d)-->\n", p->fd, host, port);
        p = p->next;
    }
}

void response(int fd, int code, const char *msg) {
    char s[100];
    bzero(s, 100);
    itoa(code, s);
    strcat(s, " ");
    strcat(s, msg);
    strcat(s, "\n");
    Write(fd, s, strlen(s));
}

void do_signup(int fd, struct user *users, char *name, char *passwd) {
    if (!check_user(users, g_user_size, name)) {
        response(fd, 400, "user has exist");
        return;
    }
    save_user(users, name, passwd);
    response(fd, 200, "SIGN_UP success");
}

int check_user(struct user *users, int len, char *name) {
    for (int i=0; i<len; i++) {
        if (strncmp(users->name, name, strlen(users->name)) == 0) {
            return 0;
        }
    }
    return 1;
}

void save_user(struct user *users, char *name, char *passwd) {
    struct user *p = users + g_user_size; 
    p->id = g_user_size;
    strncpy(p->name, name, strnlen(name, 100));
    strncpy(p->password, passwd, strnlen(passwd, 100));
    p->role = p->id == 0 ? 0 : 1;
    p->online = 0;
    g_user_size++;
}

void show_user(struct user *users, int len) {
    for (int i=0; i<len; i++) {
        printf("%d %s %s\n", users[i].id, users[i].name, users[i].password);
    }
}

void do_signin(int fd, struct user *users, char *name, char *passwd) {
    for (int i=0; i<g_user_size; i++) {
        struct user *p = &users[i];
        if (strncmp(name, p->name, strlen(p->name)) == 0) {
            if (strncmp(passwd, p->password, strlen(p->password)) == 0) {
                struct client *cli = find_client(fd);
                if (cli == NULL) printf("do_sigin() client not exist");  
                cli->usr = p;

                int len = 50;
                char s[len];
                bzero(s, len);
                strncat(s, "Welcome ", len);
                strncat(s, name, len);
                response(fd, 200, s); 

            } else {
                response(fd, 400, strcat("password is error, ", name)); 
            }
            return;
        } 
    }
    response(fd, 400, strcat(name, " is not exist, please sign up.")); 
}

void do_signout(int fd) {
    struct client *p = find_client(fd);
    p->usr = NULL;
}

struct client * find_client(int fd) {
    struct client *p = g_client_head;
    while (p && (fd != p->fd)) p = p->next;
    return p;
}