#include "include/include.h"

int core(int mode,std::string input,std::string output){
    if(mode == 0){
        nlohmann::json j;
        std::ifstream i(input,std::ios_base::in);
        if(!i){
            return ERROR_NO_SUCH_FILE_OR_DIRECTORY;
        };
        try{
            i >> j;
        }catch(nlohmann::json::parse_error& e){
            std::cerr << e.what() << std::endl;
            // return ERROR_INVALID_CONFIG;
            exit(ERROR_INVALID_CONFIG);
        }
        if(std::filesystem::exists(output)) std::filesystem::remove(output);
        std::ofstream o(output,std::ios_base::out | std::ios_base::binary);
        std::vector<uint8_t> v_msgpack = nlohmann::json::to_msgpack(j);
        if(!o){
            return ERROR_NO_SUCH_FILE_OR_DIRECTORY;
        };
        o.write(reinterpret_cast<const char*>(v_msgpack.data()), v_msgpack.size());
        o.close();
        exit(NORMAL);
    } else {
        nlohmann::json j;
        std::vector<uint8_t> v_msgpack;
        std::ifstream i(input,std::ios_base::in | std::ios_base::binary | std::ios_base::ate);
        if(!i){
            return ERROR_NO_SUCH_FILE_OR_DIRECTORY;
        };
        std::streamsize size = i.tellg();
        i.seekg(0, std::ios::beg);
        std::vector<uint8_t> buffer(size);
        try
        {
            if (i.read(reinterpret_cast<char*>(buffer.data()), size)) {
                j = nlohmann::json::from_msgpack(buffer);
            }
        } catch (nlohmann::json::parse_error& e)
        {
            std::cerr << e.what() << std::endl;
            // return ERROR_INVALID_CONFIG;
            exit(ERROR_INVALID_CONFIG);
        }
        if(std::filesystem::exists(output)) std::filesystem::remove(output);
        std::ofstream o(output,std::ios_base::out);
        if(!o){
            return ERROR_NO_SUCH_FILE_OR_DIRECTORY;
        };
        o.write(j.dump(4).c_str(),j.dump(4).size());
        o.close();
        exit(NORMAL);
    }
}
