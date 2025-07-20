#include "serviettUI.h"
#include <iostream>

int i = 0;

void AnotherView() {
    Text("Another View");
    Button("Click Me", []() {
        std::cout << "Button Clicked!" << std::endl;
    });
}

void ContentView() {
    Text("Hello, World!");
    Text("Hello, World!");
    Text("Hello, World!");
    Text("Hello, World!");
    Button("Add +1 to number below", []() {
        i++;
    });
    Text(std::to_string(i));

    Button("Open another View", []() {
        NewView(AnotherView);
    });
}

int main() {
    View(ContentView);
    return 0;
}
