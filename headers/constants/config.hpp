namespace config{
    inline constexpr const char* SERVER_PORT = "60000";

    inline constexpr const int MAX_EVENTS = 256;
    inline constexpr const int BACKLOG = 10;
    inline constexpr const int INITIAL_HASHTABLE_SIZE = 16;
    inline constexpr const int MAX_LENGTH_OF_INT_CHOICE = 5;

    inline constexpr const uint32_t TOTAL_HOSTS_SPACES = 2048;
    inline constexpr const uint32_t MAX_HOSTS = TOTAL_HOSTS_SPACES - 1;
    inline constexpr const uint32_t BUFFER_SEGMENTS_PER_CLIENT = 128;
    inline constexpr const uint32_t BUFFER_SEGMENT_SIZE = 512;

    inline constexpr const uint8_t HEADER_SIZE = 8;
    inline constexpr const uint8_t HOSTNAME_LENGTH = 16;
    inline constexpr const uint8_t MIN_PASSWORD_LENGTH = 8;
    inline constexpr const uint8_t MAX_PASSWORD_LENGTH = 60;
    inline constexpr const uint8_t CLIENT_KEY_LENGTH = 4;
    inline constexpr const uint8_t AUTH_PAYLOAD_LENGTH = 1;

    inline constexpr const uint32_t TOTAL_BUFFER_SEGMENTS =  TOTAL_HOSTS_SPACES * BUFFER_SEGMENTS_PER_CLIENT;
    inline constexpr const uint32_t BUFFER_SIZE = TOTAL_BUFFER_SEGMENTS * BUFFER_SEGMENT_SIZE;
    inline constexpr const uint32_t READING_BUFFER_SIZE = BUFFER_SEGMENTS_PER_CLIENT * BUFFER_SEGMENT_SIZE;
    inline constexpr const uint32_t MAX_MESSAGE_SIZE = READING_BUFFER_SIZE - BUFFER_SEGMENT_SIZE - HEADER_SIZE;
    inline constexpr const uint32_t BUFFER_READING_SIZE = BUFFER_SEGMENT_SIZE * 16;
}