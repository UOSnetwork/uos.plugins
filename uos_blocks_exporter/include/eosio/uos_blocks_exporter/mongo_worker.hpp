
#pragma once
#include <string>
#include <fc/variant.hpp>
#include <mongocxx/v_noabi/mongocxx/client.hpp>
#include <mongocxx/v_noabi/mongocxx/pool.hpp>
#include <mongocxx/v_noabi/mongocxx/instance.hpp>
#include <mongocxx/v_noabi/mongocxx/uri.hpp>
#include <mongocxx/v_noabi/mongocxx/exception/exception.hpp>
#include <list>

namespace uos{

    using std::string;

    struct mongo_params{
        string   mongo_uri;
        string   mongo_connection_name;
        string   mongo_db_blocks;
        string   mongo_db_results;
        string   mongo_db_balances;
        string   mongo_user;
        string   mongo_password;
    };

    class mongo_worker{

        bool                        put_by_uniq_blocknum(const string &_val,const string &_db);
        fc::mutable_variant_object  get_by_blocknum(const uint32_t &_blocknum, const string &_db);
        std::map<uint64_t , fc::variant> get_by_block_range(uint64_t _block_start, uint64_t _block_end, const string &_db);

    public:
        string uri;
        string connection_name;
        string db_blocks;
        string db_results;
        string db_balances;
        string user;
        string password;
        bool connected = false;

        mongocxx::client mongo_conn;
        mongocxx::instance inst{};

        mongo_worker(){}
        mongo_worker(const char* _uri, const char* _connection_name, const char* _db_blocks, const char* _db_results, const char* _db_balances)
                :uri(_uri),
                 connection_name(_connection_name),
                 db_blocks(_db_blocks),
                 db_results(_db_results),
                 db_balances(_db_balances)
                 //user
                 //password
        {
            mongo_conn = mongocxx::client{mongocxx::uri(uri)};
            connected = true;
            recheck_indexes();
        }

        mongo_worker(mongo_params params)
                :uri(params.mongo_uri),
                 connection_name(params.mongo_connection_name),
                 db_blocks(params.mongo_db_blocks),
                 db_results(params.mongo_db_results),
                 db_balances(params.mongo_db_balances),
                 user(params.mongo_user),
                 password(params.mongo_password)
        {
            mongo_conn = mongocxx::client{mongocxx::uri(uri)};
            connected = true;
            recheck_indexes();
        }

        void connect(){
            mongo_conn = mongocxx::client{mongocxx::uri(uri)};
            connected = true;
            recheck_indexes();
        }

        void recheck_indexes();

        fc::mutable_variant_object  get_block(const uint32_t &_blocknum);
        bool    put_block(const string& __val);

        fc::mutable_variant_object  get_balances(const uint32_t& _blocknum);
        bool    put_balances(const string& __val);

        fc::mutable_variant_object  get_results(const uint32_t& _blocknum);
        bool    put_results(const string& __val);

        std::map<uint64_t , fc::variant> get_blocks_range(const uint64_t &_block_start, const uint64_t &_block_end);
        std::map<uint64_t , fc::variant> get_results_range(const uint64_t &_block_start, const uint64_t &_block_end);
        std::map<uint64_t , fc::variant> get_balances_range(const uint64_t &_block_start, const uint64_t &_block_end);

    };

}
