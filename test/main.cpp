#include "serviettUI.h"
#include <iostream>

int i = 0;

void ContentView() {
    Text("Hello, World!");
    Text("Hello, World!");
    Text("Hello, World!");
    Text("Hello, World!");
    Button("Click Me", []() {
        std::cout << "Button Clicked!" << std::endl;
    });
    Button("Add +1 to number below", []() {
        i++;
    });
    Text(std::to_string(i));
}

int main() {
    View(ContentView);
    return 0;
}
