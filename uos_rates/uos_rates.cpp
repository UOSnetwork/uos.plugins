#include <eosio/uos_rates/uos_rates.hpp>
#include <eosio/chain/exceptions.hpp>
#include <eosio/chain_api_plugin/chain_api_plugin.hpp>
#include <eosio/http_plugin/http_plugin.hpp>
#include <../../../libraries/singularity/include/singularity.hpp>


#include <fc/io/json.hpp>
#include <fc/crypto/sha256.hpp>
#include <eosio/uos_rates/cvs.h>

#include <boost/program_options.hpp>


namespace eosio {
    using namespace std;

    static appbase::abstract_plugin& _uos_rates = app().register_plugin<uos_rates>();

    class uos_rates_impl {

    public:

        void irreversible_block_catcher(const chain::block_state_ptr& bsp);

        void calculate_rates(uint32_t current_calc_block_num);

        void calculate_result_hash();

        void report_hash(uint32_t current_calc_block_num);

        string get_consensus_leader();

        void set_rates();

        std::vector<std::shared_ptr<singularity::relation_t>> parse_transactions_from_block(
                eosio::chain::signed_block_ptr block, uint32_t current_calc_block);

        void run_transaction(
                string account,
                string action,
                fc::mutable_variant_object data,
                string pub_key,
                string priv_key,
                string acc_from = "");

        boost::program_options::variables_map _options;

        friend class uos_rates;

        CSVWriter logger{"result.csv"},logger_i{"input.csv"}, error_log{"error.txt"};

    private:

        int32_t period = 300*2;
        int32_t window = 86400*2*100;
        string contract_activity = "uos.activity";
        string contract_calculators = "calctest1111";
        string contract_rates = "uos.activity";
        std::set<chain::account_name> calculators;
        string calculator_public_key = "EOS58BF677xSvHd2Q4JiE4Xj2vEc3tzjbJya1onCxa7vKvZeK3rwt";
        string calculator_private_key = "5KGH33Z2zrBhWUmU3DmH9n1Jx2GL6H2Vwzk9AZLUPMJrMfWKgKr";
        string rates_public_key = "EOS6ZXGf34JNpBeWo6TXrKFGQAJXTUwXTYAdnAN4cajMnLdJh2onU";
        string rates_private_key = "5K2FaURJbVHNKcmJfjHYbbkrDXAt2uUMRccL6wsb2HX4nNU3rzV";


        uint64_t last_calc_block = 0;
        std::map<string, string> last_result;
        fc::sha256 last_result_hash;

        uint64_t last_setrate_block = 0;
    };

    void uos_rates_impl::irreversible_block_catcher(const eosio::chain::block_state_ptr &bsp) {
        //check the latency
        auto latency = (fc::time_point::now() - bsp->block->timestamp).count()/1000;
        if (latency > 100000)
            return;

        //determine current calculating block number
        auto irr_block_id = bsp->block->id();
        auto irr_block_num = bsp->block->num_from_id(irr_block_id);
        auto current_calc_block_num = irr_block_num - (irr_block_num % period);
        ilog((std::string("last_calc_block ") + std::to_string(last_calc_block) + " current_calc_block "+std::to_string(current_calc_block_num)).c_str());


        if(last_calc_block < current_calc_block_num) {
            //perform the calculations
            calculate_rates(current_calc_block_num);
            //reprort the result hash
            calculate_result_hash();
            report_hash(current_calc_block_num);

            last_calc_block = current_calc_block_num;
            return;
        }

        if(last_setrate_block < current_calc_block_num)
        {
            //find the consensus leader;
            string leader = get_consensus_leader();
            if (leader == "")
                return;

            //check if we have the leader among our calculators
            if(calculators.find(leader) == calculators.end())
                return;

            //set all rates
            set_rates();

            last_setrate_block = current_calc_block_num;
            return;
        }

        ilog("waiting for the next calculation block " + std::to_string(current_calc_block_num + period));
    }

