
#pragma once
#include <string>
#include <fc/variant.hpp>
#include <mongocxx/v_noabi/mongocxx/client.hpp>
#include <mongocxx/v_noabi/mongocxx/collection.hpp>

#include <mongocxx/v_noabi/mongocxx/options/update.hpp>
//#include <mongocxx/v_noabi/mongocxx/options/replace.hpp>
#include <mongocxx/v_noabi/mongocxx/pool.hpp>
#include <mongocxx/v_noabi/mongocxx/instance.hpp>
#include <mongocxx/v_noabi/mongocxx/uri.hpp>
#include <mongocxx/v_noabi/mongocxx/exception/exception.hpp>
#include <list>
#include <atomic>

namespace uos{

    using std::string;

    struct mongo_params{
        string   mongo_uri;
        string   mongo_connection_name;
        string   mongo_db_action_traces;
        string   mongo_db_blocks;
        string   mongo_db_results;
        string   mongo_db_balances;
        string   mongo_db_contracts;
    };

    struct mongo_last_state{
        int64_t  mongo_blocknum;
        string   mongo_blockid;
        int64_t  mongo_irrblocknum;
        string   mongo_irrblockid;
    };

    class mongo_worker{

        bool                        put_by_uniq_blocknum(const string &_val,const string &_db);
        bool                        put_by_uniq_blocknum_trxid(const string &_val,const string &_db);
        fc::mutable_variant_object  get_by_blocknum(const uint32_t &_blocknum, const string &_db);
        std::map<uint64_t , fc::variant> get_by_block_range(uint64_t _block_start, uint64_t _block_end, const string &_db);

    public:
        string uri;
        string connection_name;
        string db_blocks;
        string db_results;
        string db_balances;
        string db_action_traces;
        string db_contracts;
        bool connected = false;

        mongocxx::client mongo_conn;
        mongocxx::instance inst;

        mongo_worker() = delete;
        mongo_worker& operator=(mongo_worker&) = delete;
        mongo_worker(const char* _uri, const char* _connection_name, const char* _db_blocks, const char* _db_results, const char* _db_balances, const char* _db_action_traces, const char* _db_contracts)
                :uri(_uri),
                 connection_name(_connection_name),
                 db_blocks(_db_blocks),
                 db_results(_db_results),
                 db_balances(_db_balances),
                 db_action_traces(_db_action_traces),
                 db_contracts(_db_contracts)
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
                 db_action_traces(params.mongo_db_action_traces),
                 db_contracts(params.mongo_db_contracts)
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

        mongo_last_state get_last_state();
        void set_last_state(const mongo_last_state & state);

        fc::mutable_variant_object  get_action_traces(const uint32_t &_blocknum);
        bool    put_action_traces(const string& __val);
        bool    set_irreversible_block(const uint32_t &blocknum, const string &block_id);
        bool    put_trx_contracts(const string& __val);

        std::map<uint64_t , fc::variant> get_action_traces_range(const uint64_t &_block_start, const uint64_t &_block_end);

    };

}
