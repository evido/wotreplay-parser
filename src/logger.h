#ifndef wotreplay_logger_h
#define wotreplay_logger_h

#include <boost/format.hpp>
#include <iosfwd>
#include <string>

namespace wotreplay {
    enum log_level_t {
        none,
        error,
        warning,
        info,
        debug
    };

    /**
     * basic logger class
     */
    class logger_t {
    public:
        /**
         * Create new instance of logger, logger will write to the given output stream
         * @param os target outputstream
         */
        logger_t(std::ostream& os);
        /**
         * Define the minimum log level
         * @param level new minimum log level
         */
        void set_log_level(log_level_t level);
        /**
         * get current log level
         * @return the current log level
         */
        log_level_t get_log_level() const;

        /**
         * Write formatted string to log
         * @param level log level
         * @param format formatting string
         * @param args formatting string paramaters
         */
        template<typename... Args>
        void writef(log_level_t level, const std::string &format, const Args&... args) {
            if (this->level > log_level_t::none && level <= this->level) {
                writef(format, args...);
            }
        }

        /**
         * Write formatted string to log
         * @param _format formatting string
         * @param args formatting string paramaters
         */
        template<typename... Args>
        void writef(const std::string &_format, const Args&... args) {
            if (this->level > log_level_t::none) {
                boost::format format(_format);
                writef(format, args...);
            }
        }

        /**
         * Write formatted string to log
         * @param level log level
         * @param value formatting string
         */
        void write(log_level_t level, const std::string &value);

        /**
         * Write formatted string to log
         * @param value formatting string
         */
        void write(const std::string &value);
    private:
        void writef(boost::format &format);

        template<typename T, typename... Args>
        void writef(boost::format &format, const T &val, const Args&... args) {
            writef(format % val, args...);
        }
        
        std::ostream &os;
        log_level_t level = log_level_t::warning;
    };

    extern logger_t logger;
}

#endif /* defined(wotreplay_logger_h) */
