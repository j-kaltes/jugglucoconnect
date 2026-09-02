#include <assert.h>
#include <cstring>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "../jugglucoconnect.cpp"

namespace {
struct always_valid final : valid_check {
    bool valid() const override {
        return true;
        }
};

void release_response(recdata &response) {
    delete[] response.allbuf;
    response.allbuf=nullptr;
}
}

int main() {
    constexpr std::string_view label="test-generation-label";
    constexpr std::string_view old_description="old-description-abcdefghijkl";
    constexpr std::string_view new_description="new-description-abcdefghijkl";

    auto old_entry=alldata.makeEntry(label,1);
    old_entry->descriptions[0].append(old_description);
    assert(alldata.eraseEntry(label,old_entry));

    auto current_entry=alldata.makeEntry(label,2);
    current_entry->descriptions[0].append(new_description);

    assert(!alldata.eraseEntry(label,old_entry));
    assert(alldata.findEntry(label)==current_entry);

    std::vector<std::thread> stale_cleanups;
    for(int thread_index=0;thread_index<8;++thread_index) {
        stale_cleanups.emplace_back([&] {
            for(int attempt=0;attempt<100;++attempt)
                assert(!alldata.eraseEntry(label,old_entry));
        });
    }
    for(auto &cleanup:stale_cleanups)
        cleanup.join();
    assert(alldata.findEntry(label)==current_entry);

    always_valid check;
    recdata response;

    Agent_data *stale_done=Agent_data::newAgent('0',label,
        {old_description.data(),old_description.size()});
    assert(stale_done);
    putdone(reinterpret_cast<const char *>(stale_done),stale_done->datalen(),
        {},&response,check,"test");
    assert(alldata.findEntry(label)==current_entry);
    release_response(response);
    Agent_data::deleteAgent(stale_done);

    Agent_data *stale_failure=Agent_data::newAgent('0',label,
        {old_description.data(),old_description.size()});
    assert(stale_failure);
    putfailure(reinterpret_cast<const char *>(stale_failure),stale_failure->datalen(),
        {},&response,"test");
    assert(alldata.findEntry(label)==current_entry);
    release_response(response);
    Agent_data::deleteAgent(stale_failure);

    assert(alldata.eraseEntry(label,current_entry));
    assert(!alldata.findEntry(label));

    constexpr std::string_view token_a="00112233445566778899aabbccddeeff";
    constexpr std::string_view token_b="102132435465768798a9bacbdcedfe0f";
    constexpr std::string_view token_c="ffeeddccbbaa99887766554433221100";
    const auto parsed_token=parseGenerationRequest(
        {token_a.data(),token_a.size()});
    assert(parsed_token&&parsed_token->local==token_a&&
           parsed_token->observedPeer.empty());
    const std::string observed_request=std::string(token_b)+":"+std::string(token_a);
    const auto parsed_observed=parseGenerationRequest(
        {observed_request.data(),observed_request.size()});
    assert(parsed_observed&&parsed_observed->local==token_b&&
           parsed_observed->observedPeer==token_a);
    constexpr std::string_view invalid_token="00112233445566778899aabbccddee-g";
    assert(!parseGenerationRequest({invalid_token.data(),invalid_token.size()}));

    GenerationStore generations(2,std::chrono::minutes(5));
    GenerationRequest side_zero{std::string(token_a),{}};
    GenerationRequest side_one{std::string(token_b),{}};
    auto result=generations.observe(
        "generation-test-label",0,side_zero,check,std::chrono::milliseconds(0));
    assert(result.kind==GenerationResultKind::timeout);
    result=generations.observe(
        "generation-test-label",1,side_one,check,std::chrono::milliseconds(0));
    assert(result.kind==GenerationResultKind::peer&&result.peer==token_a);

    GenerationRequest wait_for_change{std::string(token_a),std::string(token_b)};
    GenerationResult changed_result{GenerationResultKind::invalid,{}};
    std::thread waiting_generation([&] {
        changed_result=generations.observe(
            "generation-test-label",0,wait_for_change,check,
            std::chrono::seconds(2));
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    GenerationRequest changed_peer{std::string(token_c),std::string(token_a)};
    result=generations.observe(
        "generation-test-label",1,changed_peer,check,std::chrono::milliseconds(0));
    assert(result.kind==GenerationResultKind::timeout);
    waiting_generation.join();
    assert(changed_result.kind==GenerationResultKind::peer&&
           changed_result.peer==token_c);

    GenerationStore capacity_store(1,std::chrono::minutes(5));
    assert(capacity_store.observe(
        "generation-capacity-one",0,side_zero,check,
        std::chrono::milliseconds(0)).kind==GenerationResultKind::timeout);
    assert(capacity_store.observe(
        "generation-capacity-two",0,side_zero,check,
        std::chrono::milliseconds(0)).kind==GenerationResultKind::capacity);

    GenerationStore expiring_store(1,std::chrono::milliseconds(1));
    assert(expiring_store.observe(
        "generation-expiry-one",0,side_zero,check,
        std::chrono::milliseconds(0)).kind==GenerationResultKind::timeout);
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    assert(expiring_store.observe(
        "generation-expiry-two",0,side_zero,check,
        std::chrono::milliseconds(0)).kind==GenerationResultKind::timeout);
    assert(expiring_store.size()==1);

    Agent_data *valid=Agent_data::newAgent('1',label,
        {new_description.data(),new_description.size()});
    assert(valid);
    const std::span<const char> legacy_body(
        reinterpret_cast<const char *>(valid),valid->datalen());
    const auto legacy_view=AgentView::parse(legacy_body);
    assert(legacy_view);
    assert(legacy_view->getLabel()==label);
    assert(legacy_view->getDescription().size()==new_description.size());
    assert(AgentView::parse(legacy_body.first(
        legacy_body.size()-AgentView::legacy_padding)));
    assert(!AgentView::parse(legacy_body.first(legacy_body.size()-1)));

    std::vector<char> malformed(legacy_body.begin(),legacy_body.end());
    const int negative=-1;
    memcpy(malformed.data(),&negative,sizeof(negative));
    assert(!AgentView::parse(malformed));
    malformed.assign(legacy_body.begin(),legacy_body.end());
    malformed[sizeof(int)+sizeof(int)]='x';
    assert(!AgentView::parse(malformed));

    std::string request="PUT /failure HTTP/1.1\r\nContent-Length: "+
        std::to_string(legacy_body.size())+"\r\n\r\n";
    request.append(legacy_body.data(),legacy_body.size());
    assert(watchcommands(request.data(),static_cast<int>(request.size()),
        &response,true,check,"test"));
    release_response(response);

    constexpr std::string_view endpoint_label="generation-endpoint-label";
    assert(generationStore.observe(
        endpoint_label,0,side_zero,check,
        std::chrono::milliseconds(0)).kind==GenerationResultKind::timeout);
    Agent_data *generation_body=Agent_data::newAgent(
        '1',endpoint_label,{token_b.data(),token_b.size()});
    assert(generation_body);
    const std::span<const char> generation_span(
        reinterpret_cast<const char *>(generation_body),
        generation_body->datalen());
    std::string generation_http="PUT /generation HTTP/1.1\r\nContent-Length: "+
        std::to_string(generation_span.size())+"\r\n\r\n";
    generation_http.append(generation_span.data(),generation_span.size());
    assert(watchcommands(generation_http.data(),
        static_cast<int>(generation_http.size()),&response,true,check,"test"));
    const std::string_view generation_response(response.data(),response.size());
    const size_t generation_header_end=generation_response.find("\r\n\r\n");
    assert(generation_header_end!=std::string_view::npos);
    const std::string_view generation_payload=
        generation_response.substr(generation_header_end+4);
    assert(generation_payload.size()==sizeof(BackDescription)+token_a.size()+1);
    const auto *generation_back=reinterpret_cast<const BackDescription *>(
        generation_payload.data());
    assert(std::string_view(generation_back->description,token_a.size())==token_a);
    release_response(response);
    Agent_data::deleteAgent(generation_body);

    std::string duplicate_length="PUT /failure HTTP/1.1\r\nContent-Length: 0\r\nContent-Length: 0\r\n\r\n";
    assert(!watchcommands(duplicate_length.data(),
        static_cast<int>(duplicate_length.size()),&response,true,check,"test"));
    release_response(response);

    std::string mismatched_length="PUT /failure HTTP/1.1\r\nContent-Length: 8\r\n\r\n";
    assert(!watchcommands(mismatched_length.data(),
        static_cast<int>(mismatched_length.size()),&response,true,check,"test"));
    release_response(response);

    std::string invalid_origin="GET / HTTP/1.1\r\nOrigin: https://example.test\rInjected: value\nContent-Length: 0\r\n\r\n";
    assert(!watchcommands(invalid_origin.data(),
        static_cast<int>(invalid_origin.size()),&response,true,check,"test"));
    release_response(response);

    constexpr std::string_view race_label="same-label-race";
    std::vector<ConnectionPtr> raced_entries(32);
    std::vector<std::thread> creators;
    for(size_t index=0;index<raced_entries.size();++index) {
        creators.emplace_back([&,index] {
            raced_entries[index]=alldata.makeEntry(race_label,3);
        });
    }
    for(auto &creator:creators)
        creator.join();
    for(const auto &entry:raced_entries)
        assert(entry&&entry==raced_entries.front());

    std::vector<std::string> capacity_labels;
    std::vector<ConnectionPtr> capacity_entries;
    capacity_labels.reserve(128);
    capacity_entries.reserve(127);
    for(int index=0;index<127;++index) {
        capacity_labels.push_back("capacity-label-"+std::to_string(index));
        auto entry=alldata.makeEntry(capacity_labels.back(),4);
        assert(entry);
        capacity_entries.push_back(std::move(entry));
    }
    assert(!alldata.makeEntry("capacity-overflow",4));
    assert(alldata.eraseEntry(race_label,raced_entries.front()));
    for(size_t index=0;index<capacity_entries.size();++index)
        assert(alldata.eraseEntry(capacity_labels[index],capacity_entries[index]));

    Agent_data::deleteAgent(valid);
    return 0;
}
