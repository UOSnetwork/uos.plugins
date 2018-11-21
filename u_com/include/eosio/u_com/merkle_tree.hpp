#pragma once

#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <fc/crypto/sha256.hpp>
#include <eosio/chain/types.hpp>

namespace uos {
    inline bool poweroftwo(size_t v) {
        return (v && !(v & (v - 1)));
    }

    using namespace std;
    using namespace eosio::chain;

    typedef pair<string, fc::sha256> leaf;
    typedef fc::sha256 node;

    template <class Type>
    class merkle_tree {
    public:
        std::map < Type, pair < string, size_t > > sum;
        vector < leaf >                                    leafs_list;
        vector < vector< node > >                          nodes_list;

        bool    counted;

        merkle_tree( ) : counted( false ) { }


        size_t set_accounts( vector < pair < Type, double > > & );
        size_t set_accounts( vector < pair < Type, string > > & );

        bool   count_tree( );

        string get_proof_for_contract(Type);

        vector < pair < node, node > > get_proof( Type );

        friend std::ostream& operator<<(std::ostream&, const merkle_tree<Type>&);
        friend std::ostream& operator>>(std::istream&, const merkle_tree<Type>&);

    };

    template <class Type>
    size_t merkle_tree<Type>::set_accounts( vector < pair < Type, double > > &accounts ) {

        if ( !accounts.size() )
            return 0;

        for ( auto account_pair : accounts ) {
            sum[ account_pair.first ].first = std::to_string( account_pair.second );
        }
        counted = false;
        return sum.size();

    }

    template <class Type>
    size_t merkle_tree<Type>::set_accounts( vector < pair < Type, string > > &accounts ) {

        if ( !accounts.size() )
            return 0;

        for ( auto account_pair : accounts ) {
            sum[ account_pair.first ].first = account_pair.second ;
        }
        counted = false;
        return sum.size();

    }


    template <class to_str>
    string uos_string(to_str item){
        return string(item);
    }

    template <class Type>
    bool merkle_tree<Type>::count_tree() {

        if ( !sum.size() )
            return false;

        size_t pos = 0;
        for ( auto &item : sum ) {
            string tstr = uos_string<Type>(item.first) + item.second.first;
            leafs_list.emplace_back( leaf { tstr, fc::sha256::hash( tstr ) } );
            item.second.second = pos;
            pos++;
        }

        while ( !poweroftwo( leafs_list.size() ) ) {
            leafs_list.emplace_back( leaf { string( "none" ) , fc::sha256::hash( string( "" ) ) } );
        }

        auto layer_size = leafs_list.size() + 1;
        vector < node > temp_node;
        for ( auto j = 0 ; j < ( layer_size / 2 ) ; j++ ) {
            node temp[2] = { leafs_list[ j * 2 ].second, leafs_list[ j * 2 + 1 ].second };
            temp_node.emplace_back( fc::sha256::hash ( ( char * ) temp, sizeof( temp ) ) );
        }
        nodes_list.emplace_back( move( temp_node ) );

        layer_size = nodes_list[ 0 ].size() + 1;
        for ( auto i = 1 ;; i++ ) {
            vector < node > t_node;
            for ( auto j = 0 ; j < ( layer_size / 2 ) ; j++ ) {
                node temp[2] = { nodes_list[ i - 1 ][ j * 2 ] , nodes_list[ i - 1 ][ j * 2 + 1 ] };
                t_node.emplace_back( fc::sha256::hash( ( char * ) temp , sizeof( temp ) ) );
            }
            nodes_list.emplace_back( t_node );
            t_node.clear();
            if ( nodes_list[ i ].size() == 1 )
                break;
            layer_size = nodes_list[ i ].size() + 1;
        }
        counted = true;
        return true;
    }

    template <class Type>
    vector < pair < node, node > > merkle_tree<Type>::get_proof( Type account ) {

        vector< pair < node, node > > result;

        if ( !( counted && sum.find( account ) != sum.end() ) )
            return result;

        if ( sum[ account ].second > leafs_list.size() )
            return result;

        auto position = sum[ account ].second;

        node right, left;

        if ( position % 2 ) {
            right = leafs_list[ position ].second;
            left  = leafs_list[ position - 1 ].second;
        } else {
            left  = leafs_list[ position ].second;
            right = leafs_list[ position + 1 ].second;
        }
        result.emplace_back( pair < node, node > { left, right } );

        position = position / 2;
        size_t r = 0;
        while ( r < nodes_list.size() - 1 ) {
            if ( position % 2 ) {
                right = nodes_list[ r ][ position ];
                left  = nodes_list[ r ][ position - 1 ];
            } else {
                left  = nodes_list[ r ][ position ];
                right = nodes_list[ r ][ position + 1 ];
            }
            result.emplace_back( pair < node, node > { left, right } );
            position = position / 2;
            r++;
        }
        return result;
    }

    template <class Type>
    string merkle_tree<Type>::get_proof_for_contract( Type account ) {
        auto result = get_proof( account );

        stringstream ss;
        ss << "[";
        bool mark = false;

        for ( auto &item : result ) {
            if ( mark )
                ss << ",";
            ss << "[\"" << string( item.first ) << "\",\"" << string( item.second ) << "\"]";
            mark = true;
        }
        ss << "]";

        return ss.str();
    }


