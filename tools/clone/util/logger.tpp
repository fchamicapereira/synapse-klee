#include "../pch.h"

namespace Clone {
    template<typename T>
    void print_arg(T&& arg) {
        std::cout << arg;
    }

    template<typename T, typename... Args>
    void print_arg(T&& arg, Args&&... args) {
        std::cout << arg;
        print_arg(std::forward<Args>(args)...);
    }

    template<typename... Args>
    void info(Args&&... args) {
        std::cout << "\033[34m";
        std::cout << "[INFO] ";
        std::cout << "\033[37m";
        print_arg(std::forward<Args>(args)...);
        std::cout << std::endl;
    }

    template<typename... Args>
    void danger(Args&&... args) {
        std::cout << "\033[31m";
        std::cout << "[ERROR] ";
        std::cout << "\033[37m";
        print_arg(std::forward<Args>(args)...);
        std::cout << std::endl;
    }

    template<typename... Args>
    void debug(Args&&... args) {
        std::cout << "\033[35m";
        std::cout << "[DEBUG] ";
        std::cout << "\033[37m";
        print_arg(std::forward<Args>(args)...);
        std::cout << std::endl;
    }

    template<typename... Args>
    void success(Args&&... args) {
        std::cout << "\033[32m";
        std::cout << "[SUCCESS] ";
        std::cout << "\033[37m";    
        print_arg(std::forward<Args>(args)...);
        std::cout << std::endl;
    }

    template<typename... Args>
    void warn(Args&&... args) {
        std::cout << "\033[33m";
        std::cout << "[WARNING] ";
        std::cout << "\033[37m";
        print_arg(std::forward<Args>(args)...);
        std::cout << std::endl;
    }
}
