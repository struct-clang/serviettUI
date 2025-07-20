#pragma once
#include <string>
#include <functional>

void Text(const std::string& text);
void Button(const std::string& label, const std::function<void()>& cb);
void View(const std::function<void()>& viewFunc);
