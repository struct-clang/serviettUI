# Complete documentation for the serviettUI framework

> [!WARNING]
> First you need to install all dependencies: `clang, sdl2, sdl2_ttf, sdl2_gfx` and also clone this repository.

## Building library and project

To build the example program (from the `test/` directory) you should run make. Then go to the test directory and launch the binary by running `./test`.

The program should start. If it doesn't check the dependencies and try rebuilding the program.

If the program has started, you’re good to go.

## Framework components

### View

View is the main display container that can hold sub-elements like `Text()`, `Button()`, `Toggle()`, and others.

The program must contain at least one View to display content.

### Text

Text is a framework component that displays text content. It is initially centered within the View and shifts its position when other components are added.

To add a `Text()` element, insert `Text("Your text to display");` into the View. The data type must be a string. You can also use a variable instead of hardcoding the text.

### Button

Button is a framework component that displays text in color `#0066FF` and can trigger a function when pressed.

To add a `Button()` element, use the following syntax:

```
Button("Text in the button", []() {
  // Callback
});
```

### Toggle

Toggle is a framework component that can be turned on and off by the user or boolean variable. 

To add a `Toggle()` element, use the followin syntax:

```
bool isOn = false; //Also you can set true
Toggle("Text in the toggle", isOn);
```

### New View

`NewView()` is a framework component that can open new view.

To open new View, use the following syntax:

```
void MyView() {
    // Your content
}

void ContentView() {
    NewView(MyView);
}

int main() {
    View(ContentView);
    return 0;
}
```