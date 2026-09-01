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

    std::string duplicate_length="PUT /failure HTTP/1.1\r\nContent-Length: 0\r\nContent-Length: 0\r\n\r\n";
    assert(!watchcommands(duplicate_length.data(),
        static_cast<int>(duplicate_length.size()),&response,true,check,"test"));
    release_response(response);

    std::string mismatched_length="PUT /failure HTTP/1.1\r\nContent-Length: 8\r\n\r\n";
    assert(!watchcommands(mismatched_length.data(),
        static_cast<int>(mismatched_length.size()),&response,true,check,"test"));
    release_response(response);
    Agent_data::deleteAgent(valid);
    return 0;
}
