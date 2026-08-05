namespace config{
    constexpr const char SERVER_PORT[] = "60000";

    constexpr int MAX_EVENTS = 256;
    constexpr int BACKLOG = 10;
    constexpr int INITIAL_HASHTABLE_SIZE = 16;
    constexpr int MAX_LENGTH_OF_INT_CHOICE = 4;

    constexpr uint32_t TOTAL_HOSTS_SPACES = 2048;
    constexpr uint32_t MAX_HOSTS = TOTAL_HOSTS_SPACES - 1;
    constexpr uint8_t BUFFER_SEGMENTS_PER_CLIENT = 128;
    constexpr uint32_t BUFFER_SEGMENT_SIZE = 512;

    constexpr uint8_t HEADER_SIZE = 8;
    constexpr uint8_t HOSTNAME_LENGTH = 16;
    constexpr uint8_t MIN_PASSWORD_LENGTH = 8;
    constexpr uint8_t MAX_PASSWORD_LENGTH = 60;
    constexpr uint8_t CLIENT_KEY_LENGTH = 4;
    constexpr uint8_t AUTH_MESSAGE_LENGTH = 9;
    constexpr uint8_t INFO_MESSAGE_LENGTH = 9;

    constexpr uint32_t TOTAL_BUFFER_SEGMENTS =  TOTAL_HOSTS_SPACES * BUFFER_SEGMENTS_PER_CLIENT;
    constexpr uint32_t BUFFER_SIZE = TOTAL_BUFFER_SEGMENTS * BUFFER_SEGMENT_SIZE;
    constexpr uint32_t READING_BUFFER_SIZE = BUFFER_SEGMENTS_PER_CLIENT * BUFFER_SEGMENT_SIZE;
    constexpr uint32_t MAX_MESSAGE_SIZE = READING_BUFFER_SIZE - BUFFER_SEGMENT_SIZE - HEADER_SIZE;
    constexpr uint32_t BUFFER_READING_SIZE = BUFFER_SEGMENT_SIZE * 16;
}