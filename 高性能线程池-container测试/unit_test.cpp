#pragma once
#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS
#define _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
#define _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#define SPDLOG_NO_EXCEPTIONS
#pragma warning(disable: 4996)   // ±ê×¼¿âÆúÓÃ

#include<iostream>
#include<thread>
#include<mutex>
#include<condition_variable>
#include<vector>
#include<queue>
#include<memory>
#include<functional>
#include<future>
#include"container.h"
#include"container_unit_test.h"
#include <string>
#include <iostream>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include<spdlog/async.h>
//#ifdef _CON_DEBUG

/*
#else
class con_debug {
public:
    void start(){}
};
#endif
*/
 

int main() {
    
    //ai_t::run_container_full_test<con::HighPerfCon128>("   ");
    return 0;
}