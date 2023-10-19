#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <optional>
#include <memory>
#include <cstdlib>
#include <fstream>
#include <chrono>
#include <iomanip>
#include "clientserver/udaStructs.h"
#include <server/getServerEnvironment.h>
/*
 *
 * NOTES:
 *
 * using char instead of byte to avoid extra casting from/to datablock
 * 
*/

namespace ram_cache
{
    enum class LogLevel { DEBUG, INFO, WARNING, ERROR };

    /**
     * @brief Temporary logging function for JSON_mapping_plugin, outputs
     * to UDA_HOME/etc/
     *
     * @param log_level The LogLevel (INFO, WARNING, ERROR, DEBUG)
     * @param log_msg The message to be logged
     * @return
     */
    inline int log(LogLevel log_level, std::string_view log_msg) {

        const ENVIRONMENT* environment = getServerEnvironment();

        std::string const log_file_name = std::string{static_cast<const char*>(environment->logdir)} + "/ramcache.log";
        std::ofstream log_file;
        log_file.open(log_file_name, std::ios_base::out | std::ios_base::app);
        std::time_t const time_now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        const auto timestamp = std::put_time(std::gmtime(&time_now), "%Y-%m-%d:%H:%M:%S"); // NOLINT(concurrency-mt-unsafe)
        if (!log_file) {
            return 1;
        }

        switch (log_level) {
            case LogLevel::DEBUG:
                log_file << timestamp << ":DEBUG - ";
                break;
            case LogLevel::INFO:
                log_file << timestamp << ":INFO - ";
                break;
            case LogLevel::WARNING:
                log_file << timestamp << ":WARNING - ";
                break;
            case LogLevel::ERROR:
                log_file << timestamp << ":ERROR - ";
                break;
            default:
                log_file << "LOG_LEVEL NOT DEFINED";
        }
        log_file << log_msg << "\n";
        log_file.close();

        return 0;
    }

   void log_datablock_status(DATA_BLOCK* data_block, std::string message); 

    struct DataEntry
    {
        std::vector<char> data; 
        std::vector<char> error_high; 
        std::vector<char> error_low; 
        std::vector<std::vector<char>> dims; 
        int order = -1; 
        int data_type;
        std::vector<int> dim_types;
        int error_type;
    };

    class RamCache
    {
        public:
        inline RamCache()
        {
            _values.reserve(_max_items);
        }

        explicit inline RamCache(uint32_t max_items) : _max_items(max_items) 
        {
            _values.reserve(_max_items);
        }
    
        inline void add(std::string key, std::shared_ptr<DataEntry> value)
        {
            if (_values.size() < _max_items)
            {
                _keys.emplace_back(key);
                _values.emplace_back(std::move(value));
            }
            else 
            {
                _keys[_current_position] = key;
                _values[_current_position++] = std::move(value);
                _current_position %= _max_items;
            }
            log(LogLevel::INFO, "entry added to cache: \"" + key + "\". cache size is now " + std::to_string(_values.size()) + " / " +std::to_string(_max_items));
            log(LogLevel::INFO, "current position is now: " + std::to_string(_current_position));
        }


        std::shared_ptr<DataEntry> make_data_entry(DATA_BLOCK* data_block);
        std::optional<DATA_BLOCK*> copy_from_cache(std::string key, DATA_BLOCK* data_block);

        private:
        uint32_t _max_items = 100;
        uint32_t _current_position = 0;
        std::vector<std::string> _keys;
        std::vector<std::shared_ptr<DataEntry>> _values;

    };
    
}
