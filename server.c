// server.c — Threaded TCP File-Based Task Manager
// Features: ADD, LIST, SEARCH, DONE, DELETE, DOWNLOAD, HELP, QUIT
// Thread-safe file handling with IST timestamps

#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define PORT 8080
#define BUFSZ 4096
#define TASKS_FILE "tasks.txt"
#define TMP_FILE   "tasks.tmp"
#define LOG_FILE   "server_log.txt"

pthread_mutex_t file_mx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t id_mx   = PTHREAD_MUTEX_INITIALIZER;
int next_id = 1;

void die(const char *msg) { perror(msg); exit(1); }

void set_timezone_ist() {
    setenv("TZ", "Asia/Kolkata", 1);
    tzset();
}

void timestamp(char *buf, size_t n) {
    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    strftime(buf, n, "%Y-%m-%d %H:%M:%S", &tmv);
}

void log_action(const char *msg) {
    pthread_mutex_lock(&file_mx);
    FILE *lfp = fopen(LOG_FILE, "a");
    if (lfp) {
        char ts[64];
        timestamp(ts, sizeof ts);
        fprintf(lfp, "[%s] %s\n", ts, msg);
        fclose(lfp);
    }
    pthread_mutex_unlock(&file_mx);
}

void trim_newline(char *s) {
    size_t n = strlen(s);
    while (n && (s[n - 1] == '\n' || s[n - 1] == '\r')) s[--n] = '\0';
}

void init_next_id() {
    pthread_mutex_lock(&file_mx);
    FILE *fp = fopen(TASKS_FILE, "a+");
    if (!fp) { pthread_mutex_unlock(&file_mx); die("open tasks.txt"); }
    rewind(fp);
    char line[BUFSZ];
    int maxid = 0, id;
    while (fgets(line, sizeof line, fp))
        if (sscanf(line, "%d|", &id) == 1 && id > maxid) maxid = id;
    fclose(fp);
    pthread_mutex_unlock(&file_mx);

    pthread_mutex_lock(&id_mx);
    next_id = maxid + 1;
    pthread_mutex_unlock(&id_mx);
}

void send_line(int fd, const char *line) { send(fd, line, strlen(line), 0); }
void send_end(int fd) { send_line(fd, "END\n"); }

void op_help(int cfd) {
    send_line(cfd,
        "Commands:\n"
        "  ADD <desc>|<priority>\n"
        "  LIST\n"
        "  SEARCH <keyword>\n"
        "  DONE <id>\n"
        "  DELETE <id>\n"
        "  DOWNLOAD\n"
        "  HELP\n"
        "  QUIT\n");
    send_end(cfd);
}

void op_add(int cfd, const char *payload) {
    char desc[BUFSZ] = "", priority[32] = "Medium";
    const char *bar = strchr(payload, '|');
    if (bar) {
        size_t dlen = bar - payload;
        if (dlen >= sizeof desc) dlen = sizeof desc - 1;
        memcpy(desc, payload, dlen);
        desc[dlen] = '\0';
        snprintf(priority, sizeof priority, "%s", bar + 1);
    } else {
        snprintf(desc, sizeof desc, "%s", payload);
    }

    trim_newline(desc);
    trim_newline(priority);
    if (!*desc) { send_line(cfd, "ERR No description\n"); send_end(cfd); return; }

    pthread_mutex_lock(&id_mx);
    int id = next_id++;
    pthread_mutex_unlock(&id_mx);

    char ts[64];
    timestamp(ts, sizeof ts);

    pthread_mutex_lock(&file_mx);
    FILE *fp = fopen(TASKS_FILE, "a");
    if (!fp) { pthread_mutex_unlock(&file_mx); send_line(cfd, "ERR AddFailed\n"); send_end(cfd); return; }
    fprintf(fp, "%d|%s|Pending|%s|%s\n", id, desc, priority, ts);
    fclose(fp);
    pthread_mutex_unlock(&file_mx);

    char msg[128];
    snprintf(msg, sizeof msg, "OK Added %d\n", id);
    send_line(cfd, msg);
    send_end(cfd);

    char logmsg[256];
    snprintf(logmsg, sizeof logmsg, "ADD id=%d \"%s\" %s", id, desc, priority);
    log_action(logmsg);
}

void op_list(int cfd) {
    pthread_mutex_lock(&file_mx);
    FILE *fp = fopen(TASKS_FILE, "r");
    if (!fp) { pthread_mutex_unlock(&file_mx); send_line(cfd, "No tasks file\n"); send_end(cfd); return; }
    char line[BUFSZ]; int empty = 1;
    while (fgets(line, sizeof line, fp)) { send_line(cfd, line); empty = 0; }
    fclose(fp);
    pthread_mutex_unlock(&file_mx);
    if (empty) send_line(cfd, "No tasks found\n");
    send_end(cfd);
}

