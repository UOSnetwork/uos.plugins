#include <eosio/uos_rates/uos_rates.hpp>
#include <eosio/uos_rates/transaction_queqe.hpp>
#include <eosio/chain/asset.hpp>
#include <eosio/chain/exceptions.hpp>
#include <eosio/chain_api_plugin/chain_api_plugin.hpp>
#include <eosio/http_plugin/http_plugin.hpp>
#include <../../../libraries/singularity/include/singularity.hpp>
#include <../../../libraries/singularity/generated/git_version.hpp>


#include <fc/io/json.hpp>
#include <fc/crypto/sha256.hpp>
#include <eosio/uos_rates/cvs.h>
#include <algorithm>
#include <boost/program_options.hpp>


namespace eosio {
    using namespace std;

    static appbase::abstract_plugin& _uos_rates = app().register_plugin<uos_rates>();

    class uos_rates_impl {

    public:

        void irreversible_block_catcher(const chain::block_state_ptr& bsp);

        void save_userres();

        result_set get_result_stub(uint64_t current_calc_block);

        void run_trx_queue(uint64_t num);

        void calculate_rates(uint32_t current_calc_block_num);

        void save_result();

        void report_hash();

        string get_consensus_leader();

        void set_rates();
        void set_emission();

        vector<std::string> get_all_accounts();

        vector<std::shared_ptr<singularity::relation_t>> parse_transactions_from_block(
                eosio::chain::signed_block_ptr block, uint32_t current_calc_block);

        void run_transaction(
                string account,
                string action,
                fc::mutable_variant_object data,
                string pub_key,
                string priv_key,
                string acc_from = "");

        void run_transaction(transaction_queue &temp);

        void add_transaction(
                string account,
                string action,
                fc::mutable_variant_object data,
                string pub_key,
                string priv_key,
                string acc_from = "");

        void run_transaction(trx_to_run trx){run_transaction(trx.account,trx.action,trx.data,trx.pub_key,trx.priv_key,trx.acc_from);}

        void add_transaction(trx_to_run trx){trx_queue.push(trx);}

        boost::program_options::variables_map _options;

        friend class uos_rates;

        CSVWriter social_activity_log,transfer_activity_log;

    private:

        int32_t period = 300*2;
        int32_t window = 86400*2*100;
        string contract_activity = "uos.activity";
        string contract_calculators = "uos.calcs";
        std::set<chain::account_name> calculators;
        string calculator_public_key = "";
        string calculator_private_key = "";
        string calc_contract_public_key = "";
        string calc_contract_private_key = "";


        double social_importance_share = 0.1;
        double transfer_importance_share = 0.1;
        double stake_importance_share = 1.0 - social_importance_share - transfer_importance_share;

        const uint32_t seconds_per_year = 365*24*3600;
        const double yearly_emission_percent = 1.0;
        const int64_t initial_token_supply = 1000000000;
        const uint8_t blocks_per_second = 2;

        bool dump_calc_data = false;
        bfs::path dump_dir;

        vector<std::shared_ptr<singularity::relation_t>> my_transfer_interactions;
        uint64_t last_calc_block = 0;
        result_set result = result_set(0); //TODO use pointer

        uint64_t last_setrate_block = 0;

        transaction_queue trx_queue;
    };

    void uos_rates_impl::irreversible_block_catcher(const eosio::chain::block_state_ptr &bsp) {

        //check the latency
        chain::controller &cc = app().get_plugin<chain_plugin>().chain();
        auto ro_api = app().get_plugin<chain_plugin>().get_read_only_api();
        chain_apis::read_only::get_block_params t;
        t.block_num_or_id = cc.fork_db_head_block_id();
        auto head_block = ro_api.get_block(t);
        auto head_block_time_str = head_block["timestamp"].as_string();
        auto head_block_time = fc::time_point::from_iso_string(head_block_time_str);
        auto latency = (fc::time_point::now() - head_block_time).count()/1000;
        if (latency > 1000)
            return;

        //determine current calculating block number
        auto irr_block_id = bsp->block->id();
        auto irr_block_num = bsp->block->num_from_id(irr_block_id);
        auto current_calc_block_num = irr_block_num - (irr_block_num % period);
        ilog(" irreversible block " + to_string(irr_block_num) +
             " last_calc_block " + to_string(last_calc_block) +
             " current_calc_block " + to_string(current_calc_block_num));

        if(last_calc_block < current_calc_block_num) {

            //perform the calculations
            calculate_rates(current_calc_block_num);

            //report the result hash
            report_hash();

            //save the result pack to file
            save_result();

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

            //emission token for account's with rates
            set_emission();

            last_setrate_block = current_calc_block_num;
            return;
        }

        ilog("waiting for the next calculation block " + std::to_string(current_calc_block_num + period));
    }

