#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include "drum_files.h"
#include <fstream>
int main(int argc,char** argv){if(argc!=2)return 1;std::ofstream out(argv[1]);out<<host::object({{"drum_version",1},{"pages",host::drum_metadata()},{"empty_rack",host::drum_empty()}}).dump()<<'\n';return out?0:1;}
