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

    class merkle_tree {
    public:
        std::map < account_name, pair < string, size_t > > sum;
        vector < leaf >                                    leafs_list;
        vector < vector< node > >                          nodes_list;

        bool    counted;

        merkle_tree( ) : counted( false ) { }

        size_t set_accounts( vector < pair < account_name, double > > & );
        size_t set_accounts( vector < pair < account_name, string > > & );

        bool   count_tree( );

        string get_proof_for_contract(account_name);

        vector < pair < node, node > > get_proof( account_name );

    };

    size_t merkle_tree::set_accounts( vector < pair < account_name, double > > &accounts ) {

        if ( !accounts.size() )
            return 0;

        for ( auto account_pair : accounts ) {
            sum[ account_pair.first ].first = std::to_string( account_pair.second );
        }
        counted = false;
        return sum.size();

    }

    size_t merkle_tree::set_accounts( vector < pair < account_name, string > > &accounts ) {

        if ( !accounts.size() )
            return 0;

        for ( auto account_pair : accounts ) {
            sum[ account_pair.first ].first = account_pair.second ;
        }
        counted = false;
        return sum.size();

    }

    bool merkle_tree::count_tree() {

        if ( !sum.size() )
            return false;

        size_t pos = 0;
        for ( auto &item : sum ) {
            string tstr = item.first.to_string() + item.second.first;
            leafs_list.emplace_back( leaf { tstr, fc::sha256::hash( tstr ) } );
            pos++;
        }

        while ( !poweroftwo( leafs_list.size() ) ) {
            leafs_list.emplace_back( leaf { string( "none" ) , fc::sha256::hash( string( "none" ) ) } );
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

    vector < pair < node, node > > merkle_tree::get_proof( account_name account ) {

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

    string merkle_tree::get_proof_for_contract( account_name account ) {
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
}

