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
std::string c_fail = "\033[1;31;40m";
std::string c_clear = "\033[1;0m";


class CSVWriter
{
    std::string fileName;
    std::string delimeter;
    int linesCount;
    bool is_apart, is_write;
    const std::string path{"/home/calc_log/"};

public:
    CSVWriter(std::string filename, std::string delm = ";", int lc  = 1, bool write = false ) :
            fileName(filename), delimeter(delm), linesCount(lc), is_write(write)
    {}
    CSVWriter(const CSVWriter & vs)
    {
        fileName =vs.fileName;
        delimeter = vs.delimeter;
        linesCount = vs.linesCount;
        is_write = vs.is_write;
        is_apart = vs.is_apart;
    }
    template<typename T>
    void addDatainRow(T first, T last);

    void inline settings(bool is_apart, bool is_write){this->is_apart = is_apart; this->is_write = is_write;}

    void inline setFilename(std::string filename){
        if(!is_write)
            return;
        if ( !boost::filesystem::exists( path ) )
            boost::filesystem::create_directories(path);
        this->fileName = path+filename;
    }
};


    template<typename T>
void CSVWriter::addDatainRow(T first, T last)
{
    if(is_write == false)
        return;

    std::fstream file;
    if(is_apart == false)
        try {
            file.open(fileName, std::ios::out | (linesCount ? std::ios::app : std::ios::trunc));
        }
        catch (...)
        {
            std::cout<<"could open file";
        }
    else
    {
        file.open(*first, std::ios::out | (linesCount ? std::ios::app : std::ios::trunc));
    }

    for (; first != last; )
    {
        file << *first;
        if (++first != last)
            file << delimeter;
    }
    file << "\n";
    linesCount++;

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