    void uos_rates_impl::save_userres() {

        string filename = "userresorces.csv";
        CSVWriter csv_result_userres{filename};
        csv_result_userres.settings(true, dump_dir.string(), filename);
        if(bfs::exists(csv_result_userres.getFilename()))
            bfs::remove(csv_result_userres.getFilename());

        vector<string> heading{
                "name",
                "cpu_weight",
                "net_weight"
        };

        csv_result_userres.addDatainRow(heading.begin(), heading.end());

        chain::controller &cc = app().get_plugin<chain_plugin>().chain();
        auto ro_api = app().get_plugin<chain_plugin>().get_read_only_api();
        chain_apis::read_only::get_account_params params;

        auto account_list = get_all_accounts();
        for (auto const &acc_name:  account_list){
            eosio::chain::name bn = acc_name;
            params.account_name = bn;
            asset val;
            auto core_symbol = val.symbol_name();
            try {
                auto users_info = ro_api.get_account(params);
                elog("Users resourses");
                auto cpu_weight = to_string(users_info.cpu_weight);
                auto net_weight = to_string(users_info.net_weight);
                vector<string> result = {acc_name, cpu_weight, net_weight};
                csv_result_userres.addDatainRow(result.begin(), result.end());
            }
            catch (exception &ex)
            {
                elog(ex.what());
            }

        }
    }

    result_set uos_rates_impl::get_result_stub(uint64_t current_calc_block){
        //TODO check if there is consensus hash in the consensus table

        //TODO get the results for this hash from the file

        //TODO request the results file from other nodes

        //if there is no previous result, create a new one
        auto result = result_set(current_calc_block);
        return result;
    }

