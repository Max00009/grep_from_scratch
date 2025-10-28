//reminder:multithreading,highlight,case insensitive,tree,strict search
#include <iostream>
#include <fstream> //to read from file
#include <string> 
#include <vector>
#include <algorithm> //for std::remove
int main(int argc,char* argv[]){
    std::vector<std::string> patterns;
    std::vector<std::string> files;
    std::vector<std::string> cant_open_files; 
    std::ifstream file(argv[2]);

    //handles argument error
    if (argc<4){    //minimum 4 arguments =program name + pattern(s) + --f + file(s)
        std::cerr<<"pattern and filename in proper format must be provided.Usage: "<<argv[0]<<"<pattern(s)> --f <filename(s)>"<<std::endl;
        return 1;
    }

    //collects patterns and files name in respective container
    bool after_f=false;
    for (size_t i=1;i<argc;i++){
        std::string arg=argv[i];
        if (arg=="--f"){
            after_f=true;
        }else if(!after_f){
            patterns.push_back(arg);
        }else{
            files.push_back(arg);
        }
    }
    //checks if atleast one pattern and one file is provided
    if (patterns.empty() ||files.empty()){
        std::cerr<<"Error.Minimum one pattern and file is required"<<std::endl;
        return 1;
    }
    for (const auto& pattern:patterns){
        std::vector<std::string> result;
        std::cout<<"----------Searching for: "<<pattern<<"----------"<<std::endl;
        unsigned int match_for_curr_pattern=0;
        for (const auto& file:files){
            std::string line;
            std::ifstream opened_file(file);//i had to change name to opened_file cause same "file" was causing issue
            if (!opened_file){
                std::cout<<"Error:Failed to open file "<<file<<".\n";
                cant_open_files.push_back(file);
                if(files.size()>1){
                    std::cout<<"Trying next file."<<std::endl;
                }
                continue;
            }
            result.push_back("\nMatches inside - " + file+":\n");
            unsigned int match_in_curr_file=0;
            while (std::getline(opened_file,line)){
                if (line.find(pattern)!=std::string::npos){
                    result.push_back("  "+line);
                    match_in_curr_file+=1;
                    match_for_curr_pattern+=1;
                }
            }
            result.push_back("\n("+pattern+ " appeared " + std::to_string(match_in_curr_file) + " times in " + file +")\n\n");
            opened_file.close();
        }
        files.erase(std::remove(files.begin(), files.end(), file), files.end());//remove the file which can't be opened so during next parameter iteration can't be bother to open
        std::cout<<"\n TOTAL COUNT OF "<<pattern<<"="<<match_for_curr_pattern<<std::endl;
        for (const auto& matched_line:result){
            std::cout<<matched_line<<std::endl;
        }
    }
    return 0;
}