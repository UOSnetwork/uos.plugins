#pragma once

#include <eosio/uos_rates/uos_rates.hpp>
//#include <eosio/uos_rates/transaction_queqe.hpp>
//#include <eosio/uos_rates/merkle_tree.hpp>
//#include <eosio/chain/asset.hpp>
//#include <eosio/chain/exceptions.hpp>
//#include <eosio/chain_api_plugin/chain_api_plugin.hpp>
//#include <eosio/http_plugin/http_plugin.hpp>
#include <../../../libraries/singularity/include/singularity.hpp>
#include <../../../libraries/singularity/generated/git_version.hpp>
//
//
//#include <fc/io/json.hpp>
//#include <fc/crypto/sha256.hpp>
//#include <eosio/uos_rates/cvs.h>
//#include <algorithm>
//#include <boost/program_options.hpp>
//#include "../../../../../../libraries/fc/include/fc/variant.hpp"


namespace uos {
    using namespace std;
    using namespace eosio;
    class data_processor {

    public:

        fc::variants source_transactions;
        uint32_t current_calc_block;
        uint32_t transaction_window;
        uint32_t activity_window;

        vector<std::shared_ptr<singularity::relation_t>> transfer_relations;
        vector<std::shared_ptr<singularity::relation_t>> social_relations;

        vector<std::shared_ptr<singularity::relation_t>> activity_relations;


        void prepare_relations();

        std::shared_ptr<singularity::relation_t> parse_token_transaction(fc::variant trx);

//        singularity::relation_t parse_social_transaction(fc::variant trx)
//        {
//
//        }
    };

    void data_processor::prepare_relations() {

        int32_t end_block = current_calc_block;
        int32_t start_block = end_block - transaction_window + 1;
        if (start_block < 1)
            start_block = 1;

        int32_t activity_end_block = end_block;
        int32_t activity_start_block = end_block - activity_window + 1;
        if (activity_start_block < 1)
            activity_start_block = 1;

        for(auto trx : source_transactions){

            auto block_num = stoi(trx["block_num"].as_string());

            if(block_num < start_block || block_num > end_block)
                continue;

            std::shared_ptr<singularity::relation_t> relation = std::shared_ptr<singularity::relation_t>(nullptr);
            if(trx["account"].as_string() == "eosio.token") {
                relation = parse_token_transaction(trx);
                transfer_relations.push_back(relation);
            }
//                if(trx["account"].as_string() == "uos.activity") {
//                    *relation = parse_social_transaction(trx);
//                    social_relations.push_back(*relation);
//                }

            if(block_num < activity_start_block || block_num > activity_end_block)
                continue;

            activity_relations.push_back(relation);
        }
    }

    std::shared_ptr<singularity::relation_t> data_processor::parse_token_transaction(fc::variant trx){
        auto from = trx["data"]["from"].as_string();
        auto to = trx["data"]["to"].as_string();
        auto quantity = asset::from_string(trx["data"]["quantity"].as_string()).get_amount();
        auto memo = trx["data"]["memo"].as_string();
        auto block_num = stoi(trx["block_num"].as_string());

        transaction_t transfer(quantity,from, to,0 , current_calc_block - block_num);
        return std::make_shared<transaction_t>(transfer);
    }
}

