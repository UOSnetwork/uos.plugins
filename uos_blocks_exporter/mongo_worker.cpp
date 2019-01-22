#include <eosio/uos_blocks_exporter/mongo_worker.hpp>
#include <bsoncxx/json.hpp>
#include <sstream>
#include <fc/variant.hpp>
#include <fc/io/json.hpp>
#include <fc/variant_object.hpp>
#include <fc/exception/exception.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/validate.hpp>

namespace uos{

    using bsoncxx::builder::basic::make_document;
    using bsoncxx::builder::basic::kvp;

    bool mongo_worker::put_by_uniq_blocknum(const std::string &_val, const std::string &_db) {
        if(!connected)
            connect();
        try {
            fc::variant block = fc::json::from_string(_val);
            block.get_object();
            if (block.get_object().contains("blocknum")) {
                try {
                    auto cursor = mongo_conn[connection_name][_db].find(
                            make_document(kvp("blocknum", block.get_object()["blocknum"].as_int64())));
                    if(cursor.begin()!=cursor.end()){
                        mongo_conn[connection_name][_db].delete_one(
                                make_document(kvp("blocknum", block.get_object()["blocknum"].as_int64())));
                    }
                }
                catch (mongocxx::exception exception) {
                    elog(exception.what());
                }
                try {
                    mongocxx::options::insert test_o;
                    test_o.bypass_document_validation(false);
                    mongo_conn[connection_name][_db].insert_one(bsoncxx::from_json(_val),test_o);
                }
                catch (mongocxx::exception exception) {
                    elog(exception.what());
                }
                return true;
            }
        }
        catch (fc::exception exception){
            std::cout<<exception.to_string()<<std::endl;
        }
        return false;
    }

    fc::mutable_variant_object mongo_worker::get_by_blocknum(const uint32_t &_blocknum, const std::string &_db) {
        if(!connected)
            connect();
        auto cursor = mongo_conn[connection_name][_db].find(make_document(kvp("blocknum",int64_t (_blocknum))));
        fc::mutable_variant_object temp;
        uint32_t i = 0;
        for (auto&& doc : cursor) {
            temp[std::to_string(i)] = fc::json::from_string(bsoncxx::to_json(doc));
        }
        return temp;
    }

    fc::mutable_variant_object mongo_worker::get_block(const uint32_t &_blocknum) {
        return get_by_blocknum(_blocknum,db_blocks);
    }
    fc::mutable_variant_object mongo_worker::get_balances(const uint32_t &_blocknum) {
        return get_by_blocknum(_blocknum,db_balances);
    }
    fc::mutable_variant_object mongo_worker::get_results(const uint32_t &_blocknum) {
        return get_by_blocknum(_blocknum,db_results);
    }


    bool mongo_worker::put_block(const std::string &__val) {
        return put_by_uniq_blocknum(__val,db_blocks);
    }
    bool mongo_worker::put_balances(const std::string &__val) {
        return put_by_uniq_blocknum(__val,db_balances);
    }
    bool mongo_worker::put_results(const std::string &__val) {
        return put_by_uniq_blocknum(__val,db_results);
    }

    void mongo_worker::recheck_indexes() {
        if (!connected)
            connect();
        if (mongo_conn[connection_name][db_blocks].indexes().list().begin() ==
            mongo_conn[connection_name][db_blocks].indexes().list().end())
            mongo_conn[connection_name][db_blocks].create_index(make_document(kvp("blocknum", 1)));

        if (mongo_conn[connection_name][db_results].indexes().list().begin() ==
            mongo_conn[connection_name][db_results].indexes().list().end())
            mongo_conn[connection_name][db_results].create_index(make_document(kvp("blocknum", 1)));

        if (mongo_conn[connection_name][db_balances].indexes().list().begin() ==
            mongo_conn[connection_name][db_balances].indexes().list().end())
            mongo_conn[connection_name][db_balances].create_index(make_document(kvp("blocknum", 1)));
    }

    std::map<uint64_t , fc::variant> mongo_worker::get_by_block_range(uint64_t _block_start,
                                                                           uint64_t _block_end,
                                                                           const std::string &_db) {
        if(!connected)
            connect();
        if(_block_start>_block_end) {
            std::swap(_block_start, _block_end);
        }
        std::map<uint64_t , fc::variant> ret;

        auto test = make_document(kvp("blocknum",
                                      make_document( kvp("$gte",static_cast<int64_t>(_block_start)),
                                                     kvp("$lte",static_cast<int64_t>(_block_end)))));
        std::cout<<bsoncxx::to_json(test.view())<<std::endl;

        auto cursor = mongo_conn[connection_name][_db].find(
                make_document(kvp("blocknum",
                                  make_document( kvp("$gte",static_cast<int64_t>(_block_start)),
                                                 kvp("$lte",static_cast<int64_t>(_block_end))))));
        for (auto&& doc : cursor) {
            auto temp = fc::json::from_string(bsoncxx::to_json(doc));
            if(!temp.get_object().contains("blocknum"))
                continue;
            ret[temp.get_object()["blocknum"].as_uint64()] = temp;
        }
        return ret;
    }

    std::map<uint64_t , fc::variant> mongo_worker::get_blocks_range(const uint64_t &_block_start,
                                                                         const uint64_t &_block_end) {
        return get_by_block_range(_block_start,_block_end,db_blocks);
    }

    std::map<uint64_t , fc::variant> mongo_worker::get_results_range(const uint64_t &_block_start,
                                                                          const uint64_t &_block_end) {
        return get_by_block_range(_block_start,_block_end,db_results);
    }

    std::map<uint64_t , fc::variant> mongo_worker::get_balances_range(const uint64_t &_block_start,
                                                                           const uint64_t &_block_end) {
        return get_by_block_range(_block_start,_block_end,db_balances);
    }
}
