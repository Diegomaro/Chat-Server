namespace config{
    inline constexpr const char* SERVER_PORT = "60000";
    inline constexpr const char* NOT_NAMED = "UNNAMED"; // a user should not be unnamed

    inline constexpr const int MAX_EVENTS = 256;
    inline constexpr const int BACKLOG = 10;

    inline constexpr const uint32_t TOTAL_HOSTS_SPACES = 2048;
    inline constexpr const uint32_t MAX_HOSTS = TOTAL_HOSTS_SPACES - 1;
    inline constexpr const uint32_t BUFFER_SEGMENTS_PER_CLIENT = 128;
    inline constexpr const uint32_t BUFFER_SEGMENT_SIZE = 512;

    inline constexpr const uint8_t HOSTNAME_LENGTH = 16;
    inline constexpr const uint8_t CLIENT_KEY_LENGTH = 4;
    inline constexpr const uint8_t HEADER_SIZE = 8;

    inline constexpr const uint32_t TOTAL_BUFFER_SEGMENTS =  TOTAL_HOSTS_SPACES * BUFFER_SEGMENTS_PER_CLIENT; //262144
    inline constexpr const uint32_t BUFFER_SIZE = TOTAL_BUFFER_SEGMENTS * BUFFER_SEGMENT_SIZE;
    inline constexpr const uint32_t AVAILABLE_BUFFER_SEGMENTS = TOTAL_BUFFER_SEGMENTS - BUFFER_SEGMENTS_PER_CLIENT;
    // From 8 to 262016, 1 client space is reserved for sending out messages.
    inline constexpr const uint32_t READING_BUFFER_SIZE = BUFFER_SEGMENTS_PER_CLIENT * BUFFER_SEGMENT_SIZE;
    inline constexpr const uint32_t MAX_MESSAGE_SIZE = READING_BUFFER_SIZE - BUFFER_SEGMENT_SIZE - HEADER_SIZE;
    inline constexpr const uint32_t BUFFER_READING_SIZE = BUFFER_SEGMENT_SIZE * 16;
    inline constexpr const uint32_t READER_BUFFER_POINTER = BUFFER_SIZE - READING_BUFFER_SIZE;
}

namespace status{
    inline constexpr const uint8_t SUCCESS = 0;
    inline constexpr const uint8_t NOTHING_TO_READ = 1;
    inline constexpr const uint8_t RESOURCE_UNAVAILABLE = 2;
    inline constexpr const uint8_t CLOSED_CONVERSATION = 3;
    inline constexpr const uint8_t INCOMPLETE_MESSAGE = 4;
    inline constexpr const uint8_t ERROR = 5;
    inline constexpr const uint8_t INVALID_CLIENT = 6;
    inline constexpr const uint8_t INVALID_MESSAGE = 7;
    inline constexpr const uint8_t EXCEEDED_CLIENT_MAX = 8;
    inline constexpr const uint8_t EXCEEDED_CLIENT_BUFFER_SIZE = 9;
    inline constexpr const uint8_t INSUFFICIENT_BUFFER_SPACE = 10;
}

namespace types{
    inline constexpr const uint8_t USER = 1;
    inline constexpr const uint8_t GROUP = 2;
    inline constexpr const uint8_t AUTH_KEY = 3;
    inline constexpr const uint8_t SEND_REQUEST = 4;
    inline constexpr const uint8_t ACCEPT_REQUEST = 5;
    inline constexpr const uint8_t UPDATE = 6;
}