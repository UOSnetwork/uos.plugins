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
        map<string, result_item> res_map;
        string result_hash;

        result_set(uint64_t bn){
          block_num = bn;
        };
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