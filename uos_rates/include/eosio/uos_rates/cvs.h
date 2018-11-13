#ifndef EOSIO_CVS_H
#define EOSIO_CVS_H

#include <unordered_set>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iterator>
#include <fstream>

#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/filesystem.hpp>
//#include "/../../libraries/chain/transaction.cpp"

/**
 * @brief
 *  class for logger(helper for write result )
 *
 */
std::string c_fail = "\033[1;31;40m";
std::string c_clear = "\033[1;0m";

using node_type = singularity::node_type;
namespace bfs = boost::filesystem;
namespace bio = boost::iostreams;

class CSVWriter
{
    using string = std::string;

    string fileName;
    string delimeter;
    int linesCount;
    bool is_apart, is_write;
    const string path{std::string(getenv("HOME")) + "/uos/calc_log/"};

public:
    CSVWriter(string filename, string delm = ";", int lc  = 1, bool write = false ) :
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

    void inline setFilename(string filename){
        if(!is_write)
            return;
        if ( !bfs::exists( path ) )
            bfs::create_directories(path);
        this->fileName = path + filename;
    }

    string inline getPath (){return path;}

    string inline getFilename() { return fileName;};

    void inline settings(bool is_apart, bool is_write, string filename = "" )
    {
        this->is_apart = is_apart;
        this->is_write = is_write;
        if(!filename.empty())
        setFilename(filename);
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

std::string to_string_from_enum(node_type type) {
    switch (type) {
        case node_type::ACCOUNT: return "ACCOUNT";
        case node_type::CONTENT: return "CONTENT";
        case node_type::ORGANIZATION: return "ORGANIZATION";
    }
    return {};
}

bool compressFile(std::string filename)
{
    bfs::path pathObj(filename);
    if (pathObj.has_extension()) {
        if (pathObj.extension().string() == ".csv" /*&& !bfs::exists(bfs::change_extension(pathObj, "gzip").string())*/) {
            if(pathObj.extension().string() == ".gzip")
                return false;

            std::ifstream inStream(filename, std::ios_base::in| std::ios_base::binary);
            std::ofstream outStream(bfs::change_extension(pathObj, "gzip").string(), std::ios_base::out | std::ios_base::binary);
            bio::filtering_streambuf<bio::input> in;
            in.push(bio::gzip_compressor());
            in.push(inStream);
            bio::copy(in, outStream);
            inStream.close();
            outStream.close();
            return true;
        }
    }
}

bool decompressFile(std::string filename)
{
    bfs::path pathObj(filename);
    if (pathObj.has_extension()) {
        if (pathObj.extension().string() == ".gzip" /*&& !bfs::exists(bfs::change_extension(pathObj, "gzip").string())*/) {
            if(pathObj.extension().string() == ".csv")
                return false;

            std::ifstream inStream(filename, std::ios_base::in| std::ios_base::binary);
            std::ofstream outStream(bfs::change_extension(pathObj, "csv").string(), std::ios_base::out | std::ios_base::binary);
            bio::filtering_streambuf<bio::input> in;
            in.push(bio::gzip_decompressor());
            in.push(inStream);
            bio::copy(in, outStream);
            inStream.close();
            outStream.close();
            return true;
        }
    }
}

std::vector<bfs::directory_entry> listFileinDir (std::string path = std::string(getenv("HOME")) + "/uos/calc_log/")
{
    std::vector<bfs::directory_entry> v;
    v.clear();
    if(bfs::is_directory(path))
    {
        copy(bfs::directory_iterator(path), bfs::directory_iterator(), back_inserter(v));
    }

    return v;
}

void compressed(std::vector<bfs::directory_entry> &v)
{
    for ( std::vector<bfs::directory_entry>::const_iterator it = v.begin(); it != v.end();  ++ it )
    {
        compressFile((*it).path().string());

    }

}

void decompressed( std::vector<bfs::directory_entry> &v)
{
    for ( std::vector<bfs::directory_entry>::const_iterator it = v.begin(); it != v.end();  ++ it )
    {
        decompressFile((*it).path().string());

    }

}


#endif //EOSIO_CVS_H
