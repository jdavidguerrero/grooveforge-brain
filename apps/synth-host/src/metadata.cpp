#include "metadata.h"
#include "params.h"
#include <algorithm>
namespace host {
Json params_json(int e) {
    Json pages=Json::array();std::vector<std::string> names;
    for(const auto& p:catalog(e))if(std::find(names.begin(),names.end(),p.page)==names.end())names.push_back(p.page);
    for(const auto& name:names) {
        Json banks=Json::array(),bank=Json::array();
        int current_bank=-1;
        for(const auto& p:catalog(e))if(p.page==name) {
            if(p.bank>=0 && p.bank!=current_bank){if(bank.size()){banks.push(bank);bank=Json::array();}current_bank=p.bank;}
            Json param=object({{"id",p.id},{"name",p.name},{"unit",p.unit},{"min",p.min},{"max",p.max},{"default",p.initial},{"step",p.step},{"kind",p.kind()}});
            if(p.options>0){Json opts=Json::array();for(int i=0;i<p.options;++i)opts.push(i<int(p.labels.size())?p.labels[size_t(i)]:std::to_string(p.min+float(i)*p.step));param.set("options",opts);}
            bank.push(param);if(bank.size()==8){banks.push(bank);bank=Json::array();}
        }
        if(bank.size())banks.push(bank);pages.push(object({{"name",name},{"banks",banks}}));
    }
    return object({{"pages",pages}});
}
}