    void uos_rates_impl::calculate_rates(uint32_t current_calc_block_num){
        int32_t end_block = current_calc_block_num;
        int32_t start_block = end_block - window + 1;
        if (start_block < 1)
            start_block = 1;

        ilog("start_block " + std::to_string(start_block));
        ilog("end_block " + std::to_string(end_block));


        chain::controller &cc = app().get_plugin<chain_plugin>().chain();

        //activity calculator for social interactions
        singularity::parameters_t params;
        auto calculator =
                singularity::rank_calculator_factory::create_calculator_for_social_network(params);

        logger_i.is_write = true;
        logger_i.setApart(false);

        logger_i.setFilename(std::string("input_")+ fc::variant(fc::time_point::now()).as_string()+".csv");

        for(int i = start_block; i <= end_block; i++)
        {
            auto block = cc.fetch_block_by_number(i);

            auto interactions = parse_transactions_from_block(block, current_calc_block_num);


            calculator->add_block(interactions);
        }


        auto a_result = calculator->calculate();

        singularity::gravity_index_calculator grv_cals(0.1, 0.9, 100000000000);

        ilog("a_result.size()" + std::to_string(a_result.size()));

        logger.is_write = true;
        logger.setApart(false);

        last_result.clear();
        for (auto group : a_result)
        {
            auto group_name = group.first;
            auto item_map = group.second;
            auto norm_map = grv_cals.scale_activity_index(*item_map);
            std::vector<std::string> vec;
            for (auto item : norm_map) {
                string name = item.first;
                string value = item.second.str(5);

                //replace new line symbol
                string nl_symbol = "\n";
                if(name.find(nl_symbol) !=  std::string::npos)
                    name.replace(name.find(nl_symbol), nl_symbol.size(), "'\\n'");

                last_result[name] = value;

                ilog(name + " " + value);

                vec.push_back(name);
                vec.push_back(value);
                logger.addDatainRow(vec.begin(), vec.end());
                vec.clear();
            }
            logger.setFilename(std::string("result_"+fc::variant(fc::time_point::now()).as_string()+"_"+ to_string_from_enum(group_name) +".csv"));
        }
    }

    void uos_rates_impl::calculate_result_hash(){
        string str_result = "";
        for(auto item : last_result)
            str_result += item.first + ";" + item.second + ";";
        last_result_hash = fc::sha256::hash(str_result);
    }

    void uos_rates_impl::report_hash(uint32_t current_calc_block_num){
        for(auto calc_name : calculators) {
            try {
                fc::mutable_variant_object data;
                data.set("acc", calc_name.to_string());
                data.set("hash", last_result_hash.str());
                data.set("block_num", current_calc_block_num);
                data.set("memo", "v1");// version lib
                string acc{"acc"};
                run_transaction(contract_calculators, "reporthash", data, calculator_public_key, calculator_private_key,
                                calc_name.to_string());
            }
            catch (const std::exception &e) {
                string c_fail = "\033[1;31;40m";
                string c_clear = "\033[1;0m";
                ilog(e.what());
                ilog(c_fail + "exception run transaction  calculate: block number " +
                     std::to_string(last_calc_block) + c_clear);
            }
        }
    }

    string uos_rates_impl::get_consensus_leader(){
        auto ro_api = app().get_plugin<chain_plugin>().get_read_only_api();

        //get list of current calculators
        std::set<string> current_calcs;
        chain_apis::read_only::get_table_rows_params get_calcs;
        get_calcs.code = eosio::chain::name(contract_calculators);
        get_calcs.scope = contract_calculators;
        get_calcs.table = eosio::chain::name("calcreg");
        get_calcs.limit = 100;
        get_calcs.json = true;
        auto calc_rows = ro_api.get_table_rows(get_calcs);
        for(auto cr : calc_rows.rows) {
            string calc_owner = cr["owner"].as_string();
            current_calcs.emplace(calc_owner);
            ilog("calc_owner " + calc_owner);
        }

        //determine minimum 3/4 consensus votes
        int consensus_minimum = (int)std::ceil(current_calcs.size() * 3.0 / 4.0);
        ilog("consensus_minimum " + std::to_string(consensus_minimum));

        //get list of reports for last_calc_block
        std::set<fc::variant> reports;
        chain_apis::read_only::get_table_rows_params get_reps;
        get_reps.code = eosio::chain::name(contract_calculators);
        get_reps.scope = contract_calculators;
        get_reps.table = N(reports);
        get_reps.key_type = chain_apis::i64;
        get_reps.index_position = "second";
        get_reps.lower_bound = std::to_string(last_calc_block);
        get_reps.limit = current_calcs.size();
        get_reps.json = true;
        auto rep_rows = ro_api.get_table_rows(get_reps);
        for(auto rr : rep_rows.rows) {
            if(rr["block_num"].as_string() != std::to_string(last_calc_block))
                break;
            reports.emplace(rr);
            ilog("report key:" + rr["key"].as_string() +
                 " acc:" + rr["acc"].as_string() +
                 " hash:" + rr["hash"].as_string() +
                 " block_num:" + rr["block_num"].as_string() +
                 " memo:" + rr["memo"].as_string());
        }

        //count votes for hashes
        std::map<string, int> hash_votes;
        for(auto r : reports) {
            auto hash = r["hash"].as_string();
            ilog("hash vote " + hash);
            ++hash_votes[hash];
        }

        //determine consensus hash
        string consensus_hash = "";
        for(auto hv : hash_votes) {
            if (hv.second >= consensus_minimum)
                consensus_hash = hv.first;
            ilog("hash " + hv.first + " votes " + std::to_string(hv.second));
        }

        //return empty string if no consensus hash
        if(consensus_hash == "") {
            ilog("no consensus hash");
            return "";
        }

        //determine the consensus leader by lowest report key
        uint32_t min_key = std::numeric_limits<uint32_t>::max();
        string leader_acc;
        for(auto r : reports) {
            if(r["hash"].as_string() != consensus_hash)
                continue;

            if(r["key"].as_uint64() >= min_key)
                continue;

            min_key = r["key"].as_uint64();
            leader_acc = r["acc"].as_string();
        }
        ilog("consensus leader " + leader_acc + " key " + std::to_string(min_key));

        return leader_acc;
    }

