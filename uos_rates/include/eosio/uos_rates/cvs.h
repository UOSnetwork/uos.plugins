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

namespace eosio {

using node_type = singularity::node_type;
namespace bfs = boost::filesystem;
namespace bio = boost::iostreams;
using string = std::string;

class CSVWriter
{
    string path;
    string fileName;
    string delimeter;
    bool write_enabled;

public:
    CSVWriter(string directory = string(getenv("HOME")) + "/uos/calc_log/", string delm = ";", bool write = false ) :
            path(directory), delimeter(delm), write_enabled(write)
    {}
    template<typename T>
    void addDatainRow(T first, T last);

    void inline set_write_enabled(bool write){this->write_enabled = write;}

    void inline set_path(string directory){
        this->path = directory + "/";
    }
    string inline getPath (){return path;}

    string inline getFilename() { return fileName;};

    void inline set_filename(string filename){
        if(!write_enabled)
            return;
        if ( !boost::filesystem::exists( path ) )
            boost::filesystem::create_directories(path);
        this->fileName = path + filename;
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
            elog("could not open the file " + fileName);
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

std::string to_string_from_enum(node_type type) {
    switch (type) {
        case node_type::ACCOUNT: return "ACCOUNT";
        case node_type::CONTENT: return "CONTENT";
        case node_type::ORGANIZATION: return "ORGANIZATION";
    }
    return {};
}
bool compressFile(string filename)
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

bool decompressFile(string filename)
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

std::vector<bfs::directory_entry> listFileinDir (string path = string(getenv("HOME")) + "/uos/calc_log/")
{
    std::vector<bfs::directory_entry> v;
    if(bfs::is_directory(path))
    {
        copy(bfs::directory_iterator(path), bfs::directory_iterator(), back_inserter(v));
    }

    return v;
}

void compressed(std::vector<bfs::directory_entry> &v)
{
    assert(v.size() == 0 && "Size can't equal null");
    for ( std::vector<bfs::directory_entry>::const_iterator it = v.begin(); it != v.end();  ++ it )
    {
        compressFile((*it).path().string());

    }

}

void decompressed( std::vector<bfs::directory_entry> &v)
{
    assert(v.size() == 0 && "Size can't equal null");
    for ( std::vector<bfs::directory_entry>::const_iterator it = v.begin(); it != v.end();  ++ it )
    {
        decompressFile((*it).path().string());

    }

}

void removeFile(std::vector<bfs::directory_entry> &v)
{
    assert(v.size() == 0 && "Size can't equal null");

    for ( std::vector<bfs::directory_entry>::const_iterator it = v.begin(); it != v.end();  ++ it )
    {
        try
        {
            bfs::remove((*it).path().string());
        }
        catch(const std::exception &ex)
        {
            ex.what();
        }
    }
}
}
#endif //EOSIO_CVS_H
