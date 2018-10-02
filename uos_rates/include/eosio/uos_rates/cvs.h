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
    std::string fileName;
    std::string delimeter;
    int linesCount;
    bool apart;
    const std::string path{"/home/calc_log/"};

public:
    CSVWriter(std::string filename, std::string delm = ";", int lc  = 1) :
            fileName(filename), delimeter(delm), linesCount(lc)
    {}
    template<typename T>
    void addDatainRow(T first, T last);
    /**
     * @brief
     *
     */
    bool is_write {true};
    /**
     * @brief set flag write to one file or more file (first parametr filename)
     * @param state
     *
     */
    void inline  setApart(bool state) {apart = state;}

    void inline setFilename(std::string filename){
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
    if(apart == false)
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


void fix_symbol(std::string & str)
{
    std::string symbol = "\n";
    size_t start_pos = str.find(symbol);
    if(start_pos == std::string::npos)
    {

    }
    else
    {
        str.replace(str.find(symbol), symbol.size(), "'\\n'");
    }
}

inline uint64_t convert(std::string const& value) {
    uint64_t result = 0;
    char const* p = value.c_str();
    char const* q = p + value.size();
    while (p < q) {
        result *= 10;
        result += *(p++) - '0';
    }
    return result;
}


#endif //EOSIO_CVS_H
