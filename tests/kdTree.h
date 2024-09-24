#pragma once

#include "testFramework.h"

constexpr int MAX_DIM = 16;

template<typename T>
class Point {
   public:
    void print() {
        for (int i = 0; i < 3; i++) {
            std::cout << x[i] << " ";
        }
    };
    void print(int d) {
        for (int i = 0; i < d; i++) {
            std::cout << x[i] << " ";
        }
    }

   public:
    int id;
    T x[MAX_DIM];
};
