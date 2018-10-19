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
        if(!is_write)
            return;
        if ( !boost::filesystem::exists( path ) )
            boost::filesystem::create_directories(path);
        this->fileName = path+filename;
    }
    
    //Algoritm Knut-Morris_Pratt
    void calc_z(std::string &s, std::vector<int> &z) {
        int len = s.size();
        z.resize(len);

        int l = 0, r = 0;
        for (int i = 1; i < len; ++i)
         if (z[i - l] + i <= r)
            z[i] = z[i - l];
         else {
            l = i;
            if (i > r) r = i;
            for (z[i] = r - i; r < len; ++r, ++z[i])
                if (s[r] != s[z[i]])
                    break;
            --r;
        }
    }

void specialSort(int* data, int const len)
{
  int const lenD = len;
  int pivot = 0;
  int ind = lenD/2;
  int i,j = 0,k = 0;
  if(lenD>1){
    int* L = new int[lenD];
    int* R = new int[lenD];
    pivot = data[ind];
    for(i=0;i<lenD;i++){
      if(i!=ind){
        if(data[i]<pivot){
          L[j] = data[i];
          j++;
        }
        else{
          R[k] = data[i];
          k++;
        }
      }
    }
    specialSort(L,j);
    specialSort(R,k);
    for(int cnt=0;cnt<lenD;cnt++){
      if(cnt<j){
        data[cnt] = L[cnt];;
      }
      else if(cnt==j){
        data[cnt] = pivot;
      }
      else{
        data[cnt] = R[cnt-(j+1)];
      }
    }
  }
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
