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
    using bsoncxx::builder::basic::make_array;
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

    fc::mutable_variant_object mongo_worker::get_action_traces(const uint32_t &_blocknum) {
        return get_by_blocknum(_blocknum,db_action_traces);
    }

    bool mongo_worker::put_action_traces(const std::string &__val) {
        return put_by_uniq_blocknum(__val,db_action_traces);
    }

    void mongo_worker::recheck_indexes() {
        if (!connected)
            connect();
        if (mongo_conn[connection_name][db_action_traces].indexes().list().begin() ==
            mongo_conn[connection_name][db_action_traces].indexes().list().end())
            mongo_conn[connection_name][db_action_traces].create_index(make_document(kvp("blocknum", 1)));
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

    bool mongo_worker::set_irreversible_block(const uint32_t &blocknum, const std::string &block_id) {

        if(!connected)
            connect();
        try{
            mongo_conn[connection_name][db_action_traces].delete_many(
                    make_document(kvp("$and",
                                      make_array(make_document(kvp("blocknum", static_cast<int64_t>(blocknum))),
                                                 make_document(kvp("blockid",
                                                                   make_document(kvp("$ne",block_id))
                                                 ))
                                      ))
                    )
            );
            return bool(
                    mongo_conn[connection_name][db_action_traces].find_one_and_update(
                    make_document(kvp("$and",
                                      make_array(make_document(kvp("blocknum", static_cast<int64_t>(blocknum))),
                                                 make_document(kvp("blockid",block_id))))),
                    make_document(kvp("$set", make_document(kvp("irreversible",true))))
                    )
            );
        }
        catch(mongocxx::exception &ex){
            elog(ex.what());
            return false;
        }
        catch (...){
//            elog("error");
        }
        return true;
    }

}