    ///꧁꧂ 𐂅 happy merkle tree friends ꧁꧂

    class my_ctype : public
                     std::ctype<char>
    {
        mask my_table[table_size];
    public:
        my_ctype(size_t refs = 0)
                : std::ctype<char>(&my_table[0], false, refs)
        {
            std::copy_n(classic_table(), table_size, my_table);
            my_table[(size_t)';'] = (mask)space;
            my_table[(size_t)' '] ^= (mask)space;
        }
    };

    template <class Type>
    ostream& operator<<(ostream& out, merkle_tree<Type>& mtree){
        for(auto &item: mtree.sum)
            out<<item.first<<';'<<item.second.first<<endl;
        return out;
    }

    template <>
    ostream& operator<<(ostream& out, merkle_tree<account_name>& mtree){
        for(auto &item: mtree.sum)
            out<<(uint64_t)item.first<<';'<<item.second.first<<endl;
        return out;
    }


    template <class Type>
    istream& operator>>(istream& in, pair <Type, string>& item){
        std::locale x(std::locale::classic(), new my_ctype);
        in.imbue(x);
        in>>item.first>>item.second;
        return in;
    }

    template <>
    istream& operator>>(istream& in, pair <account_name , string>& item){
        std::locale x(std::locale::classic(), new my_ctype);
        in.imbue(x);
        uint64_t first;
        in>>first>>item.second;
        item.first = first;
        return in;
    }


    template <class Type>
    istream& operator>>(istream& in, merkle_tree<Type>& mtree){
        pair<Type, string> item;
        while (!in.eof()){
            item.second="";
            in>>item;
            if(item.second=="")
                break;
            mtree.sum[item.first].first=item.second;
        }
        return in;
    }


    ///꧁꧂ 𐂅 merkle tree tests ꧁꧂

    void merkle_test(){
        cout<<"꧁꧂     Merkle tree test "<<endl<<endl;

        merkle_tree<account_name> tree;
        vector< pair<account_name,double>> temp;
        for(size_t i=0;i<200;i++){
            temp.emplace_back(make_pair(i+10,(double)rand() / RAND_MAX));
        }
        tree.set_accounts(temp);
        tree.count_tree();
        string contr1 = tree.get_proof_for_contract(150);
        cout<<contr1;
        cout<<endl;
        stringstream ss;
        ss<<tree;
        cout<<endl<<endl<<">>"<<ss.str()<<endl<<endl;
        merkle_tree<account_name> tree2;
        ss>>tree2;
        tree2.count_tree();
        string contr2 = tree2.get_proof_for_contract(150);
        cout<<tree2;
        cout<<contr2;
        cout<<endl;
        if(contr1==contr2)
            elog("Test OK");
        else
            elog("Test Failed");

    }

    void merkle_test_for_strings(){
        cout<<"꧁꧂     Merkle tree test 'for strings' "<<endl<<endl;

        merkle_tree<string> tree;
        vector< pair<string,double>> temp;

        temp.emplace_back(make_pair("Samuel",(double)rand() / RAND_MAX));
        temp.emplace_back(make_pair("Barbel",(double)rand() / RAND_MAX));
        temp.emplace_back(make_pair("Adagio",(double)rand() / RAND_MAX));
        temp.emplace_back(make_pair("For",(double)rand() / RAND_MAX));
        temp.emplace_back(make_pair("Strings",(double)rand() / RAND_MAX));
        temp.emplace_back(make_pair("B♭ minor",(double)rand() / RAND_MAX));

        tree.set_accounts(temp);
        tree.count_tree();
        string contr1 = tree.get_proof_for_contract("Strings");
        cout<<contr1;
        cout<<endl;
        stringstream ss;
        ss<<tree;
        cout<<endl<<endl<<">>"<<ss.str()<<endl<<endl;
        merkle_tree<string> tree2;
        ss>>tree2;
        tree2.count_tree();
        string contr2 = tree2.get_proof_for_contract("Strings");
        cout<<tree2;
        cout<<contr2;
        cout<<endl;
        if(contr1==contr2)
            elog("Test OK");
        else
            elog("Test Failed");

    }

    void merkle_test_for_strings_for_strings(){
        cout<<"꧁꧂     Merkle tree test 'for strings' for strings "<<endl<<endl;

        merkle_tree<string> tree;
        vector< pair<string,string>> temp;

        temp.emplace_back(make_pair("Samuel","1"));
        temp.emplace_back(make_pair("Barbel","2"));
        temp.emplace_back(make_pair("Adagio","3"));
        temp.emplace_back(make_pair("For","4"));
        temp.emplace_back(make_pair("Strings","5"));
        temp.emplace_back(make_pair("B♭ minor","6"));

        tree.set_accounts(temp);
        tree.count_tree();
        string contr1 = tree.get_proof_for_contract("Strings");
        cout<<contr1;
        cout<<endl;
        stringstream ss;
        ss<<tree;
        cout<<endl<<endl<<">>"<<ss.str()<<endl<<endl;
        merkle_tree<string> tree2;
        ss>>tree2;
        tree2.count_tree();
        string contr2 = tree2.get_proof_for_contract("Strings");
        cout<<tree2;
        cout<<contr2;
        cout<<endl;
        if(contr1==contr2)
            elog("Test OK");
        else
            elog("Test Failed");

    }
};

