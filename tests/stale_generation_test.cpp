#include <assert.h>
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
    putdone(reinterpret_cast<const char *>(stale_done),stale_done->datalen(),
        {},&response,check,"test");
    assert(alldata.findEntry(label)==current_entry);
    release_response(response);
    Agent_data::deleteAgent(stale_done);

    Agent_data *stale_failure=Agent_data::newAgent('0',label,
        {old_description.data(),old_description.size()});
    putfailure(reinterpret_cast<const char *>(stale_failure),stale_failure->datalen(),
        {},&response,"test");
    assert(alldata.findEntry(label)==current_entry);
    release_response(response);
    Agent_data::deleteAgent(stale_failure);

    assert(alldata.eraseEntry(label,current_entry));
    assert(!alldata.findEntry(label));
    return 0;
}