    void uos_rates_impl::calculate_rates(uint32_t current_calc_block_num) {
        int32_t end_block = current_calc_block_num;
        int32_t start_block = end_block - window + 1;
        if (start_block < 1)
            start_block = 1;

        ilog("start_block " + std::to_string(start_block));
        ilog("end_block " + std::to_string(end_block));


        chain::controller &cc = app().get_plugin<chain_plugin>().chain();

        //activity calculator for social interactions
        singularity::parameters_t params;
        auto social_calculator =
                singularity::rank_calculator_factory::create_calculator_for_social_network(params);
        auto transfer_calculator =
                singularity::rank_calculator_factory::create_calculator_for_transfer(params);
        singularity::gravity_index_calculator grv_calculator(0.1, 0.9, 100000000000);

        transfer_activity_log.set_write_enabled(dump_calc_data);
        transfer_activity_log.set_path(dump_dir.string());
        transfer_activity_log.set_filename(
                std::string("transaction_") + fc::variant(fc::time_point::now()).as_string() + ".csv");

        social_activity_log.set_write_enabled(dump_calc_data);
        social_activity_log.set_path(dump_dir.string());
        social_activity_log.set_filename(
                std::string("social_activity_") + fc::variant(fc::time_point::now()).as_string() + ".csv");

        for (int i = start_block; i <= end_block; i++) {
            if (i % 1000000 == 0)
                ilog("block " + to_string(i));
            try {
                auto block = cc.fetch_block_by_number(i);
                auto social_interactions = parse_transactions_from_block(block, current_calc_block_num);
                social_calculator->add_block(social_interactions);
                transfer_calculator->add_block(my_transfer_interactions);
            }
            catch (std::exception e) {
                elog(e.what());
            }
            catch (...) {
                elog("Error on parsing block " + std::to_string(i));
            }
        }

        std::map<node_type, string> node_type_names;
        node_type_names[node_type::ACCOUNT] = "ACCOUNT";
        node_type_names[node_type::CONTENT] = "CONTENT";
        node_type_names[node_type::ORGANIZATION] = "ORGANIZATION";

        auto social_rates = social_calculator->calculate();
        auto transfer_rates = transfer_calculator->calculate();

        ilog("social_rates.size()" + std::to_string(social_rates.size()) +
             " transfer_rates.size()" + std::to_string(transfer_rates.size()));

        result = get_result_stub(current_calc_block_num);

        //set results for social rate");
        for (auto group : social_rates) {
            auto group_name = node_type_names[group.first];
            auto item_map = group.second;
            for (auto item : *item_map) {
                string name = item.first;
                string value = item.second.str(10);
                result.res_map[name].name = name;
                result.res_map[name].type = group_name;
                result.res_map[name].soc_rate = value;
            }

            ilog("scale the group results");
            auto scaled_map = grv_calculator.scale_activity_index(*item_map);
            for (auto item : scaled_map) {
                string name = item.first;
                string value = item.second.str(10);

                result.res_map[name].soc_rate_scaled = value;
            }
        }

        //set results for transfer rates");
        for (auto group : transfer_rates) {
            auto group_name = node_type_names[group.first];
            auto item_map = group.second;
            for (auto item : *item_map) {
                string name = item.first;
                string value = item.second.str(10);
                result.res_map[name].name = name;
                result.res_map[name].type = group_name;
                result.res_map[name].trans_rate = value;
            }

            auto scaled_map = grv_calculator.scale_activity_index(*item_map);
            vector<std::string> vec;

            for (auto item : scaled_map) {
                string name = item.first;
                string value = item.second.str(10);

                result.res_map[name].trans_rate_scaled = value;
            }
        }

        //set results for importance accounts");
        for (auto item : result.res_map) {
            auto name = item.second.name;
            if (item.second.type != "ACCOUNT")
                continue;

            double importance = stod(result.res_map[name].trans_rate) * transfer_importance_share +
                                stod(result.res_map[name].soc_rate) * social_importance_share;

            std::stringstream ss;
            ss << std::fixed << std::setprecision(10) << importance;
            result.res_map[name].importance = ss.str();

            double importance_scaled = stod(result.res_map[name].trans_rate_scaled) * transfer_importance_share +
                                       stod(result.res_map[name].soc_rate_scaled) * social_importance_share;

            ss.str("");
            ss << importance_scaled;
            result.res_map[name].importance_scaled = ss.str();
        }

        //set results for emission only for accounts");
        double total_emission = initial_token_supply
                                * yearly_emission_percent / 100
                                / seconds_per_year
                                * period / blocks_per_second;

        ilog("total_emission " + to_string(total_emission));
        for (auto item : result.res_map) {
            auto name = item.second.name;

            if (item.second.type != "ACCOUNT")
                continue;

            double emission = total_emission * stod(result.res_map[name].importance);
            double cumulative_emission = stod(result.res_map[name].prev_cumulative_emission) + emission;
            stringstream ss;
            ss << fixed << setprecision(4) << emission;
            result.res_map[name].current_emission = ss.str();
            ss.str("");
            ss << cumulative_emission;
            result.res_map[name].current_cumulative_emission = ss.str();
        }

        //calculate hash");
        string str_result = std::to_string(result.block_num) + ";";
        for (auto item : result.res_map)
            str_result += item.second.name + ";" + item.second.current_emission + ";";
        result.result_hash = fc::sha256::hash(str_result).str();
    }

    void uos_rates_impl::save_result()
    {
        string filename = "result_" + to_string(result.block_num) + "_" + result.result_hash + ".csv";
        CSVWriter csv_result{filename};
        csv_result.set_write_enabled(true);
        csv_result.set_path(dump_dir.string());
        csv_result.set_filename(filename);

        vector<string> heading{
                "name",
                "type",
                "soc_rate",
                "soc_rate_scaled",
                "trans_rate",
                "trans_rate_scaled",
                "importance",
                "importance_scaled",
                "current_emission",
                "prev_cumulative_emission",
                "current_cumulative_emission"
        };
        csv_result.addDatainRow(heading.begin(), heading.end());

        for(auto item : result.res_map)
        {
            vector<string> vec{
                item.second.name,
                item.second.type,
                item.second.soc_rate,
                item.second.soc_rate_scaled,
                item.second.trans_rate,
                item.second.trans_rate_scaled,
                item.second.importance,
                item.second.importance_scaled,
                item.second.current_emission,
                item.second.prev_cumulative_emission,
                item.second.current_cumulative_emission
            };
            csv_result.addDatainRow(vec.begin(), vec.end());
        }
    }


