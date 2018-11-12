#ifndef EOSIO_CVS_H
#define EOSIO_CVS_H

#include <unordered_set>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iterator>
#include <fstream>

/**
 * @brief
 *  class for logger(helper for write result )
 *
 */

class CSVWriter
{
    std::string path;
    std::string fileName;
    std::string delimeter;
    bool write_enabled;

public:
    CSVWriter(std::string directory = "/home/calc_log/", std::string delm = ";", bool write = false ) :
            path(directory), delimeter(delm), write_enabled(write)
    {}
    template<typename T>
    void addDatainRow(T first, T last);

    void inline set_write_enabled(bool write){this->write_enabled = write;}

    void inline set_path(std::string directory){this->path = directory;}

    void inline set_filename(std::string filename){
        if(!write_enabled)
            return;
        if ( !boost::filesystem::exists( path ) )
            boost::filesystem::create_directories(path);
        this->fileName = path+filename;
    }
};


    template<typename T>
void CSVWriter::addDatainRow(T first, T last)
{
    if(!write_enabled)
        return;

    std::fstream file;

        try {
            file.open(fileName, std::ios::out | std::ios::app);
        }
        catch (...)
        {
            std::cout<<"could open file";
        }

    for (; first != last; )
    {
        file << *first;
        if (++first != last)
            file << delimeter;
    }
    file << "\n";

    file.close();
}

std::string to_string_from_enum(singularity::node_type e) {
    switch (e) {
        case singularity::node_type::ACCOUNT: return "ACCOUNT";
        case singularity::node_type::CONTENT: return "CONTENT";
        case singularity::node_type::ORGANIZATION: return "ORGANIZATION";
    }
    return {};
}


#endif //EOSIO_CVS_H
