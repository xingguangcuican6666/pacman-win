#include "include/include.h"
#include "include/json.hpp"

extern int obj_as(char* argv[],int argc){
    std::vector<std::string_view> obj;
    obj.reserve(argc);
    for(int i=0;i<argc;i++){
        obj.push_back(argv[i]);
    }
    for(std::string_view a : obj){
        // int len = a.length();
        /*
        用法:  pacman <操作> [...]
        操作:
        pacman {-h --help}
        pacman {-V --version}
        pacman {-D --database} <选项> <软件包>
        pacman {-F --files}    [选项] [文件]
        pacman {-Q --query}    [选项] [软件包]
        pacman {-R --remove}   [选项] <软件包>
        pacman {-S --sync}     [选项] [软件包]
        pacman {-T --deptest}  [选项] [软件包]
        pacman {-U --upgrade}  [选项] <文件>
        
        使用 'pacman {-h --help}' 及某个操作以查看可用选项
        */
        if(a.starts_with('-')){
            std::string_view b = a.substr(2);
            if(b == "") continue;
            switch(a.substr(1,1)[0]){
                case 'h':
                    config.HELP = 1;
                    break;
                case 'S':
                    config.SYNC = 1;
                    break;
                case 'V':
                    config.VERSION = 1;
                    break;
                case 'D':
                    config.DATABASE = 1;
                    break;
                case 'F':
                    config.FILES = 1;
                    break;
                case 'Q':
                    config.QUERY = 1;
                    break;
                case 'R':
                    config.REMOVE = 1;
                    break;
                case 'T':
                    config.DEPTEST = 1;
                    break;
                case 'U':
                    config.UPGRADE = 1;
                    break;
                default:
                    break;
            }
        };
        
    }
    return NORMAL;
}