void op_search(int cfd, const char *kw) {
    if (!kw || !*kw) { send_line(cfd, "ERR No keyword\n"); send_end(cfd); return; }

    pthread_mutex_lock(&file_mx);
    FILE *fp = fopen(TASKS_FILE, "r");
    if (!fp) { pthread_mutex_unlock(&file_mx); send_line(cfd, "No tasks file\n"); send_end(cfd); return; }
    char line[BUFSZ]; int found = 0;
    while (fgets(line, sizeof line, fp)) {
        if (strcasestr(line, kw)) { send_line(cfd, line); found = 1; }
    }
    fclose(fp);
    pthread_mutex_unlock(&file_mx);

    if (!found) send_line(cfd, "No matching tasks\n");
    send_end(cfd);

    char safe_kw[200];
    strncpy(safe_kw, kw, sizeof(safe_kw) - 1);
    safe_kw[sizeof(safe_kw) - 1] = '\0';
    char logmsg[256];
    snprintf(logmsg, sizeof logmsg, "SEARCH \"%s\"", safe_kw);
    log_action(logmsg);
}

void op_done(int cfd, int id) {
    char idstr[32];
    snprintf(idstr, sizeof idstr, "%d", id);
    int found = 0;
    char ts[64];
    timestamp(ts, sizeof ts);

    pthread_mutex_lock(&file_mx);
    FILE *in = fopen(TASKS_FILE, "r");
    FILE *out = fopen(TMP_FILE, "w");
    char line[BUFSZ];
    if (in && out) {
        while (fgets(line, sizeof line, in)) {
            if (strncmp(line, idstr, strlen(idstr)) == 0 && line[strlen(idstr)] == '|') {
                char *desc = strchr(line, '|') + 1;
                char *p2 = strchr(desc, '|');
                if (!p2) { fputs(line, out); continue; }
                *p2 = '\0';
                char *priority = strchr(p2 + 1, '|');
                if (!priority) { fputs(line, out); continue; }
                priority++;
                char *p3 = strchr(priority, '|');
                if (p3) *p3 = '\0';
                fprintf(out, "%d|%s|Completed|%s|%s\n", id, desc, priority, ts);
                found = 1;
            } else fputs(line, out);
        }
    }
    if (in) fclose(in);
    if (out) fclose(out);
    rename(TMP_FILE, TASKS_FILE);
    pthread_mutex_unlock(&file_mx);

    send_line(cfd, found ? "OK Done\n" : "ERR TaskNotFound\n");
    send_end(cfd);
    if (found) {
        char logmsg[128];
        snprintf(logmsg, sizeof logmsg, "DONE id=%d", id);
        log_action(logmsg);
    }
}

void op_delete(int cfd, int id) {
    char idstr[32];
    snprintf(idstr, sizeof idstr, "%d", id);
    int found = 0;
    pthread_mutex_lock(&file_mx);
    FILE *in = fopen(TASKS_FILE, "r");
    FILE *out = fopen(TMP_FILE, "w");
    char line[BUFSZ];
    if (in && out) {
        while (fgets(line, sizeof line, in)) {
            if (strncmp(line, idstr, strlen(idstr)) == 0 && line[strlen(idstr)] == '|')
                found = 1;
            else
                fputs(line, out);
        }
    }
    if (in) fclose(in);
    if (out) fclose(out);
    rename(TMP_FILE, TASKS_FILE);
    pthread_mutex_unlock(&file_mx);

    send_line(cfd, found ? "OK Deleted\n" : "ERR TaskNotFound\n");
    send_end(cfd);
    if (found) {
        char logmsg[128];
        snprintf(logmsg, sizeof logmsg, "DELETE id=%d", id);
        log_action(logmsg);
    }
}

void *client_thread(void *arg) {
    int cfd = *(int*)arg;
    free(arg);
    char buf[BUFSZ];
    for (;;) {
        ssize_t n = recv(cfd, buf, sizeof buf - 1, 0);
        if (n <= 0) break;
        buf[n] = '\0';
        trim_newline(buf);
        if (strncasecmp(buf, "ADD ", 4) == 0) op_add(cfd, buf + 4);
        else if (!strcasecmp(buf, "LIST")) op_list(cfd);
        else if (strncasecmp(buf, "SEARCH ", 7) == 0) op_search(cfd, buf + 7);
        else if (strncasecmp(buf, "DONE ", 5) == 0) op_done(cfd, atoi(buf + 5));
        else if (strncasecmp(buf, "DELETE ", 7) == 0) op_delete(cfd, atoi(buf + 7));
        else if (!strcasecmp(buf, "DOWNLOAD")) op_list(cfd);
        else if (!strcasecmp(buf, "HELP")) op_help(cfd);
        else if (!strcasecmp(buf, "QUIT")) { send_line(cfd, "BYE\n"); send_end(cfd); break; }
        else if (buf[0]) { send_line(cfd, "ERR UnknownCmd\n"); send_end(cfd); }
    }
    close(cfd);
    return NULL;
}

int main() {
    set_timezone_ist();
    init_next_id();

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) die("socket");
    int yes = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(PORT) };
    if (bind(sfd, (struct sockaddr*)&addr, sizeof addr) < 0) die("bind");
    if (listen(sfd, 16) < 0) die("listen");

    printf("Server listening on port %d (IST)\n", PORT);

    for (;;) {
        struct sockaddr_in cli;
        socklen_t clen = sizeof cli;
        int cfd = accept(sfd, (struct sockaddr*)&cli, &clen);
        if (cfd < 0) { if (errno == EINTR) continue; perror("accept"); continue; }
        int *pfd = malloc(sizeof *pfd);
        *pfd = cfd;
        pthread_t th;
        pthread_create(&th, NULL, client_thread, pfd);
        pthread_detach(th);
    }
}