    void uos_rates_impl::set_rates(){
        for(auto item : last_result) {
            ilog("setrate name " + item.first + " value " + item.second);
            fc::mutable_variant_object data;
            data.set("name", item.first);
            data.set("value", item.second);
            run_transaction(contract_rates, "setrate", data, rates_public_key, rates_private_key, contract_rates);
        }
    }

    std::vector<std::shared_ptr<singularity::relation_t>> uos_rates_impl::parse_transactions_from_block(
            eosio::chain::signed_block_ptr block, uint32_t current_calc_block){

        std::vector<std::shared_ptr<singularity::relation_t>> interactions;
        auto ro_api = app().get_plugin<chain_plugin>().get_read_only_api();

        for (auto trs : block->transactions) {
            uint32_t block_height = current_calc_block - block->block_num();
            try {
                auto transaction = trs.trx.get<chain::packed_transaction>().get_transaction();
                auto actions = transaction.actions;
                for (auto action : actions) {

                    if (action.account.to_string() != contract_activity)
                        continue;
                    if(action.name.to_string()!="usertouser" &&
                       action.name.to_string()!="makecontent" &&
                       action.name.to_string()!= "usertocont" &&
                       action.name.to_string()!= "makecontorg")
                        continue;

                    chain_apis::read_only::abi_bin_to_json_params bins;
                    bins.code = action.account;
                    bins.action = action.name;
                    bins.binargs = action.data;
                    auto json = ro_api.abi_bin_to_json(bins);
                    auto object = json.args.get_object();

                    if (action.name.to_string() == "usertouser") {


//                        auto from = object["acc_from"].as_string();
//                        auto to = object["acc_to"].as_string();
//                        singularity::transaction_t tran(100000, 1, from, to, time_t(), 100000, 100000);
//                        transactions_t.push_back(tran);
//                        ilog("usertouser " + from + " " + to);
                    }


                    if (action.name.to_string() == "makecontent" ) {

                        auto from = object["acc"].as_string();
                        auto to = object["content_id"].as_string();
                        auto content_type_id = object["content_type_id"].as_string();
                        if(content_type_id == "5")
                            continue;

                        ownership_t ownership(from, to, block_height);
                        interactions.push_back(std::make_shared<ownership_t>(ownership));
                        ilog("makecontent " + from + " " + to);

                        std::string s1 = ownership.get_target();
                        fix_symbol(s1);
                        std::vector<std::string> vec{block->timestamp.to_time_point(),std::to_string(block->block_num()),ownership.get_source(),s1,
                                                     ownership.get_name(),std::to_string(ownership.get_height()),std::to_string(ownership.get_weight()),
                                                     std::to_string(ownership.get_reverse_weight()),to_string_from_enum(ownership.get_source_type()),to_string_from_enum(ownership.get_target_type())};
                        logger_i.addDatainRow(vec.begin(),vec.end());
                        vec.clear();


//                        auto parent = object["parent_content_id"].as_string();
//                        if(parent != "")
//                        {
//                            singularity::transaction_t tran2(100000, 1, from, parent, time_t(), 100000, 100000);
//                            transactions_t.push_back(tran2);
//                            ilog("parent content " + from + " " + parent);
//                        }
                    }

                    if (action.name.to_string() == "usertocont") {


                        auto from = object["acc"].as_string();
                        auto to = object["content_id"].as_string();
                        auto interaction_type_id = object["interaction_type_id"].as_string();
                        if(interaction_type_id == "2") {
                            upvote_t upvote(from, to, block_height);
                            interactions.push_back(std::make_shared<upvote_t>(upvote));
                            ilog("usertocont " + from + " " + to);

                            std::string s1 = upvote.get_target();
                            fix_symbol(s1);
                            std::vector<std::string> vec{block->timestamp.to_time_point(),std::to_string(block->block_num()),upvote.get_source(),s1, upvote.get_name(),std::to_string(upvote.get_height()),std::to_string(upvote.get_weight()),
                                                         std::to_string(upvote.get_reverse_weight()),to_string_from_enum(upvote.get_source_type()),to_string_from_enum(upvote.get_target_type())};
                            logger_i.addDatainRow(vec.begin(),vec.end());
                            vec.clear();
                        }
                        if(interaction_type_id == "4") {
                            downvote_t downvote(from, to, block_height);
                            interactions.push_back(std::make_shared<downvote_t>(downvote));
                            ilog("usertocont " + from + " " + to);

                            std::string s1 = downvote.get_target();
                            fix_symbol(s1);
                            std::vector<std::string> vec{block->timestamp.to_time_point(),std::to_string(block->block_num()),downvote.get_source(),s1,
                                                         downvote.get_name(),std::to_string(downvote.get_height()),std::to_string(downvote.get_weight()),
                                                         std::to_string(downvote.get_reverse_weight()),to_string_from_enum(downvote.get_source_type()),to_string_from_enum(downvote.get_target_type())};
                            logger_i.addDatainRow(vec.begin(),vec.end());
                            vec.clear();
                        }
                    }
                    if (action.name.to_string() == "makecontorg") {

                        auto from = object["organization_id"].as_string();
                        auto to = object["content_id"].as_string();
                        ownership_t ownershiporg(from, to, block_height);
                        interactions.push_back(std::make_shared<ownership_t>(ownershiporg));
                        ilog("makecontorg " + from + " " + to);

                        std::string s1 = ownershiporg.get_target();
                        fix_symbol(s1);
                        std::vector<std::string> vec{block->timestamp.to_time_point(),std::to_string(block->block_num()),ownershiporg.get_source(),s1,
                                                     ownershiporg.get_name(),std::to_string(ownershiporg.get_height()),std::to_string(ownershiporg.get_weight()),
                                                     std::to_string(ownershiporg.get_reverse_weight()),to_string_from_enum(ownershiporg.get_source_type()),to_string_from_enum(ownershiporg.get_target_type())};
                        logger_i.addDatainRow(vec.begin(),vec.end());
                        vec.clear();

                    }
                }
            }
            catch (...){
                ilog("exception" + std::to_string(block->block_num()));
                error_log.is_write = true;
                error_log.setApart(false);
                std::vector<std::string> err { std::to_string(block->block_num())};
                error_log.addDatainRow(err.begin(),err.end());
                err.clear();
            }
        }

        return interactions;
    }

