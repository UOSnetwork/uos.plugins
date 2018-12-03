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
#include <fc/io/json.hpp>
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

        uint32_t transaction_window = 100*86400*2;//100 days
        uint32_t activity_window = 30*86400*2; //30 days
        uint32_t current_calc_block;

        fc::variants source_transactions;

        vector<std::shared_ptr<singularity::relation_t>> transfer_relations;
        vector<std::shared_ptr<singularity::relation_t>> social_relations;

        vector<std::shared_ptr<singularity::relation_t>> activity_relations;

        map<string, fc::mutable_variant_object> accounts;
        map<string, fc::mutable_variant_object> content;

        explicit data_processor(uint32_t calc_block){
            current_calc_block = calc_block;
        }

        void convert_transactions_to_relations();
        vector<std::shared_ptr<singularity::relation_t>> parse_token_transaction(fc::variant trx);
        vector<std::shared_ptr<singularity::relation_t>> parse_social_transaction(fc::variant trx);

        void calculate_social_rates();



        static string to_string_10(double value);
        static string to_string_10(singularity::double_type value);

        string get_acc_string_value(string acc_name, string value_name);
        double get_acc_double_value(string acc_name, string value_name);
    };

    void data_processor::convert_transactions_to_relations() {

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

            vector<std::shared_ptr<singularity::relation_t>> relations;

            if(trx["acc"].as_string() == "eosio.token") {
                relations = parse_token_transaction(trx);
                transfer_relations.insert(transfer_relations.end(),relations.begin(), relations.end());
            }

            if(trx["acc"].as_string() == "uos.activity") {
                relations = parse_social_transaction(trx);
                social_relations.insert(social_relations.end(),relations.begin(), relations.end());
            }

            if(block_num < activity_start_block || block_num > activity_end_block)
                continue;

            activity_relations.insert(activity_relations.end(), relations.begin(), relations.end());
        }
    }

    vector<std::shared_ptr<singularity::relation_t>> data_processor::parse_token_transaction(fc::variant trx){
        auto from = trx["data"]["from"].as_string();
        auto to = trx["data"]["to"].as_string();
        auto quantity = asset::from_string(trx["data"]["quantity"].as_string()).get_amount();
        auto memo = trx["data"]["memo"].as_string();
        auto block_num = stoi(trx["block_num"].as_string());

        transaction_t transfer(quantity,from, to,0 , current_calc_block - block_num);

        vector<std::shared_ptr<singularity::relation_t>> result;
        result.push_back(std::make_shared<transaction_t>(transfer));
        return result;
    }

    vector<std::shared_ptr<singularity::relation_t>> data_processor::parse_social_transaction(fc::variant trx){

        vector<std::shared_ptr<singularity::relation_t>> result;

        //do not even parse if transaction contains \n
        auto json = fc::json::to_string(trx);
        if(json.find("\n") != std::string::npos){
            ilog(json);
            return result;
        }

        auto block_num = stoi(trx["block_num"].as_string());
        auto block_height = current_calc_block - block_num;

        if (trx["action"].as_string() == "makecontent" ) {

            auto from = trx["data"]["acc"].as_string();
            auto to = trx["data"]["content_id"].as_string();
            auto content_type_id = trx["data"]["content_type_id"].as_string();

            //do not use "create orgainzation as the content" events, code 4
            if(content_type_id == "4")
                return result;

            ownership_t ownership(from, to, block_height);
            result.push_back(std::make_shared<ownership_t>(ownership));
        }

        if (trx["action"].as_string() == "usertocont") {

            auto from = trx["data"]["acc"].as_string();
            auto to = trx["data"]["content_id"].as_string();
            auto interaction_type_id = trx["data"]["interaction_type_id"].as_string();

            if(interaction_type_id == "2") {
                upvote_t upvote(from, to, block_height);
                result.push_back(std::make_shared<upvote_t>(upvote));
            }
            if(interaction_type_id == "4") {
                downvote_t downvote(from, to, block_height);
                result.push_back(std::make_shared<downvote_t>(downvote));
            }
        }
        if (trx["action"].as_string() == "makecontorg") {

            auto from = trx["data"]["organization_id"].as_string();
            auto to = trx["data"]["content_id"].as_string();
            ownership_t ownershiporg(from, to, block_height);
            result.push_back(std::make_shared<ownership_t>(ownershiporg));

        }

        return result;
    }

    void data_processor::calculate_social_rates() {
        singularity::parameters_t params;

        auto social_calculator =
                singularity::rank_calculator_factory::create_calculator_for_social_network(params);

        social_calculator->add_block(social_relations);
        auto social_rates = social_calculator->calculate();

        for(auto item : *social_rates[singularity::ACCOUNT]){
            if(accounts.find(item.first) == accounts.end())
                accounts[item.first] = fc::mutable_variant_object();
            accounts[item.first].set("social_rate", to_string_10(item.second));
        }

        for(auto item : *social_rates[singularity::CONTENT]){
            if(content.find(item.first) == content.end())
                content[item.first] = fc::mutable_variant_object();
            content[item.first].set("social_rate", to_string_10(item.second));
        }
    }

    string data_processor::to_string_10(double value) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(10) << value;
        return ss.str();
    }

    string data_processor::to_string_10(singularity::double_type value) {
        return value.str(10,ios_base::fixed);
    }

    string data_processor::get_acc_string_value(std::string acc_name, std::string value_name) {
        if(accounts.find(acc_name) == accounts.end())
            return "0";

        if(accounts[acc_name].find(value_name) == accounts[acc_name].end())
            return "0";

        auto str_value = accounts[acc_name][value_name].as_string();
        return str_value;
    }

    double data_processor::get_acc_double_value(std::string acc_name, std::string value_name) {
        auto str_value = get_acc_string_value(acc_name, value_name);
        return stod(str_value);
    }
}

