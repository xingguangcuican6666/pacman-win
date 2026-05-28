#include "include/include.h"

int core(int mode,std::string input,std::string output);

int main(int argc,char* argv[]){
    // std::cout << "hello" << std::endl;
    if(argc < 4 || (argv[1] != std::string_view("--save") && argv[1] != std::string_view("--load"))){
        std::cout << "Usage: langtools <--save|--load> <input_file> <output_file>" << std::endl;
        return ERROR_INVALID_CONFIG;
    }
    std::vector<std::string_view> obj;
    obj.reserve(argc);
    for(int i=0;i<argc;i++){
        obj.push_back(argv[i]);
    };
    int result=1;
    (argv[1] == std::string_view("--save")) ? result = core(0, argv[2], argv[3]) : result = core(1, argv[2], argv[3]);
    exit(result);
}
