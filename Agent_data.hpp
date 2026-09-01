#pragma once
#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <string.h>
struct Agent_data {
    int labelsize;
    int descriptionsize;
    char side;
    char label[];
    public:
 static   Agent_data* newAgent(char side,const std::string_view label,const std::span<const char> descrip) {
        int labelsize= label.size();
        int datalen =labelsize+1+descrip.size()+1+sizeof(Agent_data);
        Agent_data *agent=reinterpret_cast<Agent_data *>(new(std::align_val_t(alignof(Agent_data)),std::nothrow) char[datalen]{});
        if(!agent)
            return nullptr;
        agent->labelsize=labelsize;
        agent->descriptionsize=descrip.size();
        agent->labelsize=labelsize;
        agent->side=side;
        memcpy(agent->label,label.data(),labelsize);
        agent->label[labelsize]='\0';
        memcpy(agent->label+labelsize+1, descrip.data(),descrip.size());
        agent->label[labelsize+1+descrip.size()]='\0';
        return agent;
        }
 static void deleteAgent(Agent_data *agent) {
    ::operator delete[] (reinterpret_cast<char*>(agent),std::align_val_t(alignof(Agent_data)));
    }
    int getWhere() const {
        return side!='0';
        }
    std::string_view getLabel() const {
        return {label,size_t(labelsize)};
        }
    std::span<const char> getDescription() const {
        return std::span(label+labelsize+1,size_t(descriptionsize));
        }
    int datalen() const {
        return labelsize+1+descriptionsize+1+sizeof(Agent_data);
        }
    };

/** Alignment-safe, bounds-checked view of the legacy Agent_data wire format. */
struct AgentView {
    static constexpr size_t header_size=offsetof(Agent_data,label);
    static constexpr size_t legacy_padding=sizeof(Agent_data)-header_size;
    static constexpr size_t max_label_size=512;
    static constexpr size_t max_description_size=3072;

    char side;
    std::string_view label;
    std::span<const char> description;

    static std::optional<AgentView> parse(std::span<const char> body) {
        static_assert(sizeof(int)==4);
        if(body.size()<header_size+2)
            return std::nullopt;
        int label_size=0;
        int description_size=0;
        memcpy(&label_size,body.data(),sizeof(label_size));
        memcpy(&description_size,body.data()+sizeof(label_size),sizeof(description_size));
        const char side=body[sizeof(label_size)+sizeof(description_size)];
        if(label_size<0||description_size<0||(side!='0'&&side!='1'))
            return std::nullopt;
        const size_t label_len=static_cast<size_t>(label_size);
        const size_t description_len=static_cast<size_t>(description_size);
        if(label_len>max_label_size||description_len>max_description_size)
            return std::nullopt;
        if(label_len>body.size()-header_size-1)
            return std::nullopt;
        const size_t label_end=header_size+label_len;
        if(body[label_end]!='\0')
            return std::nullopt;
        const size_t description_start=label_end+1;
        if(description_start>=body.size())
            return std::nullopt;
        if(description_len>body.size()-description_start-1)
            return std::nullopt;
        const size_t payload_end=description_start+description_len+1;
        if(body[payload_end-1]!='\0')
            return std::nullopt;
        if(body.size()!=payload_end&&body.size()!=payload_end+legacy_padding)
            return std::nullopt;
        const std::string_view label(body.data()+header_size,label_len);
        const std::span<const char> description(body.data()+description_start,description_len);
        if(std::find(label.begin(),label.end(),'\0')!=label.end()||
           std::find(description.begin(),description.end(),'\0')!=description.end())
            return std::nullopt;
        return AgentView{side,label,description};
    }

    int getWhere() const { return side!='0'; }
    std::string_view getLabel() const { return label; }
    std::span<const char> getDescription() const { return description; }
};
