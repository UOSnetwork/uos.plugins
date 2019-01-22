#pragma once

#include <appbase/application.hpp>
#include <eosio/chain_plugin/chain_plugin.hpp>

namespace uos_plugins{

    using namespace appbase;

    class uos_BE : public plugin<uos_BE>{
    public:
        uos_BE();
        virtual ~uos_BE();

        APPBASE_PLUGIN_REQUIRES()
        virtual void set_program_options(options_description&, options_description& cfg) override;

        void plugin_initialize(const variables_map& options);
        void plugin_startup();
        void plugin_shutdown();

        void irreversible_block_catcher(const eosio::chain::block_state_ptr& bst);
        void accepted_block_catcher(const eosio::chain::block_state_ptr& ast);
    private:
        std::unique_ptr<class uos_BE_impl> my;
    };

}