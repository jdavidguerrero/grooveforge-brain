#include "metadata.h"
#include "params.h"
#include "engines/dx7_6op_core.h"
#include <fstream>
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=2){std::cerr<<"usage: gf-export-metadata output.json\n";return 1;}
    host::initialize_catalogs();host::Json engines,integers;
    for(size_t e=0;e<host::registry().size();++e) {
        const auto name=host::registry()[e].name;
        engines.set(name,host::params_json(int(e)).at("pages"));
        auto ids=host::Json::array();for(const auto& p:host::catalog(int(e)))if(p.storage!=host::Storage::Float)ids.push(p.id);
        integers.set(name,ids);
    }
    auto algorithms=host::Json::array();
    for(const auto& route:gf::dx7six::algorithms){auto flags=host::Json::array();for(auto f:route.ops)flags.push(int(f));algorithms.push(flags);}
    auto scaling=host::Json::array();
    for(int curve=0;curve<4;++curve){auto points=host::Json::array();for(int note=0;note<128;++note)points.push(gf::dx7six::scale_level(note,39,63,45,curve,curve));scaling.push(points);}
    std::ofstream out(argv[1]);out<<host::object({{"engines",engines},{"integer_params",integers},{"dx7_algorithms",algorithms},{"dx7_scaling_reference",scaling}}).dump()<<'\n';
    return out?0:1;
}
