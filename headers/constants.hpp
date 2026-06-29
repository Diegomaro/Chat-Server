namespace Constants{
    inline constexpr const char* SERVER_PORT = "60000";
    inline constexpr const char* NOT_NAMED = "UNNAMED";

    inline constexpr const int MAX_EVENTS = 256;
    inline constexpr const int MAX_HOSTS = 0X100000; //APROX 1 MILLION
    inline constexpr const int MAX_HOSTNAME_LENGTH = 16;
    inline constexpr const int BACKLOG = 10;

    inline constexpr const int SUCCESS = 0;
    inline constexpr const int NOTHING_TO_READ = 1;
    inline constexpr const int CLOSED_CONVERSATION = 2;
    inline constexpr const int INCOMPLETE_MESSAGE_RESEND = 3;
    inline constexpr const int INCOMPLETE_MESSAGE_CONTINUE = 4;
    inline constexpr const int PERROR = 5;
    inline constexpr const int INVALID_CLIENT = 6;
    inline constexpr const int EXCEEDED_CLIENT_MAX = 7;
}

namespace Commands{
    inline constexpr const char* ACK = "/received";
    inline constexpr const int ACK_LENGTH = 10;
}