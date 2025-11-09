//reminder:pipe,adjustable size,is_atty
#include <iostream>
#include <fstream> //to read from file
#include <string> 
#include <vector>
#include <algorithm> //for std::remove,std::find
#include <mutex> //for mutual exclusion to prevent race condition
#include <queue>
#include <thread>
#include <filesystem>//for std::filesystem::remove()
#include <array>
#include <cctype> //for std::tolower which is needed for for case insensitive search
#include <regex>
#include <unistd.h>//for isatty()
#define RED "\033[31m"
#define RESET "\033[0m"
#define YELLOW "\033[33m"
#define LIGHT_CYAN "\033[96m"
//necessary mutex for locking shared data among threads
std::mutex cout_mutex;
std::mutex cerr_mutex;
std::mutex pattern_queue_mutex;

bool case_insensitive=false;
bool highlight=true;
bool strict_search=false;
bool less=false;
bool is_pipe_output=!isatty(STDOUT_FILENO);
bool is_pipe_input=!isatty(STDIN_FILENO);
bool is_pipe_err=!isatty(STDERR_FILENO);

void add_virtual_file_from_piped_input(std::vector<std::string>& files){
    //first we will create a buffer and read line by line from std::cin and store that inside that buffer
    std::ostringstream buffer;
    std::string line;
    while(std::getline(std::cin,line)){
        buffer<<line<<"\n";
    }
    //create a temp file
    std::string temp_file="piped_input_file";
    //we will create a stream that is connected to our file i.e. create this file on disk in the current directory.
    std::ofstream out(temp_file);
    //now we can pour our buffer into temp_file through stream "out"
    out<<buffer.str();
    out.close();
    //now push this virtual file to files vector
    files.push_back(temp_file);
}
std::string highlight_pattern(std::string& line,std::regex& pattern){
    std::string highlighted_line;
    std::string::const_iterator curr_pos=line.cbegin();
    for (std::sregex_iterator it(line.begin(),line.end(),pattern);it!=std::sregex_iterator();++it){
        std::smatch match=*it;
        bool is_whole_word = true;
        if (match[0].first != line.begin()) {
            char before = *(match[0].first - 1);
            if (std::isalnum(static_cast<unsigned char>(before)) || before == '_' || before == '-') {
                is_whole_word = false;
            }
        }
        if (match[0].second != line.end()) {
            char after = *(match[0].second);
            if (std::isalnum(static_cast<unsigned char>(after)) || after == '_' || after == '-') {
                is_whole_word = false;
            }
        }
        if (strict_search&&(!is_whole_word)) continue;
        highlighted_line.append(curr_pos,match[0].first);//Append everything between starting_point and the beginning of this new match
        highlighted_line.append(LIGHT_CYAN);
        highlighted_line.append(match[0].first,match[0].second);//append match
        highlighted_line.append(RESET);
        curr_pos=match[0].second;//set index to next letter after match 
    }
    highlighted_line.append(std::string(curr_pos, line.cend()));//append remaining text after last match
    return highlighted_line;
}
void search_pattern(std::string& pattern,const std::vector<std::string>& files){
    std::string result;
    unsigned int match_for_curr_pattern=0;
    for(const auto& file:files){
        unsigned int match_in_curr_file=0;
        std::string line;//to store each line from file by std::getline
        std::ifstream opened_file(file);//i had to change name to opened_file cause same "file" was causing issue
        bool file_name_header_alredy_added=false;
        while (std::getline(opened_file,line)){
            bool line_has_match=false;
            //in next line the ternary conditional operator decides whether the pattern is case-sensitive or insensitive
            std::regex regex_pattern(pattern, case_insensitive ? std::regex_constants::icase : std::regex_constants::ECMAScript);
            if (regex_search(line,regex_pattern)){
                //below lines are to split the line in words and collecting those words in words_in_current_line
                std::regex word_regex(R"([A-Za-z0-9_\-\.@#!?:;,/~<>{}]+)");//word pattern.had to make it raw R to escape "-"
                std::sregex_token_iterator it(line.begin(),line.end(),word_regex);
                std::sregex_token_iterator end;
                std::vector<std::string> words_in_current_line(it,end);
                for (const auto& word:words_in_current_line){
                    if (std::regex_match(word,regex_pattern)||(std::regex_search(word,regex_pattern)&&(!strict_search))){
                        if (!line_has_match) line_has_match=true;
                        match_in_curr_file+=1;
                        match_for_curr_pattern+=1;
                    }
                }
            }
            if (line_has_match){
                if ((!file_name_header_alredy_added) && (!less)){
                    result.append("┌───────────────────────────────┐\n");
                    result.append("│ File: " + file+"\n");
                    result.append("└───────────────────────────────┘\n");
                    file_name_header_alredy_added=true;
                }
                if (highlight) line=highlight_pattern(line,regex_pattern);//by default highlight always on unless user provides --nh flag
                result.append("   → " +line+"\n");
            }
        }
        if ((match_in_curr_file!=0) && (!less)) result.append("\n("+pattern+ " appeared " + std::to_string(match_in_curr_file) + " times in " + file +")\n\n");
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
    bool is_thread_set_by_user=false;
    unsigned int thread_set_by_user;
    unsigned int num_threads;
    const unsigned int MAX_THREADS=std::min(4u, std::max(1u, std::thread::hardware_concurrency()));//thread count depends on cpu core but within 1 to 4.later i will make thread count configurable via command-line flag
    std::vector<std::thread> threads;
    std::array<std::string,5> flags={"--t","--i","--nh","--s","--l"};//t=thread,i=case insensitive,nh=highlight off,s=strict search
    //collects patterns and files name in respective container
    bool after_f=false;
    bool outside_file_scope=false;        
    if (is_pipe_input) add_virtual_file_from_piped_input(files);
    for (size_t i=1;i<argc;i++){
        std::string arg=argv[i];
        if(arg=="--f"){
            after_f=true;
        }else if(!after_f){
            pattern_queue.push(arg);
        }else{
            if(std::find(flags.begin(),flags.end(),arg)==flags.end() && (!outside_file_scope)){
                std::ifstream valid(arg);//only pushes valid files into files vector so we don't have to filter later
                if (valid){
                    files.push_back(arg);
                }else{
                    std::cerr<<"Error.Failed to open "<<arg<<std::endl;
                }
            }else{
                outside_file_scope=true;
                if (arg=="--t"){
                    if(i+1<argc){
                        try{
                            is_thread_set_by_user=true;
                            thread_set_by_user=std::stoul(argv[++i]);
                        }catch(const std::exception& e){
                            std::cerr << "Error: Invalid thread count after --t flag." << std::endl;
                            return 1;
                        }
                    }else{
                        std::cerr << "Error: Missing value after --t flag." << std::endl;
                        return 1;
                    }
                }
                if (arg=="--i") case_insensitive=true ;
                if (arg=="--nh") highlight=false;
                if (arg=="--s") strict_search=true;
                if (arg=="--l") less=true;
            }
        }
    }
    //checks if atleast one pattern and one file is provided
    if (pattern_queue.empty() || files.empty()){
        std::cerr<<"Error.Minimum one pattern and one valid file is required.Usage:"<<argv[0]<<"<pattern(s)> --f <filename(s)>"<<std::endl;
        return 1;
    }
    for (const auto& file:files){
        std::cout<<file<<std::endl;
    }
    //number of threads is decided by cpu core unless user provides it manually through --t flag.
    //if user doesn't set --t manually then maximum thread created is 4.mimimum 1.totally depends on pattern_queue size and cpu core.
    //when user set thread by --t maximum thread is 10 to prevent overloading.
    //no matter what thread number never exceeds patten=rn_queue size to prevent infinte thread creating and returning loop(crashed my program.took 1 hour to detect).
    num_threads=(is_thread_set_by_user?(std::min(std::min(10u,thread_set_by_user), static_cast<unsigned int>(pattern_queue.size()))):(std::min(MAX_THREADS, static_cast<unsigned int>(pattern_queue.size()))));
    //below block creates threads according to num_threads that calls "collecting_patterns" func
    //and adds that thread inside threads vector
    for (size_t i=0;i<num_threads;i++){
        threads.emplace_back(collecting_pattern_from_queue,std::ref(pattern_queue),std::cref(files));
    }
    for(auto& t:threads) t.join();
    if (is_pipe_input) std::filesystem::remove("piped_input_file");//clean that virtual file 
    return 0;
}