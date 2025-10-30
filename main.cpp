//reminder:highlight,case insensitive,strict search,other flags,codecrafters list,video
#include <iostream>
#include <fstream> //to read from file
#include <string> 
#include <vector>
#include <algorithm> //for std::remove
#include <mutex> //for mutual exclusion to prevent race condition
#include <queue>
#include <thread>
#define RED "\033[31m"
#define RESET "\033[0m"
#define YELLOW  "\033[33m"
//necessary mutex for locking shared data among threads
std::mutex cout_mutex;
std::mutex cerr_mutex;
std::mutex pattern_queue_mutex;

void search_pattern(std::string& pattern,const std::vector<std::string>& files){
    std::string result;
    unsigned int match_for_curr_pattern=0;
    for(const auto& file:files){
        unsigned int match_in_curr_file=0;
        std::string line;
        std::ifstream opened_file(file);//i had to change name to opened_file cause same "file" was causing issue
        result.append("┌───────────────────────────────┐\n");
        result.append("│ File: " + file+"\n");
        result.append("└───────────────────────────────┘\n");
        while (std::getline(opened_file,line)){
            if (line.find(pattern)!=std::string::npos){
                result.append("   → " +line+"\n");
                match_in_curr_file+=1;
                match_for_curr_pattern+=1;
            }
        }
        result.append("\n("+pattern+ " appeared " + std::to_string(match_in_curr_file) + " times in " + file +")\n\n");
        opened_file.close();
    }
    std::lock_guard<std::mutex> lock(cout_mutex);
    std::cout<<RED<< "\n==========================================\n";
    std::cout << "🔍 Searching for pattern: \"" << pattern << "\"\n";
    std::cout << "==========================================\n"<<RESET<<std::endl;
    std::cout<<YELLOW<<"\nTOTAL COUNT OF "<<pattern<<"="<<match_for_curr_pattern<<"\n"<<RESET<<std::endl;
    std::cout<<result<<std::endl;
}
//this function is a critical section.Each thread is assign to one pattern
//from pattern_queue and it's made sure more than one thread doesn't take same pattern
void collecting_pattern_from_queue(std::queue<std::string>& pattern_queue,const std::vector<std::string>& files){
    while (true){
        std::string pattern;
        {
            std::lock_guard<std::mutex> lock(pattern_queue_mutex);
            if (pattern_queue.empty()) return;//checks if pattern is available
            pattern=pattern_queue.front();//takes the first pattern available in queue
            pattern_queue.pop();           //and pops it out 
        }
        search_pattern(pattern,files);
    }
}
int main(int argc,char* argv[]){
    std::vector<std::string> files;
    std::queue<std::string> pattern_queue;
    const unsigned int MAX_THREADS=std::min(4u, std::max(1u, std::thread::hardware_concurrency()));//thread count depends on cpu core but within 1 to 4.later i will make thread count configurable via command-line flag
    std::vector<std::thread> threads;

    //handles argument error
    if (argc<4){    //minimum 4 arguments =program name + pattern(s) + --f + file(s)
        std::lock_guard<std::mutex> lock(cerr_mutex);//lock cerr
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
            pattern_queue.push(arg);
        }else{
            std::ifstream valid(arg);//only pushes valid files into files vector so we don't have to filter later
            if (valid){
                files.push_back(arg);
            }else{
                std::cerr<<"Error.Failed to open "<<arg<<std::endl;
            }
        }
    }
    //checks if atleast one pattern and one file is provided
    if (pattern_queue.empty() ||files.empty()){
        std::cerr<<"Error.Minimum one pattern and one valid file is required."<<std::endl;
        return 1;
    }
    //below block creates thread(maximum 4) that calls "collecting_patterns" func
    //and adds that thread inside threads vector
    size_t num_threads=std::min(MAX_THREADS, static_cast<unsigned int>(pattern_queue.size()));
    for (size_t i=0;i<num_threads;i++){
        threads.emplace_back(collecting_pattern_from_queue,std::ref(pattern_queue),std::cref(files));
    }
    for(auto& t:threads) t.join();
    return 0;
}