#pragma once
#include <appbase/application.hpp>
#include <eosio/chain_plugin/chain_plugin.hpp>
#include <../../../libraries/singularity/include/singularity.hpp>

namespace eosio {

    using namespace appbase;

    class uos_rates : public appbase::plugin<uos_rates> {
    public:
        uos_rates();
        virtual ~uos_rates();

        APPBASE_PLUGIN_REQUIRES()
        virtual void set_program_options(options_description&, options_description& cfg) override;

        void plugin_initialize(const variables_map& options);
        void plugin_startup();
        void plugin_shutdown();

        void irreversible_block_catcher(const chain::block_state_ptr& bst);
        void accepted_block_catcher(const chain::block_state_ptr& ast);

    private:
        std::unique_ptr<class uos_rates_impl> my;
    };

    class result_item
    {
    public:
        string name;
        string type;

        string soc_rate = "0";
        string soc_rate_scaled = "0";
        string trans_rate = "0";
        string trans_rate_scaled = "0";
        string staked_balance = "0";
        string stake_rate = "0";
        string stake_rate_scaled = "0";
        string importance = "0";
        string importance_scaled = "0";
        string prev_cumulative_emission = "0";
        string current_emission = "0";
        string current_cumulative_emission = "0";
    };

    class result_set
    {
    public:
        uint64_t block_num;

        string max_activity = "0";
        string current_activity = "0";
        string target_emission = "0";
        string emission_limit = "0";

        map<string, result_item> res_map;
        string result_hash;

        result_set(uint64_t bn){
          block_num = bn;
        };

        result_set(fc::variant v){
            block_num = v["block_num"].as_uint64();

            max_activity = v["max_activity"].as_string();
            current_activity = v["current_activity"].as_string();
            target_emission = v["target_emission"].as_string();
            //try {emission_limit = v["emission_limit"].as_string();}catch (...){}

            result_hash = v["result_hash"].as_string();

            auto var_list = v["res_list"].get_array();
            for(auto vi : var_list)
            {
                result_item ri;
                ri.name = vi["name"].as_string();
                ri.type = vi["type"].as_string();

                ri.soc_rate = vi["soc_rate"].as_string();
                ri.soc_rate_scaled = vi["soc_rate_scaled"].as_string();
                ri.trans_rate = vi["trans_rate"].as_string();
                ri.trans_rate_scaled = vi["trans_rate_scaled"].as_string();
                ri.staked_balance = vi["staked_balance"].as_string();
                ri.stake_rate = vi["stake_rate"].as_string();
                ri.stake_rate_scaled = vi["stake_rate_scaled"].as_string();
                ri.importance = vi["importance"].as_string();
                ri.importance_scaled = vi["importance_scaled"].as_string();

                ri.prev_cumulative_emission = vi["prev_cumulative_emission"].as_string();
                ri.current_emission = vi["current_emission"].as_string();
                ri.current_cumulative_emission = vi["current_cumulative_emission"].as_string();

                res_map[ri.name] = ri;
            }
        }

        fc::mutable_variant_object to_variant(){
            fc::mutable_variant_object result;
            result["block_num"] = block_num;

            result["max_activity"] = max_activity;
            result["current_activity"] = current_activity;
            result["target_emission"] = target_emission;
            result["emission_limit"] = emission_limit;

            result["result_hash"] = result_hash;
            fc::variants res_list;
            for(auto item : res_map){
                fc::mutable_variant_object res_var;
                res_var["name"] = item.second.name;
                res_var["type"] = item.second.type;

                res_var["soc_rate"] = item.second.soc_rate;
                res_var["soc_rate_scaled"] = item.second.soc_rate_scaled;
                res_var["trans_rate"] = item.second.trans_rate;
                res_var["trans_rate_scaled"] = item.second.trans_rate_scaled;
                res_var["staked_balance"] = item.second.staked_balance;
                res_var["stake_rate"] = item.second.stake_rate;
                res_var["stake_rate_scaled"] = item.second.stake_rate_scaled;
                res_var["importance"] = item.second.importance;
                res_var["importance_scaled"] = item.second.importance_scaled;

                res_var["prev_cumulative_emission"] = item.second.prev_cumulative_emission;
                res_var["current_emission"] = item.second.current_emission;
                res_var["current_cumulative_emission"] = item.second.current_cumulative_emission;

                res_list.push_back(res_var);
            }
            result["res_list"] = fc::variant(res_list);

            return result;
        }
    };

