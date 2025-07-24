#include "serviettUI.h"
#include <iostream>

int i = 0;
bool isOn = false;
bool imageVisible = true;
std::string myTextFieldText = "";

void AnotherView() {
    Text("Another View");
    Button("Click Me", []() {
        std::cout << "Button Clicked!" << std::endl;
    });
    Toggle("Show image below", imageVisible);
    if (imageVisible) {
        Image("Image.png", 256, 256);
    } else {
        Text("Image is hidden");
    }
}

void ContentView() {
    Text("Hello, World!");

    Button("Add +1 to number below", []() {
        i++;
    });
    Text(std::to_string(i));

    Toggle("Show text below", isOn);

    if (isOn) {
        Text("Some text");
    }

    TextField("Type something", myTextFieldText);
    Text(myTextFieldText);

    HStack([]() {
        Text("Leading");
        Text("Trailing");
    });

    Button("Open another View", []() {
        NewView(AnotherView);
    });
}

int main() {
    View(ContentView);
    return 0;
}
