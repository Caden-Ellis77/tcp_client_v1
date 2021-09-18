#include "tcp_client.h"
#include "log.h"
#include <ctype.h>
#define ARG_ERROR 1
#define MAX_PORT_NUMBER 65535
#define BACKLOG 10

void print_help_menu() {

    fprintf(stderr, "\nUsage: tcp_client [--help] [-v] [-h HOST] [-p PORT] ACTION MESSAGE\n\n"
                    "Arguments:\n   "
                    "ACTION   Must be uppercase, lowercase, title-case, \n   "
                    "         reverse, or shuffle.\n   "
                    "MESSAGE  Message to send to the server\n\n"
                    "Options:\n"
                    "   --help\n"
                    "   -v, --verbose\n"
                    "   --host HOSTNAME, -h HOSTNAME\n"
                    "   --port PORT, -p PORT\n");
}

/*
Description:
    Parses the commandline arguments and options given to the program.
Arguments:
    int argc: the amount of arguments provided to the program (provided by the main function)
    char *argv[]: the array of arguments provided to the program (provided by the main function)
    Config *config: An empty Config struct that will be filled in by this function.
Return value:
    Returns a 1 on failure, 0 on success
*/
int tcp_client_parse_arguments(int argc, char *argv[], Config *config) {

    int option_index = 0;

    // Check if the help option is given before hand, to close out the program
    for (int i = 0; i < argc; i++) {
        // If any argument is "--help", print the help menu and end right there
        if (!strcmp(argv[i], "--help")) {
            log_set_level(LOG_DEBUG);
            print_help_menu();
            return ARG_ERROR;
        }
    }

    static struct option long_options[] = {{"verbose", no_argument, NULL, 'v'},
                                           {"host", required_argument, NULL, 'h'},
                                           {"port", required_argument, NULL, 'p'},
                                           {0, 0, 0, 0}};

    int opt;
    // loop over all of the options
    while ((opt = getopt_long(argc, argv, "vh:p:", long_options, &option_index)) != -1) {
        switch (opt) {
        case 'v':
            log_set_level(LOG_DEBUG);
            log_debug("Setting verbose mode");
            break;
        case 'h':
            log_debug("Host: %s", optarg);
            config->host = optarg;
            break;
        case 'p':

            // Ensure that all characters in the port arg are integers
            for (int i = 0; optarg[i] != 0; i++) {
                if (!isdigit(optarg[i])) {
                    log_error("Incorrect port number usage.");
                    print_help_menu();
                    return ARG_ERROR;
                }
            }
            int port = atoi(optarg);
            // Make sure the port number is in range
            if (port > MAX_PORT_NUMBER) {
                log_error("Incorrect port number usage. Please specify a port in range");
                print_help_menu();
                return ARG_ERROR;
            }
            log_debug("Port: %s", optarg);
            config->port = optarg;
            break;
        case '?':
            print_help_menu();
            return ARG_ERROR;
            break;
        default:
            log_error("getopt returned character code 0%o", opt);
            print_help_menu();
            return ARG_ERROR;
            break;
        }
    }

    // Make sure there are 2 argmuments after the options
    if ((argc - optind) != 2) {
        log_error("Incorrect number of arguments");
        print_help_menu();
        return ARG_ERROR;
    } else {

        // Make sure the action is one of the 5 valid ones
        if (strcmp(argv[optind], "uppercase") && strcmp(argv[optind], "lowercase") &&
            strcmp(argv[optind], "reverse") && strcmp(argv[optind], "title-case") &&
            strcmp(argv[optind], "shuffle")) {
            log_error("Unrecognized Action: %s", argv[optind]);
            print_help_menu();
            return ARG_ERROR;
        }

        // Set the arguments in config
        config->action = argv[optind];
        config->message = argv[++optind];
        log_debug("Action: %s", config->action);
        log_debug("Message: %s", config->message);
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////
/////////////////////// SOCKET RELATED FUNCTIONS //////////////////////
///////////////////////////////////////////////////////////////////////

/*
Description:
    Creates a TCP socket and connects it to the specified host and port.
Arguments:
    Config config: A config struct with the necessary information.
Return value:
    Returns the socket file descriptor or -1 if an error occurs.
*/
int tcp_client_connect(Config config) {

    struct addrinfo hints, *res;
    int sockfd;

    // first, load up address structs with getaddrinfo():
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;     // use IPv4 or IPv6, whichever
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    int return_value;
    if ((return_value = getaddrinfo(config.host, config.port, &hints, &res)) != 0) {
        log_error("getaddrinfo failed. %s\n", gai_strerror(return_value));
        return -1;
    }

    log_info("Creating socket");
    if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
        log_error("client: socket failed to create");
        return TCP_CLIENT_BAD_SOCKET;
    }

    log_info("Connecting socket");
    if (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
        close(sockfd);
        log_error("client: failed to connect");
        return TCP_CLIENT_BAD_SOCKET;
    }

    if (res == NULL) {
        log_error("client: failed to connect\n");
        return TCP_CLIENT_BAD_SOCKET;
    }

    log_info("Returning socket file descriptor");
    return sockfd;
}

/*
Description:
    Creates and sends request to server using the socket and configuration.
Arguments:
    int sockfd: Socket file descriptor
    Config config: A config struct with the necessary information.
Return value:
    Returns a 1 on failure, 0 on success
*/
int tcp_client_send_request(int sockfd, Config config) {
    int message_len, bytes_sent, send_len;
    char request[TCP_CLIENT_MAX_RECEIVE_SIZE];

    // Find the length of the message
    message_len = strlen(config.message);
    log_info("Configuring message to be sent");

    // Create the request to be sent
    sprintf(request, "%s %d %s", config.action, message_len, config.message);
    log_debug("Sending message \"%s\"", request);
    send_len = strlen(request);

    // Send the message
    bytes_sent = send(sockfd, request, send_len, 0);
    if (bytes_sent == -1) {
        log_error("Sending error");
        return 1;
    }
    log_debug("Bytes sent in message: %d", bytes_sent);
    return 0;
}

/*
Description:
    Receives the response from the server. The caller must provide an already allocated buffer.
Arguments:
    int sockfd: Socket file descriptor
    char *buf: An already allocated buffer to receive the response in
    int buf_size: The size of the allocated buffer
Return value:
    Returns a 1 on failure, 0 on success
*/
int tcp_client_receive_response(int sockfd, char *buf, int buf_size) {
    int return_value;
    log_info("Starting to receive");
    if ((return_value = recv(sockfd, buf, buf_size, 0)) <= 0) {
        log_error("Failed on recv. Code %d", return_value);
        return 1;
    }
    log_debug("Recv succeeded. Bytes received: %d", return_value);
    return 0;
}

/*
Description:
    Closes the given socket.
Arguments:
    int sockfd: Socket file descriptor
Return value:
    Returns a 1 on failure, 0 on success
*/
int tcp_client_close(int sockfd) {
    log_info("Closing socket");
    int return_value;
    if ((return_value = close(sockfd)) != 0) {
        log_debug("Close failed. Code: %d", return_value);
        return 1;
    }
    return 0;
}