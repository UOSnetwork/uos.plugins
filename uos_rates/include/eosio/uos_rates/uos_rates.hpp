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
}