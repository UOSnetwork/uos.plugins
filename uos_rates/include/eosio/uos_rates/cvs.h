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

    void inline setFilename(std::string filename){this->fileName = filename;}

};


    template<typename T>
void CSVWriter::addDatainRow(T first, T last)
{
    if(is_write == false)
        return;

    std::fstream file;
    if(apart == false)
        file.open(fileName, std::ios::out | (linesCount ? std::ios::app : std::ios::trunc));
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

#endif //EOSIO_CVS_H
