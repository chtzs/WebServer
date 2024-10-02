//
// Created by Haotian on 2024/10/2.
//

#ifndef PREDEFINED_H
#define PREDEFINED_H

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#define WINDOWS
#elif defined(linux) || defined(__linux) || defined(__linux__) || defined(__gnu_linux)
#define LINUX
#else
#error "Unsupported platform"
#endif


#endif //PREDEFINED_H
