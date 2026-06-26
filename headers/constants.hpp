namespace Constants{
    inline constexpr int HOST_TOTAL = 10;
    inline constexpr const char* SERVER_PORT = "60000";
    inline constexpr int BACKLOG = 10;

    inline constexpr const int SUCCESS_RETURN = 0;
    inline constexpr const int NOTHING_TO_READ_RETURN = 1;
    inline constexpr const int CLOSED_RETURN = 2;
}

namespace Commands{
    inline constexpr const char* ACK = "/received";
    inline constexpr const int ACK_LENGTH = 10;
}