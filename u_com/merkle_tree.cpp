//
// Created by anton on 29.11.18.
//

#include <eosio/u_com/merkle_tree.hpp>

namespace uos {

    ///꧁꧂ 𐂅 happy merkle tree friends ꧁꧂

    template <class Type>
    ostream& operator<<(ostream& out, merkle_tree<Type>& mtree){
        for(auto &item: mtree.sum)
            out<<item.first<<';'<<item.second.first<<endl;
        return out;
    }

    template <>
    ostream& operator<<(ostream& out, merkle_tree<account_name>& mtree){
        for(auto &item: mtree.sum)
            out<<(uint64_t)item.first<<';'<<item.second.first<<endl;
        return out;
    }


    template <class Type>
    istream& operator>>(istream& in, pair <Type, string>& item){
        std::locale x(std::locale::classic(), new my_ctype);
        in.imbue(x);
        in>>item.first>>item.second;
        return in;
    }

    template <>
    istream& operator>>(istream& in, pair <account_name , string>& item){
        std::locale x(std::locale::classic(), new my_ctype);
        in.imbue(x);
        uint64_t first;
        in>>first>>item.second;
        item.first = first;
        return in;
    }


    template <class Type>
    istream& operator>>(istream& in, merkle_tree<Type>& mtree){
        pair<Type, string> item;
        while (!in.eof()){
            item.second="";
            in>>item;
            if(item.second=="")
                break;
            mtree.sum[item.first].first=item.second;
        }
        return in;
    }

///꧁꧂ 𐂅 merkle tree tests ꧁꧂

    void merkle_test() {
        cout << "꧁꧂     Merkle tree test " << endl << endl;

        merkle_tree <account_name> tree;
        vector <pair<account_name, double>> temp;
        for (size_t i = 0; i < 200; i++) {
            temp.emplace_back(make_pair(i + 10, (double) rand() / RAND_MAX));
        }
        tree.set_accounts(temp);
        tree.count_tree();
        string contr1 = tree.get_proof_for_contract(150);
        cout << contr1;
        cout << endl;
        stringstream ss;
        ss << tree;
        cout << endl << endl << ">>" << ss.str() << endl << endl;
        merkle_tree <account_name> tree2;
        ss >> tree2;
        tree2.count_tree();
        string contr2 = tree2.get_proof_for_contract(150);
        cout << tree2;
        cout << contr2;
        cout << endl;
        if (contr1 == contr2)
            elog("Test OK");
        else
            elog("Test Failed");

    }

    void merkle_test_for_strings() {
        cout << "꧁꧂     Merkle tree test 'for strings' " << endl << endl;

        merkle_tree <string> tree;
        vector <pair<string, double>> temp;

        temp.emplace_back(make_pair("Samuel", (double) rand() / RAND_MAX));
        temp.emplace_back(make_pair("Barbel", (double) rand() / RAND_MAX));
        temp.emplace_back(make_pair("Adagio", (double) rand() / RAND_MAX));
        temp.emplace_back(make_pair("For", (double) rand() / RAND_MAX));
        temp.emplace_back(make_pair("Strings", (double) rand() / RAND_MAX));
        temp.emplace_back(make_pair("B♭ minor", (double) rand() / RAND_MAX));

        tree.set_accounts(temp);
        tree.count_tree();
        string contr1 = tree.get_proof_for_contract("Strings");
        cout << contr1;
        cout << endl;
        stringstream ss;
        ss << tree;
        cout << endl << endl << ">>" << ss.str() << endl << endl;
        merkle_tree <string> tree2;
        ss >> tree2;
        tree2.count_tree();
        string contr2 = tree2.get_proof_for_contract("Strings");
        cout << tree2;
        cout << contr2;
        cout << endl;
        if (contr1 == contr2)
            elog("Test OK");
        else
            elog("Test Failed");

    }

    void merkle_test_for_strings_for_strings() {
        cout << "꧁꧂     Merkle tree test 'for strings' for strings " << endl << endl;

        merkle_tree <string> tree;
        vector <pair<string, string>> temp;

        temp.emplace_back(make_pair("Samuel", "1"));
        temp.emplace_back(make_pair("Barbel", "2"));
        temp.emplace_back(make_pair("Adagio", "3"));
        temp.emplace_back(make_pair("For", "4"));
        temp.emplace_back(make_pair("Strings", "5"));
        temp.emplace_back(make_pair("B♭ minor", "6"));

        tree.set_accounts(temp);
        tree.count_tree();
        string contr1 = tree.get_proof_for_contract("Strings");
        cout << contr1;
        cout << endl;
        stringstream ss;
        ss << tree;
        cout << endl << endl << ">>" << ss.str() << endl << endl;
        merkle_tree <string> tree2;
        ss >> tree2;
        tree2.count_tree();
        string contr2 = tree2.get_proof_for_contract("Strings");
        cout << tree2;
        cout << contr2;
        cout << endl;
        if (contr1 == contr2)
            elog("Test OK");
        else
            elog("Test Failed");

    }
}