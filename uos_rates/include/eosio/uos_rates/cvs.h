#ifndef EOSIO_CVS_H
#define EOSIO_CVS_H

#include <unordered_set>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iterator>
#include <fstream>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <assert.h>
#include <stdlib.h>


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

    string inline getFilename() { return fileName;}

    void inline set_filename(string filename){
        if(!write_enabled)
            return;
        if ( !bfs::exists( path ) )
            bfs::create_directories(path);
        this->fileName = path + filename;
    }
    void inline settings(bool is_write ="false",string path = " ", string filename = " ")
    {
        write_enabled = is_write;
        if(!path.empty())
            set_path(path);
        if(!filename.empty())
            set_filename(filename);
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

string to_string_from_enum(node_type type) {
    switch (type) {
        case node_type::ACCOUNT: return "ACCOUNT";
        case node_type::CONTENT: return "CONTENT";
        case node_type::ORGANIZATION: return "ORGANIZATION";
    }
    return {};
}

    class CSVRead
    {
        char delimeter;
        string filename;
    public:
        vector<vector<string>> buffer;

        CSVRead(string fn,char delm = ';') :filename(fn),delimeter(delm)
        {}
        string const& operator[](std::size_t index) const
        {
            return m_data[index];
        }
        std::size_t size() const
        {
            return m_data.size();
        }
        void readNextRow(std::istream& str)
        {
            string line;
            std::getline(str, line);
            std::stringstream   lineStream(line);
            string cell;

            m_data.clear();
            while(std::getline(lineStream, cell, delimeter))
            {
                m_data.push_back(cell);
            }

            if (!lineStream && cell.empty())
            {
                m_data.push_back("");
            }

        }
        string inline getFilename(){ return filename;}
    private:
        vector<string> m_data;
    };
    std::istream& operator>>(std::istream& str, CSVRead& data)
    {
        data.readNextRow(str);
        return str;
    }

    void readLine(CSVRead& row, uint8_t count_columns = 7)
    {
        string filename = row.getFilename();
    if (!bfs::exists( filename))
    {
        elog("File not found " + filename);
        return;
    }

        std::ifstream file(filename);
        while(file >> row)
        {
            vector<string > crows;
            for(uint8_t i = 0; i < count_columns; i++) {
                crows.push_back(row[i]);
            }
            row.buffer.push_back(crows);
        }
    }

    static vector<vector<string>> read_csv(string filename)
    {
        vector<vector<string>> result;
        ifstream infile(filename);
        string line;
        while(getline(infile, line)){
            vector<string> res_line;
            size_t pos;
            string token;
            while((pos = line.find(";")) != string::npos){
                token = line.substr(0, pos);
                res_line.push_back(token);
                line.erase(0, pos + 1);
            }
            res_line.push_back(line);
            result.push_back(res_line);
        }

        return result;
    }

    static vector<map<string, string>> read_csv_map(string filename)
    {
        vector<map<string, string>> result;

        auto csv = read_csv(filename);

        if(csv.size() < 2)
            return result;

        auto names = csv[0];
        for(int i = 1; i < csv.size(); i++) {
            map<string, string> line;
            for(int j = 0; j < csv[i].size(); j++){
                //check column name
                if(j >= names.size())
                    elog("no name for column " + to_string(j) +
                         " line " + to_string(i) +
                         " file " + filename);
                line[names[j]] = csv[i][j];
            }
            result.push_back(line);
        }

        return result;
    };

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
    for ( auto it = v.begin(); it != v.end();  ++ it )
    {
        compressFile((*it).path().string());

    }

}

void decompressed( std::vector<bfs::directory_entry> &v)
{
    assert(v.size() == 0 && "Size can't equal null");
    for ( auto it = v.begin(); it != v.end();  ++ it )
    {
        decompressFile((*it).path().string());

    }

}

    void removeFile(std::vector<bfs::directory_entry> &v)
    {
        assert(v.size() == 0 && "Size can't equal null");

        for (auto it = v.begin(); it != v.end();  ++ it )
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