    void uos_rates_impl::run_transaction(
            string account,
            string action,
            fc::mutable_variant_object data,
            string pub_key,
            string priv_key,
            string acc_from)
    {
        auto creator_priv_key = fc::crypto::private_key(priv_key);
        auto creator_pub_key = fc::crypto::public_key(pub_key);
        chain::controller &cc = app().get_plugin<chain_plugin>().chain();
        if(cc.pending_block_state()== nullptr){
            ilog("catch nullptr in activity");
        }
        else{
            ilog(fc::string(cc.pending_block_state()->header.timestamp.to_time_point()));
        }

        chain::signed_transaction signed_trx;
        chain::action act;
        chain::abi_serializer eosio_token_serializer;

        auto &accnt = cc.db().get<chain::account_object, chain::by_name>(account);
        eosio_token_serializer.set_abi(accnt.get_abi(), fc::milliseconds(100));

        act.name = action;//!!!!!!!!!!!!!!! move constants to settings
        act.account = account;//!!!!!!!
        act.authorization = vector<chain::permission_level>{{acc_from, chain::config::active_name}};

        act.data = eosio_token_serializer.variant_to_binary(action, data, fc::milliseconds(100));

        //signed_trx.actions.emplace_back(act);
        signed_trx.actions.push_back(act);

        signed_trx.expiration = cc.head_block_time() + fc::seconds(500);
        signed_trx.set_reference_block(cc.head_block_id());
        signed_trx.max_net_usage_words = 5000;
        signed_trx.sign(creator_priv_key, cc.get_chain_id());

        try {
            app().get_plugin<chain_plugin>().accept_transaction(
                    chain::packed_transaction(move(signed_trx)),
                    [](const fc::static_variant<fc::exception_ptr, chain::transaction_trace_ptr> &result) {});
            ilog("transaction sent " + action);

        } catch (...) {
            elog("Error in accept transaction");
        }
    }

