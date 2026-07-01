namespace Constants{
    inline constexpr const char* SERVER_PORT = "60000";
    inline constexpr const char* NOT_NAMED = "UNNAMED";

    inline constexpr const int MAX_EVENTS = 256;
    inline constexpr const int MAX_HOSTS = 0x000800 - 0x000001; // 2047
    inline constexpr const int MAX_HOSTNAME_LENGTH = 16;
    inline constexpr const int MAX_CLIENT_ID_LENGTH = 4;
    inline constexpr const int BACKLOG = 10;
    inline constexpr const int BUFFER_SIZE = 0x08000000;
    inline constexpr const int STARTING_BUFFERS = 262144 - 128; // From 8 to 262016
    inline constexpr const int CLIENT_POINTERS = 128;
    inline constexpr const int BUFFER_SEGMENT_SIZE = 0x00000200;
    inline constexpr const int MAX_MESSAGE_SIZE = 0x00010000 - 0x00000200 - 0x00000008;
    inline constexpr const int BUFFER_READING_SIZE = BUFFER_SEGMENT_SIZE * 16;
    inline constexpr const int HEADER_SIZE = 8;

    inline constexpr const int READER_BUFFER_POINTER = BUFFER_SIZE - 0x00010000;

    inline constexpr const int SUCCESS = 0;
    inline constexpr const int NOTHING_TO_READ = 1;
    inline constexpr const int CLOSED_CONVERSATION = 2;
    inline constexpr const int INCOMPLETE_MESSAGE = 3;
    inline constexpr const int ERROR = 4;
    inline constexpr const int INVALID_CLIENT = 5;
    inline constexpr const int INVALID_MESSAGE = 6;
    inline constexpr const int EXCEEDED_CLIENT_MAX = 7;
    inline constexpr const int EXCEEDED_CLIENT_BUFFER_SIZE = 8;
    inline constexpr const int INSUFFICIENT_BUFFER_SPACE = 9;
}

namespace Commands{
    inline constexpr const char* ACK = "/received";
    inline constexpr const int ACK_LENGTH = 10;
}

namespace Types{
    inline constexpr const uint8_t USER = 1;
    inline constexpr const uint8_t GROUP = 2;
    inline constexpr const uint8_t AUTH_KEY = 3;
    inline constexpr const uint8_t SEND_REQUEST = 4;
    inline constexpr const uint8_t ACCEPT_REQUEST = 5;
}