    void uos_rates_impl::report_hash(){
        for(auto calc_name : calculators) {
            ilog(calc_name.to_string() + " reportring hash " + result.result_hash +
                 " for block " + to_string(result.block_num));
            fc::mutable_variant_object data;
            data.set("acc", calc_name.to_string());
            data.set("hash", result.result_hash);
            data.set("block_num", result.block_num);
            data.set("memo", "v1");// version lib
            string acc{"acc"};
            add_transaction(contract_calculators, "reporthash", data, calculator_public_key, calculator_private_key,
                            calc_name.to_string());
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
        vector<fc::variant> reports;
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
            reports.push_back(rr);
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
        for(auto item : result.res_map) {
            fc::mutable_variant_object data;
            data.set("name", item.second.name);
            data.set("value", item.second.soc_rate_scaled);
            add_transaction(contract_calculators, "setrate", data, calc_contract_public_key, calc_contract_private_key, contract_calculators);
        }
        for(auto item : result.res_map) {
            fc::mutable_variant_object data;
            data.set("name", item.second.name);
            data.set("value", item.second.trans_rate_scaled);
            add_transaction(contract_calculators, "setratetran", data, calc_contract_public_key, calc_contract_private_key, contract_calculators);
        }
    }

    vector<std::string> uos_rates_impl::get_all_accounts()
    {
        chain::controller &cc = app().get_plugin<chain_plugin>().chain();
        const auto &database = cc.db();
        typedef typename chainbase::get_index_type< chain::account_object >::type index_type;
        const auto &table = database.get_index<index_type,chain::by_name>();
        vector <std::string> account_name;
        for(chain::account_object item: table){
           // ilog(c_fail + "ACCOUNTS:" + item.name.to_string()+ c_clear);
            account_name.push_back(item.name.to_string());
        }
        return account_name;
    }

    void uos_rates_impl::set_emission()
    {
        auto account_list = get_all_accounts();

        for(auto item : result.res_map) {
            //only for real accounts
            if(find(account_list.begin(), account_list.end(), item.second.name) == account_list.end())
                continue;

            fc::mutable_variant_object data;
            data.set("issuer", contract_calculators);
            data.set("receiver", item.second.name);
            data.set("sum", stod(item.second.current_emission));
            data.set("message", "charge for rates " + item.second.name + " " + item.second.current_emission);

            add_transaction(contract_calculators, "addsum", data, calc_contract_public_key, calc_contract_private_key, contract_calculators);
        }

    }

    vector<std::shared_ptr<singularity::relation_t>> uos_rates_impl::parse_transactions_from_block(
            eosio::chain::signed_block_ptr block, uint32_t current_calc_block){

        vector<std::shared_ptr<singularity::relation_t>> social_interactions;
        vector<std::shared_ptr<singularity::relation_t>> transfer_interactions;

        auto ro_api = app().get_plugin<chain_plugin>().get_read_only_api();

        for (auto trs : block->transactions) {
            uint32_t block_height = current_calc_block - block->block_num();

            auto transaction = trs.trx.get<chain::packed_transaction>().get_transaction();
            auto actions = transaction.actions;
            for (auto action : actions) {

                if (action.account == N(eosio.token)) {
                    //ilog("TRANSFER FOUND BLOCK:" + std::to_string(block->block_num()) + action.name.to_string());

                    if (action.name == N(transfer)) {

                        chain_apis::read_only::abi_bin_to_json_params bins;
                        bins.code = action.account;
                        bins.action = action.name;
                        bins.binargs = action.data;
                        auto json = ro_api.abi_bin_to_json(bins);
                        auto object = json.args.get_object();

                        auto from = object["from"].as_string();
                        auto to = object["to"].as_string();
                        auto quantity = asset::from_string(object["quantity"].as_string()).get_amount();
                        auto memo = object["memo"].as_string();

                        time_t epoch = 0;

                        transaction_t transfer(quantity,from, to,epoch , block_height);
                        transfer_interactions.push_back(std::make_shared<transaction_t>(transfer));
                         my_transfer_interactions = transfer_interactions;

                        vector<std::string> vec{action.account.to_string(), action.name.to_string(),
                                                     from,to,std::to_string(quantity),memo, std::to_string(block->block_num()), block->timestamp.to_time_point()};
                        transfer_activity_log.addDatainRow(vec.begin(), vec.end());
                        vec.clear();
                    }
                }

                if (action.account != eosio::chain::string_to_name(contract_activity.c_str()))
                    continue;
                if(action.name != N(usertouser) &&
                   action.name != N(makecontent) &&
                   action.name != N(usertocont) &&
                   action.name != N(makecontorg))
                    continue;

                chain_apis::read_only::abi_bin_to_json_params bins;
                bins.code = action.account;
                bins.action = action.name;
                bins.binargs = action.data;
                auto json = ro_api.abi_bin_to_json(bins);
                auto object = json.args.get_object();


                if (action.name == N(usertouser)) {
                }


                if (action.name == N(makecontent) ) {

                    auto from = object["acc"].as_string();
                    auto to = object["content_id"].as_string();
                    auto content_type_id = object["content_type_id"].as_string();
                    if(content_type_id == "4")
                        continue;

                    string nl_symbol = "\n";
                    if(from.find(nl_symbol) !=  std::string::npos ||
                            to.find(nl_symbol) != std::string::npos)
                        continue;

                    ownership_t ownership(from, to, block_height);
                    social_interactions.push_back(std::make_shared<ownership_t>(ownership));
                    //ilog("makecontent " + from + " " + to);

                    std::string s1 = ownership.get_target();
                    vector<std::string> vec{block->timestamp.to_time_point(),std::to_string(block->block_num()),ownership.get_source(),s1,
                                                 ownership.get_name(),std::to_string(ownership.get_height()),std::to_string(ownership.get_weight()),
                                                 std::to_string(ownership.get_reverse_weight()),to_string_from_enum(ownership.get_source_type()),to_string_from_enum(ownership.get_target_type())};
                    social_activity_log.addDatainRow(vec.begin(),vec.end());
                }

                if (action.name == N(usertocont)) {


                    auto from = object["acc"].as_string();
                    auto to = object["content_id"].as_string();
                    auto interaction_type_id = object["interaction_type_id"].as_string();

                    string nl_symbol = "\n";
                    if(from.find(nl_symbol) !=  std::string::npos ||
                            to.find(nl_symbol) != std::string::npos)
                        continue;

                    if(interaction_type_id == "2") {
                        upvote_t upvote(from, to, block_height);
                        social_interactions.push_back(std::make_shared<upvote_t>(upvote));
                        //ilog("usertocont " + from + " " + to);

                        std::string s1 = upvote.get_target();
                        vector<std::string> vec{block->timestamp.to_time_point(),std::to_string(block->block_num()),upvote.get_source(),s1, upvote.get_name(),std::to_string(upvote.get_height()),std::to_string(upvote.get_weight()),
                                                     std::to_string(upvote.get_reverse_weight()),to_string_from_enum(upvote.get_source_type()),to_string_from_enum(upvote.get_target_type())};
                        social_activity_log.addDatainRow(vec.begin(),vec.end());
                        vec.clear();
                    }
                    if(interaction_type_id == "4") {
                        downvote_t downvote(from, to, block_height);
                        social_interactions.push_back(std::make_shared<downvote_t>(downvote));
                        //ilog("usertocont " + from + " " + to);

                        std::string s1 = downvote.get_target();
                        vector<std::string> vec{block->timestamp.to_time_point(),std::to_string(block->block_num()),downvote.get_source(),s1,
                                                     downvote.get_name(),std::to_string(downvote.get_height()),std::to_string(downvote.get_weight()),
                                                     std::to_string(downvote.get_reverse_weight()),to_string_from_enum(downvote.get_source_type()),to_string_from_enum(downvote.get_target_type())};
                        social_activity_log.addDatainRow(vec.begin(),vec.end());
                        vec.clear();
                    }
                }
                if (action.name == N(makecontorg)) {

                    auto from = object["organization_id"].as_string();
                    auto to = object["content_id"].as_string();
                    ownership_t ownershiporg(from, to, block_height);
                    social_interactions.push_back(std::make_shared<ownership_t>(ownershiporg));

                    string nl_symbol = "\n";
                    if(from.find(nl_symbol) !=  std::string::npos ||
                       to.find(nl_symbol) != std::string::npos)
                        continue;

                    std::string s1 = ownershiporg.get_target();
                    vector<std::string> vec{block->timestamp.to_time_point(),std::to_string(block->block_num()),ownershiporg.get_source(),s1,
                                                 ownershiporg.get_name(),std::to_string(ownershiporg.get_height()),std::to_string(ownershiporg.get_weight()),
                                                 std::to_string(ownershiporg.get_reverse_weight()),to_string_from_enum(ownershiporg.get_source_type()),to_string_from_enum(ownershiporg.get_target_type())};
                    social_activity_log.addDatainRow(vec.begin(),vec.end());
                    vec.clear();

                }
            }

        }

        return social_interactions;
    }

    void uos_rates_impl::run_trx_queue(uint64_t num) {
        if (num == 0)
            return;
        if((fc::time_point::now() - app().get_plugin<chain_plugin>().chain().head_block_header().timestamp)  < fc::seconds(3)) {
            ilog("Run transactions");
            if (trx_queue.empty())
                return;
            trx_to_run last = trx_queue.front();
            transaction_queue temp;
            trx_queue.pop();
            temp.push(last);
            for (uint64_t i = 1; i < num; i++) {
                if (trx_queue.empty())
                    break;
//                ilog("trx_queue.front() " + trx_queue.front().account + " " +
//                             trx_queue.front().acc_from + " " +
//                             trx_queue.front().action + " " +
//                             fc::json::to_string(trx_queue.front().data) + " " +
//                             trx_queue.front().priv_key + " " + " " +
//                             trx_queue.front().pub_key);
                if((last.account==trx_queue.front().account)&&(last.action==trx_queue.front().action)){
                    temp.push(trx_queue.front());
                    trx_queue.pop();
                }
                else{
                    run_transaction(temp);
                    if(temp.size()>0){
                        elog("queue > 0, something went wrong");
                        return;
                    }
                    last = trx_queue.front();
                    temp.push(last);
                    trx_queue.pop();
                }
            }
            run_transaction(temp);
            if(temp.size()>0){
                elog("queue > 0, something went wrong");
                return;
            }
        }
    }

    void uos_rates_impl::add_transaction(
            string account,
            string action,
            fc::mutable_variant_object data,
            string pub_key,
            string priv_key,
            string acc_from)
    {
        trx_queue.emplace(trx_to_run(account,action,data,pub_key,priv_key,acc_from));
    }

    void uos_rates_impl::run_transaction(transaction_queue &temp){
        auto creator_priv_key = fc::crypto::private_key(temp.front().priv_key);
        auto creator_pub_key = fc::crypto::public_key(temp.front().pub_key);
        chain::controller &cc = app().get_plugin<chain_plugin>().chain();

        chain::signed_transaction signed_trx;
        chain::action act;
        chain::abi_serializer eosio_token_serializer;

        auto &accnt = cc.db().get<chain::account_object, chain::by_name>(temp.front().account);
        eosio_token_serializer.set_abi(accnt.get_abi(), fc::milliseconds(100));

        act.name = temp.front().action;//!!!!!!!!!!!!!!! move constants to settings
        act.account = temp.front().account;//!!!!!!!
        act.authorization = vector<chain::permission_level>{{temp.front().acc_from, chain::config::active_name}};

        while(temp.size()) {
            act.data = eosio_token_serializer.variant_to_binary(temp.front().action, temp.front().data, fc::milliseconds(100));
            signed_trx.actions.push_back(act);
            temp.pop();
        }
        signed_trx.expiration = cc.head_block_time() + fc::seconds(5);
        signed_trx.set_reference_block(cc.head_block_id());
        signed_trx.max_net_usage_words = 5000;
        signed_trx.sign(creator_priv_key, cc.get_chain_id());
        try {
            app().get_method<eosio::chain::plugin_interface::incoming::methods::transaction_async>()(
                    std::make_shared<chain::packed_transaction>(chain::packed_transaction(move(signed_trx))),
                    true,
                    [this](const fc::static_variant<fc::exception_ptr, chain::transaction_trace_ptr>& result) -> void{
                        if (result.contains<fc::exception_ptr>()) {
                            elog(fc::json::to_string(result.get<fc::exception_ptr>()));
                        } else {
                            auto trx_trace_ptr = result.get<chain::transaction_trace_ptr>();

                            try {
                                fc::variant pretty_output;
                                pretty_output = app().get_plugin<chain_plugin>().chain().to_variant_with_abi(*trx_trace_ptr, fc::milliseconds(100));
                                ilog(fc::json::to_string(pretty_output));
                            }
                            catch (...){
                                elog("Error ");
                            }
                        }
                    });
            ilog("transaction sent ");

        } catch (...) {
            elog("Error in accept transaction");
        }

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

        signed_trx.expiration = cc.head_block_time() + fc::seconds(5);
        signed_trx.set_reference_block(cc.head_block_id());
        signed_trx.max_net_usage_words = 5000;
        signed_trx.sign(creator_priv_key, cc.get_chain_id());

        try {

            app().get_method<eosio::chain::plugin_interface::incoming::methods::transaction_async>()(
                    std::make_shared<chain::packed_transaction>(chain::packed_transaction(move(signed_trx))),
                    true,
                    [this](const fc::static_variant<fc::exception_ptr, chain::transaction_trace_ptr>& result) -> void{
                    if (result.contains<fc::exception_ptr>()) {
                        elog(fc::json::to_string(result.get<fc::exception_ptr>()));
                    } else {
                        auto trx_trace_ptr = result.get<chain::transaction_trace_ptr>();

                        try {
                            fc::variant pretty_output;
                            pretty_output = app().get_plugin<chain_plugin>().chain().to_variant_with_abi(*trx_trace_ptr, fc::milliseconds(100));
                            ilog(fc::json::to_string(pretty_output));
                        }
                        catch (...){
                            elog("Error ");
                        }
                    }
            });

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
                ("contract-calculators", boost::program_options::value<std::string>()->default_value("uos.calcs"), "Contract account to get the calculators list")
                ("calculator-name", boost::program_options::value<vector<string>>()->composing()->multitoken(),
                 "ID of calculator controlled by this node (e.g. calc1; may specify multiple times)")
                ("calculator-public-key", boost::program_options::value<std::string>()->default_value(""), "")
                ("calculator-private-key", boost::program_options::value<std::string>()->default_value(""), "")
                ("calc-contract-public-key", boost::program_options::value<std::string>()->default_value(""), "")
                ("calc-contract-private-key", boost::program_options::value<std::string>()->default_value(""), "")
                ("dump-calc-data", boost::program_options::value<bool>()->default_value(false), "Save the input and output data as *.csv files")
                ;
    }

    void uos_rates::plugin_initialize(const variables_map& options) {
        my->_options = &options;

        my->period = options.at("calculation-period").as<int32_t>();
        my->window = options.at("calculation-window").as<int32_t>();
        my->contract_activity = options.at("contract-activity").as<std::string>();
        my->contract_calculators = options.at("contract-calculators").as<std::string>();

        if( options.count("calculator-name") ) {
            const vector<std::string>& ops = options["calculator-name"].as<vector<std::string>>();
            std::copy(ops.begin(), ops.end(), std::inserter(my->calculators, my->calculators.end()));
        }

        my->calculator_public_key = options.at("calculator-public-key").as<std::string>();
        my->calculator_private_key = options.at("calculator-private-key").as<std::string>();
        my->calc_contract_public_key = options.at("calc-contract-public-key").as<std::string>();
        my->calc_contract_private_key = options.at("calc-contract-private-key").as<std::string>();

        my->dump_calc_data = options.at("dump-calc-data").as<bool>();

        auto dump_name = bfs::path("dump");
        my->dump_dir = app().data_dir() / dump_name;
    }

    void uos_rates::plugin_startup() {
        ilog( "starting uos_rates" );

        chain::controller &cc = app().get_plugin<chain_plugin>().chain();

        cc.irreversible_block.connect([this](const auto& bsp){
            my->run_trx_queue(10);
            my->irreversible_block_catcher(bsp);
        });

    }

    void uos_rates::plugin_shutdown() {
        // OK, that's enough magic
    }

    void uos_rates::irreversible_block_catcher(const eosio::chain::block_state_ptr &bst) { my->irreversible_block_catcher(bst);}

}