    class upvote_t: public singularity::relation_t
    {
    public:
        upvote_t (std::string source, std::string target, uint64_t height):
                relation_t(source, target, height)
        {};
        virtual int64_t get_weight() {
            return 1;
        };
        virtual int64_t get_reverse_weight() {
            return 0;
        };
        virtual std::string get_name() {
            return "UPVOTE";
        };
        virtual bool is_decayable() {
            return true;
        };
        virtual singularity::node_type get_source_type() {
            return singularity::node_type::ACCOUNT;
        };
        virtual singularity::node_type get_target_type(){
            return singularity::node_type::CONTENT;
        };
    };

    class downvote_t: public  singularity::relation_t
    {
    public:
        downvote_t (std::string source, std::string target, uint64_t height):
                relation_t(source, target, height)
        {};
        virtual int64_t get_weight() {
            return -1;
        };
        virtual int64_t get_reverse_weight() {
            return 0;
        };
        virtual std::string get_name() {
            return "DOWNVOTE";
        };
        virtual bool is_decayable() {
            return true;
        };
        virtual singularity::node_type get_source_type() {
            return singularity::node_type::ACCOUNT;
        };
        virtual singularity::node_type get_target_type(){
            return singularity::node_type::CONTENT;
        };
    };

    class ownership_t: public singularity::relation_t
    {
    public:
        ownership_t (std::string source, std::string target, uint64_t height):
                singularity::relation_t(source, target, height)
        {};
        virtual int64_t get_weight() {
            return 10;
        };
        virtual int64_t get_reverse_weight() {
            return 10;
        };
        virtual std::string get_name() {
            return "OWNERSHIP";
        };
        virtual bool is_decayable() {
            return false;
        };
        virtual singularity::node_type get_source_type() {
            return singularity::node_type::ACCOUNT;
        };
        virtual singularity::node_type get_target_type(){
            return singularity::node_type::CONTENT;
        };
    };

    class ownershiporg_t: public singularity::relation_t
    {
    public:
        ownershiporg_t (std::string source, std::string target, uint64_t height):
                singularity::relation_t(source, target, height)
        {};
        virtual int64_t get_weight() {
            return 10;
        };
        virtual int64_t get_reverse_weight() {
            return 10;
        };
        virtual std::string get_name() {
            return "ORGOWNERSHIP";
        };
        virtual bool is_decayable() {
            return false;
        };
        virtual singularity::node_type get_source_type() {
            return singularity::node_type::ORGANIZATION;
        };
        virtual singularity::node_type get_target_type(){
            return singularity::node_type::CONTENT;
        };
    };

    class transaction_t: public singularity::relation_t
    {
        using money_t = singularity::money_t;
        using node_type = singularity::node_type;
    private:
        money_t amount;
       // money_t comission;
       // money_t source_account_balance;
       // money_t target_account_balance;
        time_t timestamp;
    public:
        transaction_t (
                money_t amount,
//                money_t comission,
                std::string source,
                std::string target,
                time_t timestamp,
               // money_t source_account_balance,
               // money_t target_account_balance,
                uint64_t height
        ) :
                relation_t(source, target, height),
                amount(amount),
              //  comission(comission),
              //  source_account_balance(source_account_balance),
              //  target_account_balance(target_account_balance),
                timestamp(timestamp)
        { };
        virtual int64_t get_weight() {
            return (int64_t) amount;
        };
        virtual int64_t get_reverse_weight() {
            return - (int64_t) amount;
        };
        virtual std::string get_name() {
            return "TRANSFER";
        };
        money_t get_amount()
        {
            return amount;
        };
//        money_t get_source_account_balance()
//        {
//            return source_account_balance;
//        };
//        money_t get_target_account_balance()
//        {
//            return target_account_balance;
//        };
        virtual bool is_decayable() {
            return true;
        };
        virtual node_type get_source_type() {
            return node_type::ACCOUNT;
        };
        virtual node_type get_target_type(){
            return node_type::ACCOUNT;
        };
    };

}