    uos_rates::uos_rates():my(new uos_rates_impl()){}
    uos_rates::~uos_rates(){}

    void uos_rates::set_program_options(options_description&, options_description& cfg) {
        cfg.add_options()
                ("calculation-period", boost::program_options::value<int32_t>()->default_value(300*2), "Calculation period in blocks")
                ("calculation-window", boost::program_options::value<int32_t>()->default_value(86400*100*2), "Calculation window in blocks")
                ("contract-activity", boost::program_options::value<std::string>()->default_value("uos.activity"), "Contract account to get the input activity")
                ("contract-calculators", boost::program_options::value<std::string>()->default_value("calctest1111"), "Contract account to get the calculators list")
                ("contract-rates", boost::program_options::value<std::string>()->default_value("uos.activity"), "Contract account to save rates")
                ("calculator-name", boost::program_options::value<vector<string>>()->composing()->multitoken(),
                 "ID of calculator controlled by this node (e.g. calc1; may specify multiple times)")
                ("calculator-public-key", boost::program_options::value<std::string>()->default_value("EOS58BF677xSvHd2Q4JiE4Xj2vEc3tzjbJya1onCxa7vKvZeK3rwt"), "")
                ("calculator-private-key", boost::program_options::value<std::string>()->default_value("5KGH33Z2zrBhWUmU3DmH9n1Jx2GL6H2Vwzk9AZLUPMJrMfWKgKr"), "")
                ("rates-public-key", boost::program_options::value<std::string>()->default_value("EOS6ZXGf34JNpBeWo6TXrKFGQAJXTUwXTYAdnAN4cajMnLdJh2onU"), "")
                ("rates-private-key", boost::program_options::value<std::string>()->default_value("5K2FaURJbVHNKcmJfjHYbbkrDXAt2uUMRccL6wsb2HX4nNU3rzV"), "")
                ;
    }

    void uos_rates::plugin_initialize(const variables_map& options) {
        my->_options = &options;

        my->period = options.at("calculation-period").as<int32_t>();
        my->window = options.at("calculation-window").as<int32_t>();
        my->contract_activity = options.at("contract-activity").as<std::string>();
        my->contract_calculators = options.at("contract-calculators").as<std::string>();
        my->contract_rates = options.at("contract-rates").as<std::string>();

        if( options.count("calculator-name") ) {
            const std::vector<std::string>& ops = options["calculator-name"].as<std::vector<std::string>>();
            std::copy(ops.begin(), ops.end(), std::inserter(my->calculators, my->calculators.end()));
        }

        my->calculator_public_key = options.at("calculator-public-key").as<std::string>();
        my->calculator_private_key = options.at("calculator-private-key").as<std::string>();
        my->rates_public_key = options.at("rates-public-key").as<std::string>();
        my->rates_private_key = options.at("rates-private-key").as<std::string>();
    }

    void uos_rates::plugin_startup() {
        ilog( "starting uos_rates" );

        chain::controller &cc = app().get_plugin<chain_plugin>().chain();

        cc.irreversible_block.connect([this](const auto& bsp){my->irreversible_block_catcher(bsp);});

    }

    void uos_rates::plugin_shutdown() {
        // OK, that's enough magic
    }

    void uos_rates::irreversible_block_catcher(const eosio::chain::block_state_ptr &bst) { my->irreversible_block_catcher(bst);}

}
