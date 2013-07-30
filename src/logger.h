#ifndef wotreplay_logger_h
#define wotreplay_logger_h

#include <iosfwd>
#include <string>


namespace wotreplay {
enum log_level {
    error,
    warning,
    info,
    debug
};

    /**
     * basic logger class
     */
    class logger {
    public:
        /**
         * Create new instance of logger, logger will write to the given output stream
         * @param os target outputstream
         */
        logger(std::ostream& os);
        /**
         * Define the minimum log level
         * @param level new minimum log level
         */
        void set_log_level(log_level level);
        /**
         * Write a message to the log file
         * @param level define a level for the message
         * @param message the message to log
         */
        void write(log_level level, const std::string& message);
    private:
        std::ostream &os;
        log_level level = log_level::warning;
    };

    extern logger log;
}

#endif /* defined(wotreplay_logger_h) */
